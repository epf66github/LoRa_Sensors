// 915 MHz Gateway Module

// Board model: ESP32-WROOM-DA Module

// Sketch version *********************************************************************************
const char *verSkt = "2025/09/04 version";

// ESP-WROOM-32 ideaspark - esp32 by Espressif Systems 2.0.17 *************************************
//                   ______________
//               EN |              | D23   LORA MOSI
//               VP |              | D22   OLED SCL
//               VN |              | TX0
//              D34 |              | RX0
//              D35 |   ________   | D21   OLED SDA
//              D32 |  |        |  | D19   LORA MISO
//              D33 |  |        |  | D18   LORA SCK
//              D25 |  |  OLED  |  | D5    LORA NSS
//              D26 |  |        |  | TX2   
//     SD MISO  D27 |  |________|  | RX2   LORA RST
//     SD SCK   D14 |     ____     | D4    LORA DIO1 
//     SD CS    D12 |    |    |    | D2
//     SD MOSI  D13 |    | USB|    | D15   LORA BUSY
//     SD GND   GND |    |____|    | GND   LORA GND  
//              VIN |______________| 3V3   LORA 3V3   SD 3V3
//

// Libraries definitions **************************************************************************

// Clock libraries
#include "SSD1306Wire.h" // ESP8266 and ESP32 OLED driver for SSD1306 displays by ThingPulse, Fabrice Weinberg 4.1.0
#include "OLEDDisplayUi.h"

// Gateway libraries 
#include "NTPClient.h" // NTPClient by Fabrice Weinberg 3.2.1
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include "SX126x.h" // LoRaRF by Chandra Wijaya Sentosa 2.1.1
#include <FS.h>
#include <SD.h>
#include "AsyncTCP.h" // AsyncTCP by dvarrel 1.1.4
#include "ESPAsyncWebServer.h" // ESPAsyncWebServer by lacamera 3.1.0 ** Install without add-ons

#include "Images.h"

// Port definitions *******************************************************************************

// SPI port: LoRa LLCC68 with ESP-WROOM-32
#define  LoRa_MOSI  23
#define  LoRa_MISO  19
#define  LoRa_SCK   18
#define  LoRa_NSS    5
#define  LoRa_RST   16
#define  LoRa_BUSY  15
#define  LoRa_DIO1   4
#define  LoRa_TX    -1
#define  LoRa_RX    -1

// SPI port:  SD Card
#define  SD_MOSI    13
#define  SD_MISO    27
#define  SD_SCK     14
#define  SD_CS      12

// Oled display
#define SCL_OLED    22
#define SDA_OLED    21

#define Select     LOW
#define DeSelect  HIGH

SPIClass SD_spi(HSPI);

SX126x LoRa;

// Definition of core tasks ***********************************************************************
TaskHandle_t Task0;
TaskHandle_t Task1;

// Constants definitions **************************************************************************
const uint8_t gtwAddr = 0x01;
const uint8_t netAddr = 0xFF;

const uint8_t TP = 20;
const uint8_t SF = 7;
const uint32_t BW = 500E3;
const uint8_t CR = 8;
const uint8_t SW = 0xFE;
const uint32_t HZ = 915E6;
const bool crcEna = true;

const int8_t LTZ = -3;
const uint8_t TUI = 1;

// WiFi definitions *******************************************************************************
const char *WiFissid = "WIFI_SSID";
const char *WiFipass = "WIFI_PASSWORD";

// Variable definitions ***************************************************************************
// Display variables
uint8_t Wol = 128;
uint8_t Hol = 64;
uint8_t cenXol = Wol / 2;
uint8_t cenYol = ((Hol - 16) / 2) + 16;
uint8_t FC = 4;
uint8_t OC = 1;

// Gateway variables
String vetMsg;
String sndMsg;
uint32_t RTI = 1.0 * 60 * 60 * 1000; // 1h default
uint32_t TTI = 4.5 * RTI;            // 4.5 reading cycles by default
uint32_t SCT = 15 * 60 * 1000;       // 15min default
uint32_t EPT = 946758403;            // 2000/01/01 default

// Initializations ********************************************************************************
// Initializing the OLED display with Wire library - clock
SSD1306Wire display(0x3c,SDA_OLED,SCL_OLED);
OLEDDisplayUi ui(&display);

// NTP Service Startup - clock and gateway
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"pool.ntp.org",LTZ * 3600,TUI * 3600 * 1000);

// Http service initialization - gateway
AsyncWebServer server(80);
IPAddress ip;


// Display routines *******************************************************************************
String twoDigits(uint8_t digClk){
  if(digClk < 10){
    String i = '0' + String(digClk);
    return i;
  }else{
    return String(digClk);
  }
}

void clockOverlay(OLEDDisplay *display,OLEDDisplayUiState* state){
}

void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t HDP, int16_t VDP) {
  display->drawXbm(HDP + 4, VDP + 16, Logo_width, Logo_height, Logo_bits);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(cenXol + HDP,cenYol + VDP + 12,"LoRa gateway system");
}

void drawFrame2(OLEDDisplay *display,OLEDDisplayUiState* state,int16_t HDP,int16_t VDP){
  String timeNow = String(timeClient.getHours()) + ":" + twoDigits(timeClient.getMinutes()) + ":" + twoDigits(timeClient.getSeconds());
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(cenXol + HDP,cenYol + VDP,timeNow);
}

void drawFrame3(OLEDDisplay *display,OLEDDisplayUiState* state,int16_t HDP,int16_t VDP){
  String IPDisp = ip.toString();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  display->drawString(cenXol + HDP,cenYol + VDP - 11,IPDisp);
}

void drawFrame4(OLEDDisplay *display,OLEDDisplayUiState* state,int16_t HDP,int16_t VDP){
  String SDDisp = "SD: " + String(int((SD.totalBytes() - SD.usedBytes()) / (1024 *1024))) + " MB free";
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(cenXol + HDP,cenYol + VDP - 26,SDDisp);
}

FrameCallback frames[] = {drawFrame1,drawFrame2,drawFrame3,drawFrame4};
OverlayCallback overlays[] = {clockOverlay};


// Gateway routines *******************************************************************************
// Connecting ESP32 to WiFi
void WiFi_connect(){
  if(WiFi.status() != WL_CONNECTED){
    WiFi.begin(WiFissid,WiFipass);
    delay(5000);
  }
  if(WiFi.status() == WL_CONNECTED){
    Serial.print("\nConnected to WiFi ");
    Serial.println(WiFissid);
    Serial.println(ip);
  }else{
    Serial.println("\nWiFi disconnected!");
  }
  ip = WiFi.localIP();
}

// Create a folder on the SD Card
void createDir(fs::FS &fs,String path){
    Serial.println("\nCreating folder: " + path);
    if(fs.mkdir(path)){
        Serial.println("Folder created!");
    }else{
        Serial.println("\nFolder creation failed!\n");
    }
}

// List the folders on the SD Card module and mount the html file
void listDir(fs::FS &fs,const char *path,uint8_t dirLev){
  Serial.print("\nListing directory: ");
  Serial.println(path);
  File root = fs.open(path);
  if(!root){
      Serial.println("\nFailed to open directory!\n");
      return;
  }
  if(!root.isDirectory()){
    Serial.println("\nNot a directory!\n");
    return;
  }
  String fileHTML = "<!DOCTYPE HTML><html><head>"
    "<meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ESP Web Server</title>"
    "</head><body><h1>Sensors Modules</h1>"
    "<p>Click on a link below to access the data from the respective sensor module, recorded on the microSD Card.</p><ul>";
  writeFile(SD,"/index.html",fileHTML);
  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      String path = file.name();
      Serial.print("  DIR : ");
      Serial.println(path);
      if(dirLev){
        listDir(fs,file.path(),dirLev -1);
      }
      if(path.substring(0,2) == "0x"){
        fileHTML = String("<li><a href=\"../" + path + "/" + path + ".DAT\">Module " + path + "</a></li>");
        appendFile(SD,"/index.html",fileHTML);
      }
    }else{
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
  fileHTML = "</ul></body></html>";
  appendFile(SD,"/index.html",fileHTML);
  Serial.print("index.html file in ");
  Serial.println(ip);
}

// Creates a file in a folder on the SD Card
void writeFile(fs::FS &fs,String path,String arcTxt){
  if(path != "/index.html"){
    Serial.println("Writing to the file: " + path);
  }
  File file = fs.open(path,FILE_WRITE);
  if(!file){
    Serial.println("\nFailed to open file for writing!\n");
    return;
  }
  if(path != "/index.html"){
    file.write(0xEF);
    file.write(0xBB);
    file.write(0xBF);
  }
  if(file.print(arcTxt)){
    if(path != "/index.html"){
      Serial.println("\nWritten message!");
    }
  }else{
    Serial.println("\nFailed to write!");
  }
  file.close();
}

// Add content to a file in a folder on the SD Card
void appendFile(fs::FS &fs,String path,String arcTxt){
  if(path != "/index.html"){
    Serial.println("Attaching to file: " + path);
  }
  File file = fs.open(path,FILE_APPEND);
  if(!file){
    Serial.println("\nFailed to open file for attachment!\n");
    return;
  }
  if(file.print(arcTxt)){
    if(path != "/index.html"){
      Serial.println("Attached message!");
    }
  }else{
    Serial.println("\nFailed to attach!");
  }
  file.close();
}

// Send a LoRa message
void sendMessage(String vetMsg,uint8_t snrAddr){
  int lenMsg = vetMsg.length() + 1;
  char arrMsg[lenMsg];
  vetMsg.toCharArray(arrMsg,lenMsg);
  LoRa.beginPacket();
  LoRa.write(snrAddr);
  LoRa.write(gtwAddr);
  LoRa.write(lenMsg);
  LoRa.write(arrMsg,lenMsg);
  LoRa.endPacket();
  LoRa.wait(10);
}

// Defines configuration messages to be sent to sensors
void sendCFG(uint8_t snrAddr){
  EPT = timeClient.getEpochTime();
  if(snrAddr == netAddr){
    sndMsg = String("CONFIG:" + String(EPT) + ";" + String(RTI) + ";" + String(TTI));
  }else{
    sndMsg = String("RECEIV:" + String(EPT) + ";" + String(RTI) + ";" + String(TTI));
  }
  String snrID = String(snrAddr,HEX);
  if(snrAddr < 10){
    snrID = String("0" + snrID);
  }
  snrID.toUpperCase();
  snrID = "0x" + snrID;
  sendMessage(sndMsg,snrAddr);
  Serial.println("\nMessage sent to " + snrID);
  Serial.println(sndMsg);
}

// Receive message
void onReceive(){
  LoRa.purge();
  LoRa.request();
  LoRa.wait(500);
  if(LoRa.available() < 20){
    return;
  }
  uint8_t recAddr = LoRa.read();
  if(recAddr != gtwAddr){
    return;
  }
  uint8_t sndAddr = LoRa.read();
  uint16_t incLen = LoRa.read();
  String recMsg = "";
  while(LoRa.available()){
    recMsg += (char)LoRa.read();
  }
  if(incLen != recMsg.length()){
    return;                        
  }
  recMsg = recMsg.substring(0,incLen-1);
  String snrID = String(sndAddr,HEX);
  if(sndAddr < 10){
    snrID = String("0" + snrID);
  }
  snrID.toUpperCase();
  snrID = "0x" + snrID;
  String gtwID = String(gtwAddr,HEX);
  if(gtwAddr < 10){
    gtwID = String("0" + gtwID);
  }
  gtwID.toUpperCase();
  gtwID = "0x" + gtwID;
  Serial.println("\nReceived from device: " + snrID);
  Serial.println("Sent to: " + gtwID);
  Serial.println("Message size: " + String(incLen));
  Serial.println("Message: " + recMsg);
  String RSSI = String(LoRa.packetRssi());
  String SNR = String(LoRa.snr());
  Serial.println("RSSI: " + RSSI);
  Serial.println("SNR: " + SNR);
  sendCFG(sndAddr);
  digitalWrite(SD_CS,Select);
  String toSD = String(snrID + "\t" + recMsg + "\t" + RSSI + "\t" + SNR + "\n");
  String folder = "/" + snrID;
  if(SD.exists(folder)){
    Serial.println("\nExisting directory!");
  }else{
    createDir(SD,folder);
  }
  String file = folder + "/" + snrID + ".DAT";
  if(SD.exists(file)){
    Serial.println("\nExisting file!");
    appendFile(SD,file,toSD);
  }else{
    Serial.println("\nNon-existent file!");
    writeFile(SD,file,toSD);
  }
  digitalWrite(SD_CS,DeSelect);
}

// SETUP ******************************************************************************************
void setup(){
  // initialization of digital ports for LoRa
  pinMode(LoRa_MOSI,OUTPUT);
  pinMode(LoRa_MISO,INPUT);
  pinMode(LoRa_SCK,OUTPUT);
  pinMode(LoRa_NSS,OUTPUT);
  pinMode(LoRa_RST,OUTPUT);
  pinMode(LoRa_BUSY,INPUT);
  pinMode(LoRa_DIO1,INPUT);
  digitalWrite(LoRa_MOSI,LOW);
  digitalWrite(LoRa_SCK,LOW);
  digitalWrite(LoRa_NSS,LOW);
  digitalWrite(LoRa_RST,LOW);
  // Serial initialization
  Serial.begin(9600);  
  while(!Serial);
  // SD Card module initialization
  Serial.print("\n915 MHz Gateway Module - ");
  Serial.println(verSkt);
  Serial.println("\nStarting SD Card module!");
  digitalWrite(SD_CS,Select);
  SD_spi.begin(SD_SCK,SD_MISO,SD_MOSI,SD_CS);
  if(!SD.begin(SD_CS,SD_spi)){
    Serial.println("\nSD Card mounting failure!\n");
    while(true);
  }
  uint8_t typSD = SD.cardType();
  if(typSD == CARD_NONE){
    Serial.println("\nSD Card not connected!\n");
    return;
  }
  Serial.print("SD Card type: ");
  if(typSD == CARD_MMC){
    Serial.println("MMC");
  }else if(typSD == CARD_SD){
    Serial.println("SDSC");
  }else if(typSD == CARD_SDHC){
    Serial.println("SDHC");
  }else{
    Serial.println("UNKNOWN");
  }
  uint64_t sizSD = SD.cardSize() / (1024 * 1024);
  uint64_t spcSD = SD.totalBytes() / (1024 * 1024);
  uint64_t useSD = SD.usedBytes() / (1024 * 1024);
  Serial.printf("SD Card size: %lluMB\n",sizSD);
  Serial.printf("Total space: %lluMB\n",spcSD);
  Serial.printf("Space used: %lluMB\n",useSD);
  listDir(SD,"/",0);
  digitalWrite(SD_CS,DeSelect); 
  // LoRa module initialization
  LoRa.setPins(LoRa_NSS,LoRa_RST,LoRa_BUSY,LoRa_DIO1,LoRa_TX,LoRa_RX);
  Serial.println("\n\nStarting LoRa half-duplex module!");
  if (!LoRa.begin()){
    Serial.println("\nError initializing LoRa module! Check pin connections.\n");
    while(true);
  }
  LoRa.setFrequency(HZ);
  LoRa.setTxPower(TP, SX126X_TX_POWER_SX1262);
  LoRa.setSpreadingFactor(SF);
  LoRa.setBandwidth(BW);
  LoRa.setCodeRate(CR);
  LoRa.setCrcEnable(crcEna);
  LoRa.setSyncWord(SW);
  Serial.println("LoRa module started successfully!");
  // Initializing WiFi and NTP
  Serial.print("\n\nConnecting to WiFi!");
  do{
    Serial.print(".");
    WiFi.begin(WiFissid,WiFipass);
    delay(5000);
  }while(WiFi.status() != WL_CONNECTED);
  Serial.print("\nConnected to WiFi ");
  Serial.println(WiFissid);
  ip = WiFi.localIP();
  Serial.println(ip);
  timeClient.begin();
  timeClient.update();
  EPT = timeClient.getEpochTime();
  // WebServer initialization
  server.on("/",HTTP_GET,[](AsyncWebServerRequest *request){
    request->send(SD,"/index.html","text/html");
  });
  server.serveStatic("/",SD,"/");
  server.begin();
  // Display initialization
  Serial.println();
  ui.setTargetFPS(30);
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);
  ui.setIndicatorPosition(TOP);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames,FC);
  ui.setOverlays(overlays,OC);
  ui.init();
  display.flipScreenVertically();
  //create a task that will be executed in the Task0code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(Task0code,"OLED",10000,NULL,1,&Task0,0);               
  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(Task1code,"GATEWAY",10000,NULL,1,&Task1,1);
}

void Task0code(void * pvParameters){
  Serial.print("Task0code() running on core ");
  Serial.println(xPortGetCoreID());
  uint32_t LWF = 0;
  int32_t TAU;
  while(1){
    TAU = millis() - LWF;
    if(abs(TAU) > SCT){
      WiFi_connect();
      if(WiFi.status() == WL_CONNECTED){
        timeClient.update();
      }
      LWF = millis();
    }
    uint16_t RTB = ui.update();
    if(RTB > 0){
      delay(RTB);
    }
  }
}

void Task1code(void * pvParameters){
  Serial.print("Task1code() running on core ");
  Serial.println(xPortGetCoreID());
  uint32_t LST = 0;
  int32_t TAU;
  while(1){
    TAU = millis() - LST;
    if(abs(TAU) > SCT){
      sendCFG(netAddr);
      LST = millis();
      listDir(SD,"/",0);
    }
    onReceive();
  }
}

void loop(){
}

/*
Constants and variables (name - type - discrimination) ********************************************

crcEna   - bool     - LoRa CRC check
dirName  - char     - Directory name on SD Card
verSkt   - char     - Sketch version
WiFipass - char     - Wi-Fi password
WiFissid - char     - Wi-Fi SSID
arrMsg   - char     - Array message
arcTxt   - String   - Content to be recorded on the SD Card
file     - String   - Full name of the file to be written to the SD Card, including folder
fileHTML - String   - Header of the index.html file
folder   - String   - Name of the folder where the file will be saved on the SD Card
gtwID    - String   - Gateway ID
IPDisp   - String   - IP displayed on the display
path     - String   - path on SD Card
recMsg   - String   - Content of the received message
RSSI     - String   - Broadcast RSSI value (dBm)
SDDisp   - String   - Available space on the SD Card shown on the display (MB)
sndMsg   - String   - Message to be sent
SNR      - String   - Transmission SNR value (dB)
snrID    - String   - Sensor ID
timeNow  - String   - Time shown on the display
toSD     - String   - Formatted text line for writing to SD Card
vetMsg   - String   - Message vector
lenMsg   - int      - Message length
LTZ      - int8_t   - Local time zone (hours)
HDP      - int16_t  - Horizontal display positioner (pixels)
VDP      - int16_t  - Vertical display positioner (pixels)
TAU      - int32_t  - Accumulated time since the last of the correct hour (milliseconds)
cenXol   - uint8_t  - Central X coordinate of the display (pixels)
cenYol   - uint8_t  - Central Y coordinate of the blue band on the display (pixels)
CR       - uint8_t  - LoRa Coding Rate: 5-8
digClk   - uint8_t  - Controlling the number of clock digits on the display
dirLev   - uint8_t  - Directory Level on SD Card
TUI      - uint8_t  - Time update interval (hours)
FC       - uint8_t  - Number of frames
gtwAddr  - uint8_t  - Address of this LoRa gateway device (0x01 default)
Hol      - uint8_t  - Vertical screen size in pixels
netAddr  - uint8_t  - Generic sensor address (0xFF default)
OC       - uint8_t  - Add overlays
recAddr  - uint8_t  - Recipient address
SF       - uint8_t  - LoRa Spreading Factor: 5-9 (125 kHz); 5-10 (250 kHz); 5-11 (500 kHz)
sndAddr  - uint8_t  - Sender's address
snrAddr  - uint8_t  - LoRa sensor device address
SW       - uint8_t  - LoRa Sync Word
TP       - uint8_t  - LoRa TX power: 2-22 (dBm)
typSD    - uint8_t  - SD Card type
Wol      - uint8_t  - Horizontal screen size (pixels)
incLen   - uint16_t - Size of received message
RTB      - uint16_t - Time remaining until display update (milliseconds)
SCT      - uint32_t - Time delay in sending the correct time (milliseconds)
BW       - uint32_t - LoRa signal bandwidth: 125E3, 250E3, 500E3 (Hz)
EPT      - uint32_t - Epoch Time (seconds)
HZ       - uint32_t - LoRa Frequency (Hz)
LST      - uint32_t - Timestamp of the last time-correct message sent (milliseconds)
LWF      - uint32_t - Time since last WiFi update (milliseconds)
RTI      - uint32_t - Readings time interval (milliseconds)
TTI      - uint32_t - Transmission time interval (milliseconds)
sizSD    - uint64_t - SD Card Memory Amount (MB)
spcSD    - uint64_t - Available memory on SD Card (MB)
useSD    - uint64_t - Used memory on SD Card (MB)
*/
