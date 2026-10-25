#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateDateTime();
    void refreshSerialPorts();
    void toggleSerialConnection();
    void readSerialData();
    void handleSerialError(QSerialPort::SerialPortError error);

    void onStartClicked();
    void onEmergencyStopClicked();
    void onResetCountersClicked();
    void onCalibrateClicked();
    void onTestAcceptClicked();
    void onTestRejectClicked();

    void onBrightnessChanged(int value);
    void onExposureChanged(int value);

private:
    void processSerialLine(const QString &line);
    void sendSerialCommand(const QString &command);
    void updateCountersUI();
    void triggerInspectionSimulation(bool isDefect);
    void renderVisionFeeds(bool isDefect);

    Ui::MainWindow *ui;
    QSerialPort *m_serialPort;
    QTimer *m_clockTimer;
    QTimer *m_sensorResetTimer;
    QByteArray m_serialBuffer;

    int m_acceptedCount;
    int m_rejectedCount;
    bool m_currentDefectState;
};

#endif // MAINWINDOW_H
