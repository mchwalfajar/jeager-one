#include "jeager/jeager.c"
#include <Adafruit_SHT31.h>
#include <Wire.h>

/* -------------------------- Inisialisasi Sensor -------------------------- */
Adafruit_SHT31 sht31 = Adafruit_SHT31();  

float temp,hum;       
float kalibrasi_temp = -(0.5),                                        // Variable for calibration temperature      
      kalibrasi_hum = 0;                                              // Variable for callibration humidity

void i2cSensor(){
  temp = sht31.readTemperature() + kalibrasi_temp;                    // Get data sensor suhu                                          
  hum = sht31.readHumidity() + kalibrasi_hum;                         // Get data sensor kelembaban
}

/* -------------------------- Pengiriman data sesuai sensor -------------------------- */
void sendToWiFi() {
  antaresWiFi.add("T", temp);   // ubah bagian ini
  antaresWiFi.add("H", hum);    // ubah bagian ini
  
  kirimWiFi();
}

void sendToLoRa() {
  dataSend = "T:"+String(temp)+",H:"+String(hum);     // ubah bagian ini
  
  delay(10000);
  Serial.println(dataSend);
  antares.init(ACCESSKEY, DEVICEID);sendPacket(dataSend);
}

void GSMSetup() {
  sim800.begin(9600);  
  pinMode(GSM_PIN,OUTPUT);
  initsim800();
}

void sendToGSM() {
    checkStatusConnection();     
    if (statusKoneksi) {
    sendToAntares("\\\"T\\\":\\\""+(String)temp+"\\\",\\\"H\\\":\\\""+(String)hum+"\\\"");    // ubah bagian ini
    Serial.println("data terkirim");
    delay(60000);
  }
}
/* -------------------------------------------------------------------------------------- */


void setup() {
  Serial.begin(9600);
  sht31.begin(0x44);  

  /* beri "//" pada koneksi yang tidak digunakan */
  LoRaSetup();
  //WiFiSetup();
  //GSMSetup;
}

void loop() {
  i2cSensor();

  /* beri "//" pada koneksi yang tidak digunakan */
  sendToLoRa();
  //sendToWiFi();
  //sendToGSM();
}
