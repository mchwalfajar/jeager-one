#define TINY_GSM_MODEM_SIM800

#include <SoftwareSerial.h>
#include <TinyGsmClient.h>
#include <AntaresLoRaWAN.h>
#include <EEPROM.h>
#include <AntaresESP32HTTP.h>
#include "credential.c"


// -------------------------------------GSM-----------------------------------------//

#define PIN_TX     17
#define PIN_RX     16
#define GSM_PIN    13

SoftwareSerial     sim800(PIN_RX,PIN_TX);
#undef TINY_GSM_USE_GPRS
#define TINY_GSM_USE_GPRS true
TinyGsm modem(sim800);

char bufferRX[200];
int indexBufferRX;
bool statusKoneksi = false;


void sendCommand(String AtCommand) {
  sim800.print(AtCommand+ "\r\n");
  delay(500);
  sim800.listen();
  indexBufferRX =0;
  while(sim800.available()){
    bufferRX[indexBufferRX] = sim800.read();
    indexBufferRX++;
  }
  sim800.flush();
}

void cleanBuffer(char *buffer,int count) {
    for(int i=0; i < count; i++){
        buffer[i] = '\0';
    }
}

int readBuffer(char *buffer, int count, unsigned int timeout=0, unsigned int chartimeout=0) {
    int i = 0;
    unsigned long timerStart, prevChar;
    timerStart = millis();
    prevChar = 0;
    while(1){
        while(sim800.available()){
            buffer[i++] = sim800.read();
            prevChar = millis();
            if(i >= count)
                return i;
        }
        if(timeout){
            if((unsigned long) (millis() - timerStart) > timeout*1000){
                break;
            }
        }
        if(((unsigned long) (millis() - prevChar) > chartimeout*10) && (prevChar != 0)){
            break;
        }
    }
    sim800.flush();
    return i;
}

bool sendCommand(const char* cmd, const char* resp) {
    char SIMbuffer[100];
    cleanBuffer(SIMbuffer,100);
    sim800.write(cmd);
    readBuffer(SIMbuffer,100,1,1);
    Serial.println("====> " + (String)cmd);
    Serial.println("Response Want : " + (String)resp);
    Serial.println("<====" + (String)SIMbuffer);
    if(NULL != strstr(SIMbuffer,resp)){
        Serial.println("Masuk");
        return  true;
    }else{
        return  false;
    }
}

void turnONsim800() {
  digitalWrite(GSM_PIN, HIGH);
  delay(2000);
  digitalWrite(GSM_PIN, LOW);
}

boolean checksim800() {
  if (sendCommand("AT\r\n","OK")) Serial.println("cek modul sim800 Sudah ON");
  else
  {
    turnONsim800();
    Serial.println("Tunggu 5 Detik");
    for (int i=0;i<5;i++)
    {
      delay(1000);
      if (sendCommand("AT\r\n","OK"))
      {
        sendCommand("ATE0\r\n","OK");
        Serial.println("SIM800 Berhasil ON");
        i=10;
      }
    }
  }
}

void setGSM()
{
  Serial.println("Set GSM");
  if (sendCommand("AT+CNMP=13\r\n","OK")) Serial.println("Setting GSM OK!");
  delay(500);
  if (sendCommand("AT+CMNB=3\r\n","OK")) Serial.println("Setting GPRS OK!");
}

void checkStatusConnection()
{
    Serial.println("Check Status Connection");
    char SIMbuffer[100];
    cleanBuffer(SIMbuffer,100);
    sim800.write("AT+CIPSTATUS\r\n");
    readBuffer(SIMbuffer,100,1,1);
    Serial.println("Response : " + (String)SIMbuffer);
    if(NULL != strstr(SIMbuffer,"PDP DEACT")) sim800.write("AT+CIPSHUT\r\n");
    if(NULL != strstr(SIMbuffer,"IP START")) sim800.write("AT+CIICR\r\n");
    if(NULL != strstr(SIMbuffer,"IP INITIAL")) sim800.write("AT+CSTT=\"Telkomsel\"\r\n");
    if(NULL != strstr(SIMbuffer,"IP GPRSACT")) sim800.write("AT+CIFSR\r\n");
    if(NULL != strstr(SIMbuffer,"TCP CLOSED")) sim800.write("AT+CIPSHUT\r\n");
    if(NULL != strstr(SIMbuffer,"IP STATUS")) sim800.write("AT+CIPSTART=\"TCP\",\"platform.antares.id\",\"8080\"\r\n");
    if(NULL != strstr(SIMbuffer,"CONNECT OK"))
    {
      statusKoneksi=true;
      Serial.println("CONNECT OK!!");//sim800.write("AT+CIPSEND\r\n");
    }
}

void sendToAntares(String dataSend)
{
    String dataXSend = "{\"m2m:cin\": {\"con\": \"{"+dataSend+"}\"}}";
    Serial.println(dataXSend);
    delay(1000);
    sim800.write("AT+CIPSEND\r\n");
    delay(1000);
    sim800.print("POST /~/antares-cse/antares-id/"+deviceID+" HTTP/1.1\r\nHost: platform.antares.id:8080 \r\n");
    sim800.print("Accept: application/json\r\nContent-Length: "+(String)dataXSend.length()+"\r\n");
    sim800.print("Content-Type: application/json;ty=4\r\n");
    sim800.print("X-M2M-Origin: "+ACCESSKEY+"\r\n\r\n");
    sim800.print(dataXSend);
    delay(100);
    sim800.write(0x1A);
    Serial.println("Send OK");
}

void startConnection()
{
  if (sendCommand("AT+CSTT=\"internet\"\r\n","OK")) Serial.println("Setting APN OK!");
}

void initsim800 ()
{
  sim800.write(0x1A);
  checksim800();
  setGSM();
  delay(500);
  if (sendCommand("AT+CGDCONT=1,\"IP\",\"Telkomsel\"","OK")){
    Serial.println("CGD OK!");
  }
  delay(1000);
}

//--------------------------------------WiFi----------------------------------------//

AntaresESP32HTTP antaresWiFi(ACCESSKEY);  

void WiFiSetup() {
  antaresWiFi.setDebug(true);  
  antaresWiFi.wifiConnection(WIFISSID,PASSWORD); 
}

void kirimWiFi() {
  // Kirim dari penampungan data ke Antares
  antaresWiFi.send(app, deviceName);
  delay(10000);
}

// -------------------------------------LoRa---------------------------------------//
AntaresLoRaWAN antares;
size_t EPROM_MEMORY_SIZE = 512;

String        dataSend = "",
			  counting;
      
int           readaddress,
              address = 0,
              hitung;             
        

void LoRaSetup() {
  Serial.println(F("Starting"));
  EEPROM.begin(EPROM_MEMORY_SIZE);
  EEPROM.get(address, readaddress);
  hitung = readaddress;
  
  antares.setPins(2, 14, 12);                         // Set pins for NSS, DIO0, and DIO1
  antares.setTxInterval(1);                           // Set the amount of interval time (in seconds) to transmit
  antares.setSleep(true);                             // antares.setSleep(true, 10);
  antares.init(ACCESSKEY, DEVICEID);
  antares.setDataRateTxPow(DR_SF10, 17);
}



//Fungsi send data, disarankan untuk tidak merubah fungsi ini
void sendPacket(String &input) {
  String dataA = String(input);
  antares.send(dataA);

  char dataBuf[50];
  int dataLen = dataA.length();
  
  dataA.toCharArray(dataBuf, dataLen + 1);
  
  Serial.println("Data :" + (String)dataLen );
  
  if (dataLen > 1)
  {
    
    Serial.println("\n[ANTARES] Data: " + dataA + "\n");

    if (LMIC.opmode & OP_TXRXPEND) {
      Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
      LMIC_setTxData2(1, (uint8_t*)dataBuf, dataLen, 0);
      Serial.println(F("Packet queued"));
      esp_sleep_enable_timer_wakeup(300 * 1000000);
      //esp_sleep_enable_ext0_wakeup(GPIO_NUM_34,0);
      esp_deep_sleep_start();
    }
  }
  else
  {
    Serial.println("\n[ANTARES] Data: Kosong\n");
  }
  delay(300000);
}

/*
void counter_LoRa_Send() {				  //  counter send data LoRaWAN

	hitung = hitung +1;
	EEPROM.put(address, hitung);
	EEPROM.commit();

	counting =  "Counting saat ini : " + String(hitung);
  
  if (hitung >= 4096) {
    hitung = 0;
    EEPROM.put(address, hitung);
    EEPROM.commit();
    delay(10);
  }
  if (hitung <= 4095 ) {
    Serial.println("saat ini masih" + String(hitung) + ", belum 4096");
    delay(10);
    sendPacket(counting);
  }
   delay(300000);
}
*/











































