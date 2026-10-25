# CapsuX — Proje Geliştirme Raporu (Yapılanlar & Yapılacaklar Yol Haritası)

**Proje Adı:** CAPSULE INSPECTION SYSTEM — Yapay Zeka Destekli Kapsül Kalite Kontrolü  
**Geliştirici:** Kadir Duyan  
**Son Güncelleme:** 25.10.2026  

---

## ✅ 1. YAPILANLAR (Tamamlanan Mühendislik Adımları)

### Aşama 1: Gömülü Sistem ve Donanım Otomasyonu (`Arduino_Firmware`)
- [x] **TCRT5000 Kızılötesi Sensör Donanım Kesmesi (Hardware Interrupt):** Sensör dijital çıkışı `Pin 2` (`INT0`) üzerine bağlanarak `FALLING` (düşen kenar) kesmesiyle kapsül geçişlerinin sıfır gecikmeyle yakalanması sağlandı (`KAPSUL_GELDI` sinyali).
- [x] **SG90 Servo Ayırma Mekanizması:** `Pin 9` PWM hattı üzerinden sağlam kapsüllerde `0°` referans konumu, hatalı kapsüllerde `45°` ayırma açısına yönlendirme (`350 ms` tepki süresi) ve otomatik geri dönüş mantığı kodlandı.
- [x] **Çift Yönlü Seri Haberleşme Protokolü (9600 Baud, 8N1):** PC'den gelen `'S'` (Başlat), `'E'` (Acil Durdurma), `'C'` (Kalibrasyon), `'G'` (Kabul/Green), `'R'` (Red/45°) komutlarını işleyen ve durum geri bildirimlerini (`KABUL`, `RED`, `KAPSUL_GELDI`) döndüren gömülü yazılım tamamlandı.

### Aşama 2: Endüstriyel Kontrol Paneli Arayüzü (`Qt6 C++ & QSS`)
- [x] **Dark Modern Industrial (QSS) Tasarım:** `1280x720` (tam ekran uyumlu) koyu lacivert/siyah (`#0b1320` arka plan, `#142034` paneller, `#1e3250` çerçeveler) endüstriyel operatör arayüzü (`mainwindow.ui`) oluşturuldu.
- [x] **Üst Bilgi Barı (Header):** Sistem aktiflik durumu göstergesi ve `QTimer` ile her saniye güncellenen canlı Tarih/Saat modülü eklendi.
- [x] **Gerçek Zamanlı Üretim Sayaçları:** Kabul Edilen, Reddedilen ve `((Reddedilen / Toplam) * 100)` formülüyle otomatik hesaplanan **Anlık Hata Oranı (%)** bilgi kartları entegre edildi.
- [x] **Donanım ve Haberleşme Durumu Paneli:** `QSerialPortInfo` ile aktif COM portlarını listeleme, Yenile/Bağlan/Bağlantıyı Kes kontrolleri ve Arduino, Motor, Çıkarım Hızı, Sensör, Kamera durum göstergeleri eklendi.
- [x] **Operatör Kontrolleri & Manuel Servo Testi:** `▶ Başlat`, `■ Acil Durdurma`, `↻ Sayaçları Sıfırla`, `⊕ Kalibre Et`, `Kabul Test (GREEN)` ve `Red Test (RED - 45°)` butonları seri port komutlarıyla eşleştirildi.

### Aşama 3: OpenCV 4.14.0 ve Harici Endüstriyel USB Kamera Entegrasyonu
- [x] **CMake & OpenCV (`C:/opencv`) Entegrasyonu:** `CMakeLists.txt` içerisine `find_package(OpenCV REQUIRED)` ve `target_link_libraries` tanımları eklendi.
- [x] **MinGW Sembol Köprüsü & Otomatik DLL Kopyalama (`POST_BUILD`):** Qt6 MinGW 64-bit derleyicisinin `opencv_world4140.dll` ile hatasız bağlanması için `dlltool` sembol köprüsü ve derleme sonrası otomatik `.dll` kopyalama adımı yapılandırıldı.
- [x] **Harici Endüstriyel USB Kamera (`İndeks 1`) Desteği:** Sistem açılışında dizüstü bilgisayarın dahili kamerası (`0`) yerine öncelikli olarak harici USB kameranın (`1`) DirectShow (`cv::CAP_DSHOW`) sürücüsüyle düşük gecikmeli (`CAP_PROP_BUFFERSIZE = 1`, `1280x720 @ 30 FPS`) açılması sağlandı.
- [x] **Canlı Kamera Seçim Kutusu (`QComboBox`):** Programı kapatmadan `Harici USB Kamera (Kamera 1)`, `Dahili Kamera (Kamera 0)`, `Harici Kamera 2 (Kamera 2)` ve `Simülasyon Akışı` arasında anlık geçiş altyapısı (`kamerayiAc` / `onCameraSelectionChanged`) kuruldu.
- [x] **Hata Toleransı (Fail-Safe Mekanizması):** Kamera kablosu çekildiğinde veya bağlantı koptuğunda (`frame.empty()`) uygulamanın çökmesini engelleyen, uyarı verip güvenli simülasyon moduna geçen koruma eklendi.
- [x] **Donanımsal Parlaklık ve Pozlama Kontrolü:** Arayüzdeki slider'ların OpenCV üzerinden doğrudan kameranın `cv::CAP_PROP_BRIGHTNESS` ve `cv::CAP_PROP_EXPOSURE` parametrelerini güncellemesi sağlandı.

---

## 🎯 2. YAPILACAKLAR (Gelecek Yol Haritası / To-Do)

### Aşama 4: Yapay Zeka (YOLOv8 / OpenCV DNN) Model Entegrasyonu
- [ ] Kapsül veri seti (sağlam, kırık, çapaklı, ezik, renk kusurlu kapsüller) ile eğitilmiş **YOLOv8 (.onnx)** modelinin `cv::dnn::Net` üzerinden projeye yüklenmesi.
- [ ] Kameradan alınan her kare (veya sensör tetikleme anındaki kare) üzerinde gerçek zamanlı nesne tespiti (Bounding Box + Sınıf Etiketi + Güven Skoru) çalıştırılması.

### Aşama 5: Gerçek Zamanlı Yapay Zeka Isı Haritası (Live Heatmap / ROI Analizi)
- [ ] Orta üst paneldeki **Yapay Zeka Isı Haritası (AI Analysis)** alanının, kameradan yakalanan gerçek kapsül ROI (Region of Interest) görüntüsü üzerinden dinamik anomali yoğunluk haritası (`cv::applyColorMap( ..., cv::COLORMAP_JET )`) üretmesi.
- [ ] Modelden dönen gerçek sınıf ve güven skorunun (`%96.3` vb.) alt uyarı kutusuna anlık yazdırılması.

### Aşama 6: Kapalı Çevrim (Closed-Loop) Otomatik Karar ve Ayırma Senkronizasyonu
- [ ] Arduino'dan `"KAPSUL_GELDI"` kesme mesajı geldiği anda kameradan en net karenin yakalanması.
- [ ] Yapay zeka çıkarım sonucuna göre operatör müdahalesi olmadan otomatik olarak Arduino'ya `'G'` (Sağlam ➔ `0°`) veya `'R'` (Hatalı ➔ `45°` Servo Ayırma) komutunun gönderilmesi.
- [ ] Bant/kanal hızı ile kamera ve servo arasındaki fiziksel mesafeye göre **Tetikleme Gecikmesi (Actuation Delay)** kalibrasyon parametresinin eklenmesi.

### Aşama 7: Çoklu İş Parçacığı (Multi-Threading) ve Gürültü Filtreleme
- [ ] Kamera okuma + AI çıkarım işlemlerinin ana arayüz iş parçacığından (`GUI Thread`) ayrı bir `QThread` / Worker yapısına taşınarak yüksek FPS altında sıfır arayüz takılması sağlanması.
- [ ] TCRT5000 optik sensör için yazılımsal debounce / gürültü filtreleme süresinin optimize edilmesi.

### Aşama 8: Üretim Kayıtları ve Vardiya Raporlama
- [ ] Tespit edilen hatalı kapsüllerin zaman damgalı ekran görüntülerinin `logs/defects/` klasörüne otomatik kaydedilmesi.
- [ ] Vardiya sonunda toplam üretim, kabul/red sayıları ve hata oranlarının **CSV / PDF Kalite Kontrol Raporu** olarak dışa aktarılması.
