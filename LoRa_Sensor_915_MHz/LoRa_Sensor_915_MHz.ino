// 915 MHz Sensor Module

// Sketch version *********************************************************************************
const char *verSkt = "2025/09/04 version";

// LoRa Communication with Arduino ****************************************************************
//               Arduino Nano                        Arduino Pro Mini 3v3 ATmega 328P (3.3V, 8 MHz)
//                 _______                                            _______  
//               o|  ooo  |o                                        o|ooooooo|o
//               o|  ooo  |o                                        o|       |o
//               o|       |o                                        o|       |o
//      GND GND  o|       |o                               GND GND  o|       |o  VCC 3V3
//   DIO0/1 D2   o|       |o                            DIO0/1 D2   o|       |o
//     BUSY D3   o|       |o                              BUSY D3   o|       |o
//      RST D4   o|       |o                               RST D4   o|       |o
//               o|       |o                                        o|       |o
//               o|       |o                                        o|       |o  D13 SCK
//               o|       |o                                        o|       |o  D12 MISO
//               o|       |o                                        o| |RST| |o  D11 MOSI
//               o|  ___  |o                                        o|_______|o  D10 NSS
//      NSS D10  o| |   | |o
//     MOSI D11  o| |usb| |o  3V3 VCC  
//     MISO D12  o|_|___|_|o  D13 SCK  
// ************************************************************************************************

// Libraries definitions **************************************************************************

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SX126x.h>
#include <UnixTime.h> // UnixTime by AlexGyver 1.1.0

SX126x LoRa;
UnixTime DT(0);

// Port definitions *******************************************************************************

//  SPI port:  LoRa LLCC68 with Arduino (Nano or Pro Mini)
#define  LoRa_MOSI  11 
#define  LoRa_MISO  12 
#define  LoRa_SCK   13 
#define  LoRa_NSS   10
#define  LoRa_RST    4
#define  LoRa_BUSY   3
#define  LoRa_DIO1   2
#define  LoRa_TX    -1
#define  LoRa_RX    -1

// Constants definitions **************************************************************************
const uint8_t snrAddr = 0x02;
const uint8_t snrMod = 1;
const uint8_t gtwAddr = 0x01;

const uint8_t TP = 20;
const uint8_t SF = 7;
const uint32_t BW = 500E3;
const uint8_t CR = 8;
const uint8_t SW = 0xFE;
const uint32_t HZ = 915E6;
const bool crcEna = true;

const uint8_t snrNum = 4;
const char snrTyp[] = {'R','R','R','R'};
const uint8_t readPin[] = {A0,A1,A2,A3};
const uint8_t VCCpinR = 5;
const uint8_t VCCpinS[] = {6,7,8,9};
const uint16_t lowSnr[] = {0,0,0,0};
const uint16_t uppSnr[] = {1023,1023,1023,1023};
const float calLow[] = {0,0,0,0};
const float calUpp[] = {100,100,100,100};
uint16_t RX = 1480;

const uint32_t IRV = 1063;

// Variable definitions ***************************************************************************
volatile uint8_t f_wdt = 1;

String snrDat;
String colDat = "";

bool cfgSnr = false;
bool msgRec;
uint32_t RTI = 1 * 60 * 60 * 1000;   // 1h default
uint32_t TTI = 4.5 * RTI;            // 4.5 reading cycles by default
uint32_t EPT = 946758403;            // 2000/01/01 default
uint32_t looSnr;
uint32_t looTmt;
uint32_t valRes;

// Watchdog Interrupt Service *********************************************************************
ISR(WDT_vect){
  if(f_wdt == 0){
    f_wdt=1;
  }
}

// Sensors module routines ************************************************************************
// Send a LoRa message
void sendMessage(String vetMsg){
  msgRec = false;
  int lenMsg = vetMsg.length() + 1;
  char arrMsg[lenMsg];
  vetMsg.toCharArray(arrMsg,lenMsg);
  LoRa.beginPacket();
  LoRa.write(gtwAddr);
  LoRa.write(snrAddr);
  LoRa.write(lenMsg);
  LoRa.write(arrMsg,lenMsg);
  LoRa.endPacket();
  LoRa.wait();
}
 
// Receive message
void onReceive(){
  LoRa.purge();
  LoRa.request(5000);
  LoRa.wait(250);
  if(LoRa.available() < 20){
    return;
  }
  uint8_t recAddr = LoRa.read();
  uint8_t sndAddr = LoRa.read();
  if(sndAddr == gtwAddr){
    if(recAddr != snrAddr && recAddr != 0xFF){
      delay(30000);
      return;
    }
  }else{
    delay(1000);
    return;
  }
  uint8_t incLen = LoRa.read();
  String recMsg = "";
  while(LoRa.available()){
    recMsg += (char)LoRa.read();
  }
  if(incLen != recMsg.length()){   
    delay(1000);
    return;                        
  }
  String sbjMsg = recMsg.substring(0,6);
  if((sbjMsg == "CONFIG" && cfgSnr == false) || (sbjMsg == "RECEIV" && recAddr == snrAddr)){
    uint16_t aux1 = recMsg.indexOf(";",7);
    uint16_t aux2 = recMsg.indexOf(";",aux1 + 1);
    uint16_t aux3 = recMsg.indexOf(";",aux2 + 1);
    uint32_t EPTtr = recMsg.substring(7,aux1).toInt();
    uint32_t RTItr = recMsg.substring(aux1 + 1,aux2).toInt();
    uint32_t TTtr = recMsg.substring(aux2 + 1,aux3).toInt();
    if(EPTtr != 0){
      EPT = EPTtr;
    }
    if(RTItr != RTI) RTI = RTItr;
    if(TTtr != TTI) TTI = TTtr;
    Serial.println("\n\nEpoch Time: " + String(EPT) + " s");
    Serial.println("Read Time: " + String(RTI / 1000) + " s");
    Serial.println("Airtime: " + String(TTI / 1000) + " s");
    Serial.println("RSSI: " + String(LoRa.packetRssi()));
    Serial.println("SNR: " + String(LoRa.snr()));
    looSnr = RTI / 8000;
    looTmt = TTI / RTI;
    cfgSnr = true;
    if(sbjMsg == "RECEIV"){
      msgRec = true;
    }
  }else if(recAddr == snrAddr){
    Serial.println("\nReceived from device: 0x" + String(sndAddr,HEX));
    Serial.println("Sent to: 0x" + String(recAddr,HEX));
    Serial.println("Message size: " + String(incLen));
    Serial.println("Message: " + recMsg);
    Serial.println("RSSI: " + String(LoRa.packetRssi()));
    Serial.println("SNR: " + String(LoRa.snr()));
    Serial.println();
  }
}

// Sensor readings ********************************************************************************
void readSensor(){
  if(snrMod == 1){
    DT.getDateTime(EPT);
    uint16_t dateTime[6] = {DT.year,DT.month,DT.day,DT.hour,DT.minute,DT.second};
    snrDat = String(dateTime[0]);
    for(uint8_t cont0 = 1;cont0 < 3;cont0++){
      if(dateTime[cont0] < 10){
        snrDat += "/0" + String(dateTime[cont0]);
      }else{
        snrDat += "/" + String(dateTime[cont0]);
      }
    }
    if(dateTime[3] < 10){
      snrDat += "\t0" + String(dateTime[3]);
    }else{
      snrDat += "\t" + String(dateTime[3]);
    }
    for(uint8_t cont0 = 4;cont0 < 6;cont0++){
      if(dateTime[cont0] < 10){
        snrDat += ":0" + String(dateTime[cont0]);
      }else{
        snrDat += ":" + String(dateTime[cont0]);
      }
    }
  }
  power_adc_enable();
  float VCCsupp = getBandgap();
  uint32_t sum;
  uint8_t cont0;
  for(uint8_t snrID = 0;snrID < snrNum;snrID++){
    sum = 0;
    for(cont0 = 0;cont0 < 20;cont0++){
      if(snrTyp[snrID] == 'R'){
        pinMode(readPin[snrID],INPUT);
        pinMode(VCCpinR,OUTPUT);
        digitalWrite(VCCpinR,LOW);
        digitalWrite(VCCpinS[snrID],HIGH);
        delayMicroseconds(90);
        sum += analogRead(readPin[snrID]);
        digitalWrite(VCCpinS[snrID],LOW);
        pinMode(readPin[snrID],OUTPUT);
        digitalWrite(readPin[snrID],LOW);
        delay(100);
        digitalWrite(readPin[snrID],HIGH);
        digitalWrite(VCCpinR,HIGH);
        delayMicroseconds(90);
        digitalWrite(VCCpinR,LOW);
        pinMode(readPin[snrID],OUTPUT);
        digitalWrite(readPin[snrID],LOW);
        delay(100);
      }
      if(snrTyp[snrID] == 'D'){
        pinMode(readPin[snrID],INPUT);
        pinMode(VCCpinR,INPUT);
        digitalWrite(VCCpinS[snrID],HIGH);
        delay (50);
        sum += analogRead(readPin[snrID]);
        digitalWrite(VCCpinS[snrID],LOW);
        pinMode(readPin[snrID],OUTPUT);
        digitalWrite(readPin[snrID],LOW);
        delay(50);
      }
    }
    uint16_t valSnr = sum / cont0;
    float VCCsnr = (VCCsupp * valSnr / 1023.0);
    uint16_t OhmSnr = RX * (VCCsupp / VCCsnr - 1);
    uint32_t valCal = map(valSnr,lowSnr[snrID],uppSnr[snrID],calLow[snrID],calUpp[snrID]);
    snrDat += "\t" + String(snrID) + snrTyp[snrID] + "\t" + String(OhmSnr);
  }
  snrDat += "\t" + String(VCCsupp,2) + "V";
  Serial.println(snrDat);
  power_adc_disable();
}

// Get the voltage at the Arduino's VCC port ******************************************************
float getBandgap(){
  uint8_t cont0 = 0;
  uint16_t sum = 0;
  do{
    ADMUX = bit (REFS0) | bit (MUX3) | bit (MUX2) | bit (MUX1);
    ADCSRA |= bit(ADSC);
    while(ADCSRA & bit(ADSC)){
    }
    delay(50);
    sum += ADC;
    cont0++;
  }while(cont0 < 10);
  float VCCsupp = ((IRV * 1023) / (sum / cont0)) / 1000.0;
  return VCCsupp;
}

// SETUP ******************************************************************************************
void setup(){
  // initialization of digital ports for sensors
  uint8_t cont0 = 0;
  do{
    pinMode(readPin[cont0],OUTPUT);
    pinMode(VCCpinS[cont0],OUTPUT);
    digitalWrite(readPin[cont0],LOW);
    digitalWrite(VCCpinS[cont0],LOW);
    cont0++;
  }while(cont0 < 4);
  pinMode(VCCpinR,OUTPUT);
  digitalWrite(VCCpinR,LOW);
  // Serial initialization
  Serial.begin(9600); 
  while(!Serial);
  Serial.print("\n915 MHz Sensor Module - ");
  Serial.println(verSkt);
  String snrID = String(snrAddr,HEX);
  if(snrAddr < 10){
    snrID = String("0" + snrID);
  }
  snrID.toUpperCase();
  snrID = "0x" + snrID;
  Serial.print("\nSensor module ");
  Serial.println(snrID);
  // initialization of digital ports for LoRa
  pinMode(LoRa_MOSI,OUTPUT);
  pinMode(LoRa_MISO,INPUT);
  pinMode(LoRa_SCK,OUTPUT);
  pinMode(LoRa_NSS,OUTPUT);
  pinMode(LoRa_RST,OUTPUT);
  pinMode(LoRa_BUSY,INPUT);
  digitalWrite(LoRa_MOSI,LOW);
  digitalWrite(LoRa_SCK,LOW);
  digitalWrite(LoRa_NSS,LOW);
  digitalWrite(LoRa_RST,LOW);
  // Definition of operating mode
  while(snrMod == 0){
    snrDat = "Calibration mode:";
    readSensor();
  }
  // LoRa module initialization 
  Serial.println("\nStarting LoRa half-duplex module!");
  LoRa.setPins(LoRa_NSS,LoRa_RST,LoRa_BUSY,LoRa_DIO1,LoRa_TX,LoRa_RX);
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
  while(cfgSnr == false){
    onReceive();
  }
  Serial.println("\nEntering Power Down Mode!");
  delay(500);
  // WDT initialization
  MCUSR &= ~(1<<WDRF);
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  WDTCSR = 1<<WDP0 | 1<<WDP3;
  WDTCSR |= _BV(WDIE);
  // Setting sleep mode and disabling Arduino components
  power_adc_disable();
  power_twi_disable();
  power_usart0_disable();
  power_timer1_disable();
  power_timer2_disable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

// Microcontroller Loop - LoRa Communication Operations *******************************************
void loop(){
  LoRa.sleep();
  uint32_t cont1 = 0;
  do{
    Serial.flush();
    uint32_t cont2 = 0; 
    sleep_enable();
    do{
      if(f_wdt == 1){
        f_wdt = 0;
        sleep_mode();
      }
      cont2++;
    }while(cont2 < looSnr);
    sleep_disable();
    valRes = millis();
    EPT += 8 * looSnr;
    readSensor();
    colDat += snrDat;
    cont1++;
    EPT += (millis() - valRes) / 1000;
  }while(cont1 < looTmt);
  valRes = millis();
  LoRa.wake();
  Serial.print("\nSending message to " + String(HZ / 1E6) + " MHz\n");
  uint16_t aux1 = 0;
  String sndMsg = "";
  do{
    uint16_t aux2 = colDat.indexOf("V",aux1) + 1;
    sndMsg = colDat.substring(aux1,aux2);
    do{
      sendMessage(sndMsg);
      onReceive();
    }while(msgRec == false);
    aux1 = aux2;
  }while((colDat.length() - aux1) >= sndMsg.length());
  colDat = "";
}

/*
Constants and variables (name - type - discrimination) ********************************************

cfgSnr     - bool     - Module status as configured by the gateway
crcEna     - bool     - LoRa CRC check
msgRec     - bool     - Message receipt validation
verSkt     - char     - Sketch version
arrMsg     - char     - Array message
snrTyp     - char     - Sensor type: resistive (R); non-resistive (D)
colDat     - String   - Data ready for transmission
recMsg     - String   - Content of the received message
sbjMsg     - String   - Subject of the received message
sndMsg     - String   - Message to be sent
snrDat     - String   - Sensor's data
snrID      - String   - Sensor ID
vetMsg     - String   - Message vector
lenMsg     - int      - Message length
cont0      - uint8_t  - Counter
CR         - uint8_t  - LoRa Coding Rate: 5-8
f_wdt      - uint8_t  - Volatile flag to watchdog timer interrupt
gtwAddr    - uint8_t  - Address of the LoRa gateway device (0x01 default)
incLen     - uint8_t  - Size of received message
reaPin     - uint8_t  - Analog ports for reading the sensors
recAddr    - uint8_t  - Recipient address
SF         - uint8_t  - LoRa Spreading Factor: 5-9 (125 kHz); 5-10 (250 kHz); 5-11 (500 kHz)
sndAddr    - uint8_t  - Sender's address
snrAddr    - uint8_t  - LoRa sensor device address
snrID      - uint8_t  - Sensor ID
snrMod     - uint8_t  - Module operating mode: calibration (0); standard (1)
snrNum     - uint8_t  - Number of sensors (4 max.)
SW         - uint8_t  - LoRa Sync Word
TP         - uint8_t  - LoRa TX power: 2-22 (dBm)
VCCpinR    - uint8_t  - Digital port for grounding the resistors
VCCpinS    - uint8_t  - Digital inputs and sensor sources
aux1       - uint16_t - Auxiliary
aux2       - uint16_t - Auxiliary
aux3       - uint16_t - Auxiliary
dateTime   - uint16_t - Timestamp
lowSnr     - uint16_t - Lower limit reading of the sensors
OhmSnr     - uint16_t - Ohmic resistance of the sensor (ohms)
RX         - uint16_t - Grounding resistor value (ohms)
uppSnr     - uint16_t - Upper limit reading of the sensors
valSnr     - uint16_t - Average reading value at the analog port
BW         - uint32_t - LoRa signal bandwidth: 125E3, 250E3, 500E3 (Hz)
cont1      - uint32_t - Counter
cont2      - uint32_t - Counter
EPT        - uint32_t - Epoch Time (seconds)
EPTtr      - uint32_t - Epoch time transmitted by the gateway (seconds)
HZ         - uint32_t - LoRa Frequency (Hz)
IRV        - uint32_t - Value for adjust to board's specific internal bandgap voltage: Nano (1054); Pro Mini (1063)
looSnr     - uint32_t - Number of loops between each sensor reading.
looTmt     - uint32_t - Number of sensor transmission reading loops
RTI        - uint32_t - Readings time interval (milliseconds)
RTItr      - uint32_t - Time interval between readings transmitted by the gateway (milliseconds)
sum        - uint32_t - Accumulator
TTI        - uint32_t - Transmission time interval (milliseconds)
TTItr      - uint32_t - Time interval between transmissions transmitted by the gateway (milliseconds)
valCal     - uint32_t - Adjust the analog reading to the new scale
valRes     - uint32_t - Recording the epoch time before entering sleep mode
calLow     - float    - Lower limit of sensor calibration
calUpp     - float    - Upper limit of sensor calibration
getBandgap - float    - Arduino's bandgap voltage function
VCCsnr     - float    - Sensor voltage
VCCsupp    - float    - Voltage at the VCC pin of the board
VCCsupp    - float    - Voltage at the VCC pin of the board
*/
