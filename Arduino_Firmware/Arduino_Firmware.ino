#include <Servo.h>

const int sensorPin = 2; // TCRT5000 D0 pini
const int servoPin = 9;  // SG90 Turuncu/Sarı sinyal pini

volatile bool kapsulAlgilandiBayrak = false;
bool sistemAktif = true;
Servo ayiriciMotor;

void setup() {
  // Qt arayüzü ile tam uyum için 9600 baud rate
  Serial.begin(9600);
  pinMode(sensorPin, INPUT);
  
  // Servo motoru başlat ve 0 (başlangıç) konumuna getir
  ayiriciMotor.attach(servoPin);
  ayiriciMotor.write(0); 

  // Kesme (Interrupt) ile sensör takibi
  attachInterrupt(digitalPinToInterrupt(sensorPin), kapsulGordu, FALLING);
  
  Serial.println("SISTEM_BASLATILDI");
}

void loop() {
  // 1. Bilgisayardan (Qt Arayüzünden) gelen komutları dinle
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    if (cmd == 'S') {
      // Başlat
      sistemAktif = true;
      Serial.println("DURUM: SISTEM_AKTIF");
    }
    else if (cmd == 'E') {
      // Acil Durdurma
      sistemAktif = false;
      ayiriciMotor.write(0);
      Serial.println("DURUM: ACIL_DURDURULDU");
    }
    else if (cmd == 'C') {
      // Kalibrasyon
      ayiriciMotor.write(0);
      Serial.println("DURUM: KALIBRE_EDILDI");
    }
    else if (cmd == 'G') {
      // Sağlam kapsül (Kabul - 0 derece)
      ayiriciMotor.write(0);
      Serial.println("KABUL");
    } 
    else if (cmd == 'R') {
      // Hatalı kapsül (Red - 45 dereceye çevirip geri dön)
      ayiriciMotor.write(45);
      delay(350); // Servo hareket süresi
      ayiriciMotor.write(0); // Eski pozisyona dön
      Serial.println("RED");
    }
  }

  // 2. Sensör kapsül gördüğünde bilgisayara haber ver
  if (kapsulAlgilandiBayrak) {
    if (sistemAktif) {
      Serial.println("KAPSUL_GELDI");
    }
    kapsulAlgilandiBayrak = false;
    delay(200); // Çift okumayı engellemek için kısa bir koruma gecikmesi
  }
}

void kapsulGordu() {
  kapsulAlgilandiBayrak = true;
}