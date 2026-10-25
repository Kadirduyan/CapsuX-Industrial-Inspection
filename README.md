<div align="center">

# CapsuX — Endüstriyel Kapsül Kalite Kontrol ve Otomasyon Sistemi

**Gerçek Zamanlı Bilgisayarlı Görü (Edge AI) & Mikrodenetleyici Tabanlı Yüksek Hızlı Endüstriyel Kalite Kontrol ve Ayırma (Sorting) Platformu**

[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6.x-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io/)
[![OpenCV DNN](https://img.shields.io/badge/OpenCV-DNN-5C3EE8?style=for-the-badge&logo=opencv&logoColor=white)](https://opencv.org/)
[![YOLOv8](https://img.shields.io/badge/YOLOv8-Edge_AI-00FFFF?style=for-the-badge&logo=ultralytics&logoColor=black)](https://ultralytics.com/)
[![Arduino](https://img.shields.io/badge/Arduino-Firmware-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Embedded C](https://img.shields.io/badge/Embedded-C%2FC%2B%2B-A8B9CC?style=for-the-badge&logo=c&logoColor=black)](https://en.cppreference.com/)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?style=for-the-badge&logo=cmake&logoColor=white)](https://cmake.org/)

</div>

---

## 1. Proje Özeti ve Mühendislik Yaklaşımı

**CapsuX**, farmasötik ve endüstriyel üretim hatlarında hareket halindeki kapsüllerin yüzey kusurlarını (çapak, çatlak, kırık, deformasyon vb.) gerçek zamanlı olarak tespit etmek ve kusurlu ürünleri üretim hattından fiziksel olarak ayırmak amacıyla geliştirilmiş **düşük gecikmeli (low-latency)** bir kalite kontrol ve otomasyon sistemidir.

Proje, modern **Edge AI / Bilgisayarlı Görü** teknikleri ile **gömülü donanım otomasyonunu** deterministik bir iş akışı altında birleştirir:

- **Düşük Gecikmeli Algılama ve Karar Mekanizması:** Üretim bandından geçen her kapsül, optik sensör donanım kesmesi (Hardware Interrupt) ile milisaniyeler içinde yakalanır ve görüntü işleme / derin öğrenme çıkarım (inference) hattını tetikler.
- **Endüstriyel HMI (Human-Machine Interface):** Operatörlerin saha şartlarında göz yorgunluğu yaşamadan sistemi izleyebilmesi için koyu lacivert/siyah (`#0b1320` / `#142034`) endüstriyel QSS tasarım diline sahip **Qt6 C++** masaüstü kontrol paneli geliştirilmiştir.
- **Kapalı Çevrim (Closed-Loop) Donanım Senkronizasyonu:** Yazılım katmanında alınan kalite kontrol kararları (`KABUL` / `RED`), seri haberleşme protokolü üzerinden gerçek zamanlı olarak mikrodenetleyiciye iletilir ve servo tabanlı mekanik ayırıcıyı (diverter) harekete geçirir.

---

## 2. Sistem Mimarisi ve Haberleşme Protokolü

Sistem, **Gömülü Donanım Katmanı (Arduino / Embedded C)** ve **Endüstriyel Kontrol & Görü Katmanı (Qt6 C++)** olmak üzere iki ana bileşenin senkronize çalışması üzerine inşa edilmiştir:

```
┌────────────────────────────────────────────────────────────────────────────┐
│                   ENDÜSTRİYEL KONTROL PANELİ (Qt6 C++)                     │
│  ┌─────────────────────┐  ┌──────────────────────┐  ┌───────────────────┐  │
│  │ Live Vision Akışı   │  │ AI Isı Haritası      │  │ Üretim Sayaçları  │  │
│  │ (Kamera / ROI)      │  │ (Anomali & Skor)     │  │ (Kabul/Red/Hata%) │  │
│  └─────────────────────┘  └──────────────────────┘  └───────────────────┘  │
└──────────────────▲─────────────────────────────────────────┬───────────────┘
                   │ UART / Serial (9600 Baud, 8N1)          │
     "KAPSUL_GELDI"│ "KABUL" / "RED"                         │ Komutlar:
                   │                                         │ 'S','E','C','G','R'
┌──────────────────┴─────────────────────────────────────────▼───────────────┐
│                    GÖMÜLÜ DONANIM KATMANI (Arduino)                        │
│  ┌────────────────────────────────────┐   ┌─────────────────────────────┐  │
│  │ TCRT5000 Kızılötesi Optik Sensör   │   │ SG90 Servo Ayırma Mekanizması│  │
│  │ (Pin D2 - FALLING Edge Interrupt)  │   │ (Pin D9 - 0° Normal / 45° Red)│ │
│  └────────────────────────────────────┘   └─────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────────┘
```

### 2.1. Donanım Katmanı (Embedded Hardware & Actuation)
- **TCRT5000 IR Sensör & Donanım Kesmesi (Hardware Interrupt):** Kapsül geçişlerini kaçırmamak ve yoklama (polling) kaynaklı gecikmeleri ortadan kaldırmak için sensör dijital çıkışı `D2` (`INT0`) pinine bağlanmıştır. `attachInterrupt(digitalPinToInterrupt(sensorPin), kapsulGordu, FALLING)` yapılandırması sayesinde kapsül sensör önünden geçtiği anda düşen kenar (`FALLING edge`) kesmesi tetiklenir.
- **SG90 Servo Ayırma Mekanizması:** `D9` PWM pinine bağlı olan servo motor, sağlam kapsüllerde `0°` (doğrudan geçiş hattı) konumunda kalır. Hatalı kapsül tespit edildiğinde (`'R'` komutu), servo motor hızla `45°` açıya yönlenerek kusurlu kapsülü red haznesine ayırır ve `350 ms` sonra tekrar `0°` referans konumuna döner.

### 2.2. Yazılım Katmanı (Qt6 C++ HMI & Vision Pipeline)
- **Canlı Kamera Akışı (Live Vision) & Kamera Kontrolleri:** Kamera kaynağı seçimi, ROI (Region of Interest) sınır kutusu gösterimi ve anlık Parlaklık (`-100..+100`) / Pozlama (`-10..+10`) parametre kontrolü.
- **Yapay Zeka Isı Haritası (AI Heatmap Analysis):** Kapsül yüzeyindeki olası deformasyon bölgelerini sıcak-soğuk aktivasyon haritası (Grad-CAM / Anomali yoğunluğu) ve güven skoru (`%96.3` vb.) ile görselleştiren analiz paneli.
- **Gerçek Zamanlı Üretim Sayaçları:** Kabul edilen kapsül sayısı, reddedilen kapsül sayısı ve `((Reddedilen / Toplam) * 100)` formülüyle dinamik hesaplanan **Anlık Hata Oranı** kartları.
- **Seri Haberleşme Motoru (`QSerialPort` & `QSerialPortInfo`):** Aktif COM portlarını tarayan, `9600 Baud (8-N-1, No Flow Control)` standardında tam çift yönlü (full-duplex) satır tabanlı veri okuma ve komut gönderme altyapısı.

### 2.3. Seri Port Haberleşme Protokolü Tablosu

| Komut / Mesaj | Yön | Fonksiyon / Anlamı | Sistem & Donanım Tepkisi |
| :---: | :---: | :--- | :--- |
| **`'S'`** | `PC ➔ Arduino` | **Sistemi Başlat (Start)** | Üretim döngüsünü aktif eder (`sistemAktif = true`), durumu `"Sistem Aktif"` yapar. |
| **`'E'`** | `PC ➔ Arduino` | **Acil Durdurma (Emergency Stop)** | Sistemi güvenli moda alır (`sistemAktif = false`), servoyu `0°` konumuna çeker. |
| **`'C'`** | `PC ➔ Arduino` | **Kalibrasyon (Calibrate)** | Ayırıcı servo motoru `0°` referans başlangıç pozisyonuna kalibre eder. |
| **`'G'`** | `PC ➔ Arduino` | **Kabul / Sağlam (Green)** | Servoyu `0°` geçiş açısında tutar, Arduino `"KABUL"` yanıtı döner ve kabul sayacı artar. |
| **`'R'`** | `PC ➔ Arduino` | **Red / Hatalı (Reject - 45°)** | Servoyu `45°` ayırma açısına yönlendirir (`350 ms`), `"RED"` yanıtı döner ve red sayacı artar. |
| **`KAPSUL_GELDI`** | `Arduino ➔ PC` | **Sensör Tetikleme (Trigger)** | TCRT5000 kesmesi oluştuğunda gönderilir; arayüzde `"Tetiklendi!"` durumunu ve AI analizini başlatır. |
| **`KABUL` / `RED`** | `Arduino ➔ PC` | **Ayırma Onay Geri Bildirimi** | İlgili üretim sayacını (`+1`) ve **Anlık Hata Oranı (%)** metriğini otomatik günceller. |

---

## 3. Klasör Mimarisi (Directory Tree)

```text
CapsuX/
├── CMakeLists.txt                  # Qt6 (Widgets, SerialPort, LinguistTools) CMake yapılandırması
├── main.cpp                        # Uygulama giriş noktası ve çeviri (i18n) yükleyicisi
├── mainwindow.h                    # Ana kontrol paneli sınıfı, seri port ve simülasyon bildirimleri
├── mainwindow.cpp                  # Seri haberleşme protokolü, AI ısı haritası ve sayaç iş mantığı
├── mainwindow.ui                   # 1280x720 Dark Modern Industrial (QSS) arayüz tasarımı
├── CapsuX_tr_TR.ts                 # Türkçe dil ve yerelleştirme dosyası
├── otomatik_guncelle.bat           # Tek tıkla otomatik Git commit & push otomasyon betiği
├── .gitignore                      # Derleme (build/) ve IDE (.qtcreator/) filtre kuralları
└── Arduino_Firmware/
    └── Arduino_Firmware.ino        # TCRT5000 Interrupt ve SG90 Servo kontrol gömülü yazılımı
```

---

## 4. Kurulum ve Çalıştırma

### 4.1. Ön Gereksinimler
- **Qt 6.x** (`Qt6::Widgets` ve `Qt6::SerialPort` modülleri kurulu olmalıdır)
- **CMake** `3.16` veya üzeri
- **C++17** uyumlu derleyici (MinGW 64-bit, MSVC veya GCC/Clang)
- **Arduino IDE** (Gömülü yazılımı mikrodenetleyiciye yüklemek için)

### 4.2. Qt6 C++ Masaüstü Uygulamasının Derlenmesi

#### Komut Satırı (CMake) ile Derleme:
```bash
# 1. Depoyu klonlayın
git clone https://github.com/Kadirduyan/CapsuX-Industrial-Inspection.git
cd CapsuX-Industrial-Inspection

# 2. Derleme dizinini oluşturun ve yapılandırın
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Projeyi derleyin
cmake --build build --config Release
```

#### Qt Creator ile Çalıştırma:
1. **Qt Creator** uygulamasını açın ve `CMakeLists.txt` dosyasını proje olarak seçin.
2. Masaüstü Qt6 kitinizi (ör. *Desktop Qt 6.x MinGW 64-bit*) seçip **Configure Project** butonuna tıklayın.
3. `Ctrl + R` kısayolu ile projeyi derleyip başlatın.

### 4.3. Arduino Firmware Yükleme Talimatları
1. `Arduino_Firmware/Arduino_Firmware.ino` dosyasını **Arduino IDE** ile açın.
2. Donanım bağlantılarını gerçekleştirin:
   - **TCRT5000 Sensör `D0` Pini** ➔ Arduino **`Pin 2`** (`VCC` ➔ `5V`, `GND` ➔ `GND`)
   - **SG90 Servo Sinyal Pini** ➔ Arduino **`Pin 9`** (`VCC` ➔ `5V`, `GND` ➔ `GND`)
3. Kart modelinizi (Arduino Uno / Nano) ve bağlı olduğu COM portunu seçerek kodu karta yükleyin (**Upload**).
4. Arduino IDE Seri Port Ekranını kapatın (portun meşgul olmaması için) ve **CapsuX** arayüzündeki **Donanım ve Haberleşme Durumu** panelinden ilgili COM portunu seçip **Bağlan** butonuna tıklayın.

---

## 5. Geliştirici & Mühendislik Profili (Author)

**Kadir Duyan**  
*Sakarya Üniversitesi — Bilgisayar Mühendisliği (4. Sınıf)*

Bu projenin uçtan uca sistem mimarisi; **EPROM Elektronik San. Tic. A.Ş.** bünyesinde gerçekleştirilen **Edge AI / Bilgisayarlı Görü (Qt6, GStreamer, YOLOv8)** ve **Gömülü Sistemler (STM32, Donanım-Yazılım Entegrasyonu)** mühendislik stajlarında edinilen endüstriyel saha deneyimlerinin doğrudan bir yansıması olarak tasarlanmıştır. Yüksek hızlı görüntü işleme hatlarının deterministik mikrodenetleyici kesmeleriyle (Hardware Interrupts) senkronize edilmesi ve endüstriyel standartlarda operatör arayüzü (HMI) geliştirilmesi süreçlerinde bu mühendislik birikimi temel alınmıştır.

### İletişim ve Bağlantılar
- **GitHub:** [github.com/Kadirduyan](https://github.com/Kadirduyan)
- **LinkedIn:** [linkedin.com/in/kadir-duyan4](https://www.linkedin.com/in/kadir-duyan4)
- **E-posta:** [kadirduyan482@gmail.com](mailto:kadirduyan482@gmail.com)
