#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QRandomGenerator>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , cap()
    , kameraTimer(new QTimer(this))
    , m_serialPort(new QSerialPort(this))
    , m_clockTimer(new QTimer(this))
    , m_sensorResetTimer(new QTimer(this))
    , m_acceptedCount(0)
    , m_rejectedCount(0)
    , m_currentDefectState(true)
{
    ui->setupUi(this);

    // Kamera seçim kutusunu (ComboBox) indekslerle yapılandır
    ui->cmbCamera->blockSignals(true);
    ui->cmbCamera->clear();
    ui->cmbCamera->addItem(QStringLiteral("Harici USB Kamera (Kamera 1)"), 1);
    ui->cmbCamera->addItem(QStringLiteral("Dahili Kamera (Kamera 0)"), 0);
    ui->cmbCamera->addItem(QStringLiteral("Harici Kamera 2 (Kamera 2)"), 2);
    ui->cmbCamera->addItem(QStringLiteral("Simülasyon Akışı"), -1);
    ui->cmbCamera->blockSignals(false);

    connect(kameraTimer, &QTimer::timeout, this, &MainWindow::kameraKaresiniGuncelle);
    connect(ui->cmbCamera, &QComboBox::currentIndexChanged, this, &MainWindow::onCameraSelectionChanged);

    // Öncelikle harici endüstriyel USB kamerayı (İndeks 1) açmayı dene, bulunamazsa İndeks 0'a geç
    if (kamerayiAc(1)) {
        ui->cmbCamera->blockSignals(true);
        ui->cmbCamera->setCurrentIndex(0); // Harici USB Kamera (Kamera 1)
        ui->cmbCamera->blockSignals(false);
    } else if (kamerayiAc(0)) {
        ui->cmbCamera->blockSignals(true);
        ui->cmbCamera->setCurrentIndex(1); // Dahili Kamera (Kamera 0)
        ui->cmbCamera->blockSignals(false);
    } else {
        ui->cmbCamera->blockSignals(true);
        ui->cmbCamera->setCurrentIndex(3); // Simülasyon Akışı
        ui->cmbCamera->blockSignals(false);
    }

    // Canlı Tarih / Saat zamanlayıcısı (1 saniyede bir güncellenir)
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateDateTime);
    m_clockTimer->start(1000);
    updateDateTime();

    // Sensör durumu sıfırlama zamanlayıcısı
    m_sensorResetTimer->setSingleShot(true);
    connect(m_sensorResetTimer, &QTimer::timeout, this, [this]() {
        ui->lblSensorStatus->setText(QStringLiteral("Bekleniyor..."));
        ui->lblSensorStatus->setStyleSheet(QStringLiteral("color: #fbbf24; font-size: 12px; font-weight: bold;"));
    });

    // Seri port sinyalleri
    connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::readSerialData);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &MainWindow::handleSerialError);

    // Donanım ve Haberleşme buton bağlantıları
    connect(ui->btnRefreshPorts, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    connect(ui->btnConnectSerial, &QPushButton::clicked, this, &MainWindow::toggleSerialConnection);

    // Operatör Kontrolleri buton bağlantıları
    connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->btnEmergencyStop, &QPushButton::clicked, this, &MainWindow::onEmergencyStopClicked);
    connect(ui->btnResetCounters, &QPushButton::clicked, this, &MainWindow::onResetCountersClicked);
    connect(ui->btnCalibrate, &QPushButton::clicked, this, &MainWindow::onCalibrateClicked);
    connect(ui->btnTestAccept, &QPushButton::clicked, this, &MainWindow::onTestAcceptClicked);
    connect(ui->btnTestReject, &QPushButton::clicked, this, &MainWindow::onTestRejectClicked);

    // Kamera Ayarları slider bağlantıları
    connect(ui->sliderBrightness, &QSlider::valueChanged, this, &MainWindow::onBrightnessChanged);
    connect(ui->sliderExposure, &QSlider::valueChanged, this, &MainWindow::onExposureChanged);

    // Başlangıç durumlarını hazırla
    refreshSerialPorts();
    updateCountersUI();

    // İlk açılışta görsel akışları hazırla
    QTimer::singleShot(50, this, [this]() {
        renderVisionFeeds(m_currentDefectState);
    });
}

MainWindow::~MainWindow()
{
    if (kameraTimer->isActive()) {
        kameraTimer->stop();
    }
    if (cap.isOpened()) {
        cap.release();
    }
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    delete ui;
}

bool MainWindow::kamerayiAc(int cameraIndex)
{
    if (kameraTimer->isActive()) {
        kameraTimer->stop();
    }
    if (cap.isOpened()) {
        cap.release();
    }

    // Simülasyon modu seçildiyse fiziksel kamerayı kapat
    if (cameraIndex < 0) {
        ui->lblCameraHwStatus->setText(QStringLiteral("Simülasyon Modu"));
        ui->lblCameraHwStatus->setStyleSheet(QStringLiteral("color: #38bdf8; font-size: 12px; font-weight: bold;"));
        ui->lblCameraStatus->setText(QStringLiteral("● SIM | 1920 x 1080 | Yapay Zeka Simülasyon Akışı"));
        renderVisionFeeds(m_currentDefectState);
        return false;
    }

    // Windows'ta harici USB kameraların hızlı ve gecikmesiz açılması için önce DirectShow (CAP_DSHOW) dene
    bool opened = cap.open(cameraIndex, cv::CAP_DSHOW);
    if (!opened) {
        opened = cap.open(cameraIndex, cv::CAP_ANY);
    }

    if (opened && cap.isOpened()) {
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
        cap.set(cv::CAP_PROP_FPS, 30);

        ui->lblCameraHwStatus->setText(QStringLiteral("Bağlı (Kamera #%1)").arg(cameraIndex));
        ui->lblCameraHwStatus->setStyleSheet(QStringLiteral("color: #4ade80; font-size: 12px; font-weight: bold;"));
        ui->lblCameraStatus->setText(QStringLiteral("● LIVE | Kamera #%1 | 1280 x 720 | 30 FPS").arg(cameraIndex));
        kameraTimer->start(30);
        return true;
    }

    ui->lblCameraHwStatus->setText(QStringLiteral("Kamera #%1 Bulunamadı").arg(cameraIndex));
    ui->lblCameraHwStatus->setStyleSheet(QStringLiteral("color: #fbbf24; font-size: 12px; font-weight: bold;"));
    ui->lblCameraStatus->setText(QStringLiteral("● UYARI | Kamera #%1 Açılamadı (Simülasyon Aktif)").arg(cameraIndex));
    renderVisionFeeds(m_currentDefectState);
    return false;
}

void MainWindow::onCameraSelectionChanged(int comboIndex)
{
    if (comboIndex < 0) {
        return;
    }
    const int cameraIndex = ui->cmbCamera->itemData(comboIndex).toInt();
    kamerayiAc(cameraIndex);
}

void MainWindow::kameraKaresiniGuncelle()
{
    if (!cap.isOpened()) {
        return;
    }

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        // Hata toleransı (Fail-safe): Kamera bağlantısı aniden koparsa çökme, uyarı ver ve simülasyona geç
        kameraTimer->stop();
        cap.release();
        ui->lblCameraHwStatus->setText(QStringLiteral("Bağlantı Koptu! (Fail-Safe)"));
        ui->lblCameraHwStatus->setStyleSheet(QStringLiteral("color: #f87171; font-size: 12px; font-weight: bold;"));
        renderVisionFeeds(m_currentDefectState);
        return;
    }

    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);

    const QImage qimg(rgbFrame.data,
                      rgbFrame.cols,
                      rgbFrame.rows,
                      static_cast<int>(rgbFrame.step.p[0]),
                      QImage::Format_RGB888);

    ui->lblCameraFeed->setPixmap(
        QPixmap::fromImage(qimg).scaled(
            ui->lblCameraFeed->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    renderVisionFeeds(m_currentDefectState);
}

void MainWindow::updateDateTime()
{
    const QString nowStr = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy | HH:mm:ss"));
    ui->lblDateTime->setText(nowStr);
}

void MainWindow::refreshSerialPorts()
{
    const QString currentSelection = ui->cmbComPort->currentText();
    ui->cmbComPort->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString label = info.portName();
        if (!info.description().isEmpty()) {
            label += QStringLiteral(" (%1)").arg(info.description());
        }
        ui->cmbComPort->addItem(label, info.portName());
    }

    if (ui->cmbComPort->count() == 0) {
        ui->cmbComPort->addItem(QStringLiteral("Port Bulunamadı"), QString());
    } else {
        int idx = ui->cmbComPort->findText(currentSelection);
        if (idx >= 0) {
            ui->cmbComPort->setCurrentIndex(idx);
        }
    }
}

void MainWindow::toggleSerialConnection()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        ui->btnConnectSerial->setText(QStringLiteral("Bağlan"));
        ui->btnConnectSerial->setStyleSheet(QStringLiteral(
            "background-color: #0284c7; border: 1px solid #38bdf8; color: #ffffff;"));
        ui->lblArduinoStatus->setText(QStringLiteral("Bağlı Değil"));
        ui->lblArduinoStatus->setStyleSheet(QStringLiteral("color: #f87171; font-size: 12px; font-weight: bold;"));
        ui->cmbComPort->setEnabled(true);
        ui->btnRefreshPorts->setEnabled(true);
        return;
    }

    QString portName = ui->cmbComPort->currentData().toString();
    if (portName.isEmpty()) {
        const QString rawText = ui->cmbComPort->currentText().trimmed();
        if (!rawText.isEmpty() && rawText != QStringLiteral("Port Bulunamadı")) {
            portName = rawText.section(QLatin1Char(' '), 0, 0);
        }
    }

    if (portName.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Seri Port Uyarısı"),
                             QStringLiteral("Bağlanılacak geçerli bir COM port bulunamadı.\nLütfen Arduino bağlantısını kontrol edip 'Yenile' butonuna basın."));
        return;
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(QSerialPort::Baud9600);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        m_serialBuffer.clear();
        ui->btnConnectSerial->setText(QStringLiteral("Bağlantıyı Kes"));
        ui->btnConnectSerial->setStyleSheet(QStringLiteral(
            "background-color: #dc2626; border: 1px solid #ef4444; color: #ffffff;"));
        ui->lblArduinoStatus->setText(QStringLiteral("Bağlı (%1)").arg(portName));
        ui->lblArduinoStatus->setStyleSheet(QStringLiteral("color: #4ade80; font-size: 12px; font-weight: bold;"));
        ui->cmbComPort->setEnabled(false);
        ui->btnRefreshPorts->setEnabled(false);
    } else {
        ui->lblArduinoStatus->setText(QStringLiteral("Bağlı Değil"));
        ui->lblArduinoStatus->setStyleSheet(QStringLiteral("color: #f87171; font-size: 12px; font-weight: bold;"));
        QMessageBox::critical(this,
                              QStringLiteral("Bağlantı Hatası"),
                              QStringLiteral("%1 portuna bağlanılamadı:\n%2").arg(portName, m_serialPort->errorString()));
    }
}

void MainWindow::readSerialData()
{
    m_serialBuffer.append(m_serialPort->readAll());

    while (m_serialBuffer.contains('\n')) {
        const int newlineIndex = m_serialBuffer.indexOf('\n');
        const QByteArray rawLine = m_serialBuffer.left(newlineIndex);
        m_serialBuffer.remove(0, newlineIndex + 1);

        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (!line.isEmpty()) {
            processSerialLine(line);
        }
    }
}

void MainWindow::processSerialLine(const QString &line)
{
    const QString upper = line.toUpper();

    if (upper.contains(QStringLiteral("KAPSUL_GELDI")) || upper.contains(QStringLiteral("TRIGGER"))) {
        ui->lblSensorStatus->setText(QStringLiteral("Tetiklendi!"));
        ui->lblSensorStatus->setStyleSheet(QStringLiteral("color: #4ade80; font-size: 12px; font-weight: bold;"));
        m_sensorResetTimer->start(1800);

        const int simLatency = QRandomGenerator::global()->bounded(9, 16);
        ui->lblInferenceSpeed->setText(QStringLiteral("%1 ms").arg(simLatency));
        triggerInspectionSimulation(m_currentDefectState);
    }

    if (upper == QStringLiteral("KABUL") || upper == QStringLiteral("GREEN")
        || upper.contains(QStringLiteral("KABUL")) || upper.contains(QStringLiteral("GREEN"))) {
        m_acceptedCount++;
        updateCountersUI();
        triggerInspectionSimulation(false);
    } else if (upper == QStringLiteral("RED") || upper.contains(QStringLiteral("RED"))) {
        m_rejectedCount++;
        updateCountersUI();
        triggerInspectionSimulation(true);
    }
}

void MainWindow::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError || error == QSerialPort::PermissionError) {
        if (m_serialPort->isOpen()) {
            m_serialPort->close();
        }
        ui->btnConnectSerial->setText(QStringLiteral("Bağlan"));
        ui->btnConnectSerial->setStyleSheet(QStringLiteral(
            "background-color: #0284c7; border: 1px solid #38bdf8; color: #ffffff;"));
        ui->lblArduinoStatus->setText(QStringLiteral("Bağlı Değil"));
        ui->lblArduinoStatus->setStyleSheet(QStringLiteral("color: #f87171; font-size: 12px; font-weight: bold;"));
        ui->cmbComPort->setEnabled(true);
        ui->btnRefreshPorts->setEnabled(true);
    }
}

void MainWindow::sendSerialCommand(const QString &command)
{
    if (m_serialPort->isOpen() && m_serialPort->isWritable()) {
        QByteArray payload = command.toUtf8();
        if (!payload.endsWith('\n')) {
            payload.append('\n');
        }
        m_serialPort->write(payload);
        m_serialPort->flush();
    }
}

void MainWindow::updateCountersUI()
{
    ui->lblAcceptedCount->setText(QString::number(m_acceptedCount));
    ui->lblRejectedCount->setText(QString::number(m_rejectedCount));

    const int total = m_acceptedCount + m_rejectedCount;
    double errorRate = 0.0;
    if (total > 0) {
        errorRate = (static_cast<double>(m_rejectedCount) / static_cast<double>(total)) * 100.0;
    }

    ui->lblErrorRate->setText(QStringLiteral("%%1").arg(QString::number(errorRate, 'f', 1)));
}

void MainWindow::onStartClicked()
{
    sendSerialCommand(QStringLiteral("S"));
    if (cap.isOpened() && !kameraTimer->isActive()) {
        kameraTimer->start(30);
    }
    ui->lblSystemStatus->setText(QStringLiteral("● Sistem Aktif"));
    ui->lblSystemStatus->setStyleSheet(QStringLiteral(
        "color: #22c55e; background-color: rgba(34, 197, 94, 0.12); "
        "border: 1px solid rgba(34, 197, 94, 0.4); border-radius: 12px; "
        "padding: 4px 12px; font-size: 12px; font-weight: bold;"));
    ui->lblMotorStatus->setText(QStringLiteral("Çalışıyor"));
    ui->lblMotorStatus->setStyleSheet(QStringLiteral("color: #4ade80; font-size: 12px; font-weight: bold;"));
}

void MainWindow::onEmergencyStopClicked()
{
    sendSerialCommand(QStringLiteral("E"));
    ui->lblSystemStatus->setText(QStringLiteral("● Acil Durduruldu"));
    ui->lblSystemStatus->setStyleSheet(QStringLiteral(
        "color: #ef4444; background-color: rgba(239, 68, 68, 0.15); "
        "border: 1px solid rgba(239, 68, 68, 0.5); border-radius: 12px; "
        "padding: 4px 12px; font-size: 12px; font-weight: bold;"));
    ui->lblMotorStatus->setText(QStringLiteral("Durduruldu (ACİL)"));
    ui->lblMotorStatus->setStyleSheet(QStringLiteral("color: #f87171; font-size: 12px; font-weight: bold;"));
}

void MainWindow::onResetCountersClicked()
{
    m_acceptedCount = 0;
    m_rejectedCount = 0;
    updateCountersUI();
}

void MainWindow::onCalibrateClicked()
{
    sendSerialCommand(QStringLiteral("C"));
    ui->lblMotorStatus->setText(QStringLiteral("Kalibre Edildi (Hazır)"));
    ui->lblMotorStatus->setStyleSheet(QStringLiteral("color: #38bdf8; font-size: 12px; font-weight: bold;"));
}

void MainWindow::onTestAcceptClicked()
{
    sendSerialCommand(QStringLiteral("G"));
    ui->lblSensorStatus->setText(QStringLiteral("Tetiklendi! (Kabul Test)"));
    ui->lblSensorStatus->setStyleSheet(QStringLiteral("color: #4ade80; font-size: 12px; font-weight: bold;"));
    m_sensorResetTimer->start(1800);

    if (!m_serialPort->isOpen()) {
        m_acceptedCount++;
        updateCountersUI();
    }
    triggerInspectionSimulation(false);
}

void MainWindow::onTestRejectClicked()
{
    sendSerialCommand(QStringLiteral("R"));
    ui->lblSensorStatus->setText(QStringLiteral("Tetiklendi! (Red Test)"));
    ui->lblSensorStatus->setStyleSheet(QStringLiteral("color: #f87171; font-size: 12px; font-weight: bold;"));
    m_sensorResetTimer->start(1800);

    if (!m_serialPort->isOpen()) {
        m_rejectedCount++;
        updateCountersUI();
    }
    triggerInspectionSimulation(true);
}

void MainWindow::onBrightnessChanged(int value)
{
    ui->lblBrightnessValue->setText(QString::number(value));
    if (cap.isOpened()) {
        cap.set(cv::CAP_PROP_BRIGHTNESS, value);
    } else {
        renderVisionFeeds(m_currentDefectState);
    }
}

void MainWindow::onExposureChanged(int value)
{
    ui->lblExposureValue->setText(QString::number(value));
    if (cap.isOpened()) {
        cap.set(cv::CAP_PROP_EXPOSURE, value);
    } else {
        renderVisionFeeds(m_currentDefectState);
    }
}

void MainWindow::triggerInspectionSimulation(bool isDefect)
{
    m_currentDefectState = isDefect;

    if (isDefect) {
        ui->frameAnomalyAlert->setStyleSheet(QStringLiteral(
            "QFrame#frameAnomalyAlert { background-color: rgba(239, 68, 68, 0.12); "
            "border: 1px solid rgba(239, 68, 68, 0.45); border-radius: 6px; }"));
        ui->lblAnomalyTitle->setText(QStringLiteral("⚠ Anomali Tespit Edildi"));
        ui->lblAnomalyTitle->setStyleSheet(QStringLiteral("color: #f87171; font-size: 13px; font-weight: bold;"));
        ui->lblAnomalyType->setText(QStringLiteral("Tür: Çapak / Kırık (Tahmini)"));
        ui->lblAnomalyConfidence->setText(QStringLiteral("Güven Skoru: %96.3"));
        ui->lblAnomalyConfidence->setStyleSheet(QStringLiteral("color: #fca5a5; font-size: 12px; font-weight: bold;"));
    } else {
        ui->frameAnomalyAlert->setStyleSheet(QStringLiteral(
            "QFrame#frameAnomalyAlert { background-color: rgba(34, 197, 94, 0.12); "
            "border: 1px solid rgba(34, 197, 94, 0.45); border-radius: 6px; }"));
        ui->lblAnomalyTitle->setText(QStringLiteral("✔ Kapsül Sağlam (Normal)"));
        ui->lblAnomalyTitle->setStyleSheet(QStringLiteral("color: #4ade80; font-size: 13px; font-weight: bold;"));
        ui->lblAnomalyType->setText(QStringLiteral("Tür: Kusursuz Yüzey"));
        ui->lblAnomalyConfidence->setText(QStringLiteral("Güven Skoru: %99.1"));
        ui->lblAnomalyConfidence->setStyleSheet(QStringLiteral("color: #86efac; font-size: 12px; font-weight: bold;"));
    }

    renderVisionFeeds(isDefect);
}

void MainWindow::renderVisionFeeds(bool isDefect)
{
    const int brightnessOffset = ui->sliderBrightness ? (ui->sliderBrightness->value() - 50) / 3 : 0;

    // 1. Ana Kamera Akışı: Fiziksel kamera açık değilse simülasyon görüntüsünü çiz
    const QSize camSize = ui->lblCameraFeed->size();
    if (!cap.isOpened() && camSize.width() > 20 && camSize.height() > 20) {
        QPixmap camPix(camSize);
        camPix.fill(QColor(7, 12, 21));

        QPainter p(&camPix);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.setPen(QPen(QColor(30, 50, 80, 90), 1, Qt::DotLine));
        const int step = 32;
        for (int x = step; x < camSize.width(); x += step) {
            p.drawLine(x, 0, x, camSize.height());
        }
        for (int y = step; y < camSize.height(); y += step) {
            p.drawLine(0, y, camSize.width(), y);
        }

        const int cx = camSize.width() / 2;
        const int cy = camSize.height() / 2;
        const int capW = qMin(camSize.width() * 55 / 100, 240);
        const int capH = qMin(camSize.height() * 36 / 100, 96);
        const QRect capRect(cx - capW / 2, cy - capH / 2, capW, capH);

        QPainterPath capsulePath;
        capsulePath.addRoundedRect(capRect, capH / 2.0, capH / 2.0);
        p.save();
        p.setClipPath(capsulePath);

        QLinearGradient leftGrad(capRect.left(), capRect.top(), capRect.left(), capRect.bottom());
        leftGrad.setColorAt(0.0, QColor(qBound(0, 56 + brightnessOffset, 255),
                                        qBound(0, 189 + brightnessOffset, 255),
                                        qBound(0, 248 + brightnessOffset, 255)));
        leftGrad.setColorAt(0.5, QColor(qBound(0, 2 + brightnessOffset, 255),
                                        qBound(0, 132 + brightnessOffset, 255),
                                        qBound(0, 199 + brightnessOffset, 255)));
        leftGrad.setColorAt(1.0, QColor(12, 74, 110));
        p.fillRect(QRect(capRect.left(), capRect.top(), capW / 2, capH), leftGrad);

        QLinearGradient rightGrad(capRect.left(), capRect.top(), capRect.left(), capRect.bottom());
        rightGrad.setColorAt(0.0, QColor(qBound(0, 248 + brightnessOffset, 255),
                                         qBound(0, 250 + brightnessOffset, 255),
                                         qBound(0, 252 + brightnessOffset, 255)));
        rightGrad.setColorAt(0.5, QColor(qBound(0, 203 + brightnessOffset, 255),
                                         qBound(0, 213 + brightnessOffset, 255),
                                         qBound(0, 225 + brightnessOffset, 255)));
        rightGrad.setColorAt(1.0, QColor(100, 116, 139));
        p.fillRect(QRect(cx, capRect.top(), capW / 2, capH), rightGrad);

        p.setPen(QPen(QColor(15, 23, 42, 180), 2));
        p.drawLine(cx, capRect.top(), cx, capRect.bottom());

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 70));
        p.drawRoundedRect(capRect.left() + 18, capRect.top() + 10, capW - 36, capH / 5, 6, 6);

        if (isDefect) {
            const int defectX = cx + capW / 6;
            const int defectY = cy - capH / 8;
            p.setPen(QPen(QColor(220, 38, 38), 2.5));
            p.drawLine(defectX - 12, defectY - 8, defectX + 4, defectY + 6);
            p.drawLine(defectX + 4, defectY + 6, defectX + 14, defectY - 3);
            p.setBrush(QColor(239, 68, 68, 90));
            p.drawEllipse(QPoint(defectX, defectY), 16, 12);
        }
        p.restore();

        p.setPen(QPen(QColor(148, 163, 184, 160), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(capRect, capH / 2.0, capH / 2.0);

        const QRect roiBox = capRect.adjusted(-14, -14, 14, 14);
        const QColor boxColor = isDefect ? QColor(239, 68, 68) : QColor(34, 197, 94);
        p.setPen(QPen(boxColor, 2, Qt::DashLine));
        p.drawRect(roiBox);

        p.setPen(boxColor);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::Bold));
        const QString tagText = isDefect
                                    ? QStringLiteral("DEFECT: CAP_CRACK (96.3%)")
                                    : QStringLiteral("PASS: CAPSULE_OK (99.1%)");
        p.drawText(roiBox.left(), roiBox.top() - 6, tagText);

        p.end();
        ui->lblCameraFeed->setPixmap(camPix);
    }

    // 2. Yapay Zeka Isı Haritası (AI Analysis Heatmap) Görseli
    const QSize heatSize = ui->lblHeatmapFeed->size();
    if (heatSize.width() > 20 && heatSize.height() > 20) {
        QPixmap heatPix(heatSize);
        heatPix.fill(QColor(8, 15, 32));

        QPainter hp(&heatPix);
        hp.setRenderHint(QPainter::Antialiasing, true);

        const int cx = heatSize.width() / 2;
        const int cy = heatSize.height() / 2;
        const int capW = qMin(heatSize.width() * 64 / 100, 210);
        const int capH = qMin(heatSize.height() * 40 / 100, 84);
        const QRect capRect(cx - capW / 2, cy - capH / 2, capW, capH);

        QRadialGradient bgGrad(QPointF(cx, cy), qMax(heatSize.width(), heatSize.height()) * 0.6);
        bgGrad.setColorAt(0.0, QColor(29, 78, 216, 110));
        bgGrad.setColorAt(0.6, QColor(30, 58, 138, 70));
        bgGrad.setColorAt(1.0, QColor(7, 12, 21, 255));
        hp.fillRect(heatPix.rect(), bgGrad);

        QPainterPath capsulePath;
        capsulePath.addRoundedRect(capRect, capH / 2.0, capH / 2.0);
        hp.save();
        hp.setClipPath(capsulePath);

        QLinearGradient baseHeat(capRect.left(), capRect.top(), capRect.right(), capRect.bottom());
        baseHeat.setColorAt(0.0, QColor(37, 99, 235));
        baseHeat.setColorAt(0.5, QColor(16, 185, 129));
        baseHeat.setColorAt(1.0, QColor(29, 78, 216));
        hp.fillRect(capRect, baseHeat);

        if (isDefect) {
            const QPointF hotSpot(cx + capW / 6.0, cy - capH / 10.0);
            QRadialGradient hotGrad(hotSpot, capH * 0.65);
            hotGrad.setColorAt(0.0, QColor(255, 255, 255));
            hotGrad.setColorAt(0.22, QColor(239, 68, 68));
            hotGrad.setColorAt(0.55, QColor(245, 158, 11));
            hotGrad.setColorAt(0.85, QColor(16, 185, 129, 140));
            hotGrad.setColorAt(1.0, QColor(29, 78, 216, 0));
            hp.fillRect(capRect, hotGrad);
        }
        hp.restore();

        hp.setPen(QPen(QColor(125, 211, 252, 150), 1.5));
        hp.setBrush(Qt::NoBrush);
        hp.drawRoundedRect(capRect, capH / 2.0, capH / 2.0);

        if (isDefect) {
            const QPoint hotCenter(cx + capW / 6, cy - capH / 10);
            hp.setPen(QPen(QColor(248, 113, 113), 2, Qt::DotLine));
            hp.drawEllipse(hotCenter, 24, 24);
        }

        hp.end();
        ui->lblHeatmapFeed->setPixmap(heatPix);
    }
}
