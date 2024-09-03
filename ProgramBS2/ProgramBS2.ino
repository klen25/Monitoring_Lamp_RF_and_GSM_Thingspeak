/*
 *               MONITORING LAMPU PJU
 * ------------------------------------------------------
 *                    Spesifikasi
 * MCU                : Arduino Nano
 * Sensor Arus AC     : ACS712
 * Sensor Tegangan AC : ZMPT101B
 * I/O                : Relay
 * Komunikasi Modem   : SIM800L
 * Komunikasi RF      : CC1101 TransCeiver
 * 
 *                      PinOut
 * Arus AC            : Pin A1, 0A = 2,5V ; 185mV/A
 * Tegangan AC        : Pin A0, 
 * Relay 1            : Pin A2
 * Relay 2            : Pin A3
 * Modem              : RX, TX
 * RF                 : Pin SPI
 *                      - CSN   D10
 *                      - SI    D11
 *                      - SO    D12
 *                      - SCK   D13
 *                      - GD0   D2
 */

#include <SPI.h>
#include "ELECHOUSE_CC1101.h"
#include "gprs.h"

int pinArus =  A1;
int pinTeg =   A0;
#define pinRelay1 A2
#define pinRelay2 A3

#define Master   0 // Master = 0; Slave = selain 0
#define MaxSlave 2 // Maximum Master & Slave

#define BS 0

#define TIMEOUT  5000
GPRS gprs;

// --------------- VARIABEL ------------
// ---------------- MODEM ---------------
String inputan = "";
bool inputComplete;

// ------------------- RF ---------------
byte buffer[61] = {0};
int repeat, CL;
const int len = 15;
byte Request[len] = "";
int timeout = 0;

// ----------------- FLAG ---------------
bool flagCPINout, flagCPAS = 0;
int flagARS, flag_detik, flag_menit, flag_reset, flagReq;
//byte flag_report;
//bool flag_timer_report;
byte flag_ON, flag_OFF;

// ----------------- TIMER --------------
#define TimerDemo 50000 // DEMO ON/OFF selama 50s
unsigned long currentMillis;
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 1000;              // interval at which to blink (milliseconds)
int DETIK, MENIT, JAM;
unsigned long TimerON, TimerOFF;
int JAM_old, menit_old;

// ----------------- GLOBAL -----------
int kode, kodex, menit_mod;
int gvARS = 1;

// ----------------- SENSOR -------------
double NilaiArus, NilaiTegangan;
float NilaiArus1, NilaiTegangan1;
float NilaiArus2, NilaiTegangan2;
String Arus, Tegangan;

void setup()
{
  Serial.begin(9600);
  SPI.begin();
  ELECHOUSE_cc1101.Init(F_433);    // Set Frequensi - F_433, F_868, F_965 MHz
  ELECHOUSE_cc1101.SetReceive();   //
  
  pinMode(pinArus, INPUT);
  pinMode(pinTeg, INPUT);
  pinMode(pinRelay1, OUTPUT);
  pinMode(pinRelay2, OUTPUT);
  digitalWrite(pinRelay1, LOW);
  digitalWrite(pinRelay2, LOW);

  delay(5000);
//  Serial.println("BASE READY");
//  modem_init();

  gprs.preInit(); delay(1000);

  while(0 != gprs.init()) { delay(1000); }
  
//Set SMS mode ke mode ASCII
  if(0 != gprs.sendCmdAndWaitForResp("AT+CMGF=1\r\n", "OK", TIMEOUT)) { ERROR("ERROR:CNMI"); return; }
   
//mulai baca ada indikasi sms baru
  if(0 != gprs.sendCmdAndWaitForResp("AT+CNMI=2,2,0,0,0\r\n", "OK", TIMEOUT)) { ERROR("ERROR:CNMI"); return; }
}

void(* reset_nano) (void)= 0;

char currentLine[500] = "";
int currentLineIndex = 0;
 
bool nextLineIsMessage = false;

void loop()
{
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    
    DETIK++;
    if (DETIK == 60)
    {
      DETIK = 0;
      MENIT++;

      if (MENIT >= 60)
      {
        MENIT = 0;
        JAM++;
        if (JAM == 25)
        {
//          flag_reset = 1; // Jika ingin reset tiap jam 12 malam
          JAM = 0;
        }
      }
    }
//    Serial.print("Tegangan AC = "); Serial.println(Tegangan);
//    Serial.print("Arus AC     = "); Serial.println(Arus);
  }

  readarus();
  readtegangan();
  
  if(DETIK % 15 == 0)
  { digitalWrite(pinRelay1, ! digitalRead(pinRelay1)); }
  
  if (flag_reset == 1)
  { delay(2000); reset_nano(); }
  
  if (gvARS == 0) menit_mod = 5;
  else if (gvARS == 1) menit_mod = 15;
  else if (gvARS == 2) menit_mod = 30;
  else if (gvARS >= 3) menit_mod = 60;

  if ((MENIT != menit_old) and (MENIT % menit_mod == 0))
  {
    SendGPRS(0); delay(5000);
    ReqALLCL(); delay(1000);
    SendGPRS(1); delay(5000);
    SendGPRS(2); delay(5000);
    menit_old = MENIT;
  }

//  if((flag_ON == 1) and (currentMillis - TimerON == TimerDemo)) { digitalWrite(pinRelay1, LOW); TimerON = 0; flag_ON = 0; }
//  if((flag_OFF == 1) and (currentMillis - TimerOFF == TimerDemo)) { TimerOFF = 0; flag_OFF = 0; }
  if((flag_ON == 1) and (MENIT != menit_old) and (DETIK % TimerDemo == 0)) { digitalWrite(pinRelay1, LOW); TimerON = 0; flag_ON = 0; }
  if((flag_OFF == 1) and (MENIT != menit_old) and (DETIK % TimerDemo == 0)) { TimerOFF = 0; flag_OFF = 0; }

  CekSMSMasuk();
}

void CekSMSMasuk()
{
  if(gprs.serialSIM800.available())  //jika ada data serial terbaca
  {
    char lastCharRead = gprs.serialSIM800.read();
    if(lastCharRead == '\r' || lastCharRead == '\n')//baca karakter dari serial
    {
      String isisms = String(currentLine);
      if(isisms.startsWith("+CMT:"))//jika baris terakhir adalah +CMT, ada indikasi sms baru masuk. 
      {
//        Serial.println(lastLine);
        nextLineIsMessage = true;
      }
      else if (isisms.length() > 0)
      {
        if(nextLineIsMessage) //disini tempat pembacaan sms oleh sim800, kontrol sms ada disini
        {
//          Serial.println(lastLine);
          int n = isisms.length();
          if((isisms[n-6] == 'B') and (isisms[n-5] == 'S') and (int(isisms[n-4]) - 48 == BS))
          {
//          gprs.serialSIM800.print(isisms[n-3]);
//          gprs.serialSIM800.print(isisms[n-2]);
//          gprs.serialSIM800.print(isisms[n-1]);
//          gprs.serialSIM800.print("\r\n");
            if((isisms[n-3] == 'R') and (isisms[n-2] == 'E') and (isisms[n-1] == 'Q'))
            { readarus(); readtegangan(); delay(2000); SendGPRS(0); }
            if((isisms[n-3] == 'O') and (isisms[n-2] == 'N')  and (isisms[n-1] == 'N'))
            { TimerON = millis();  flag_ON = 1;  digitalWrite(pinRelay1, HIGH); }
            if((isisms[n-3] == 'O') and (isisms[n-2] == 'F')  and (isisms[n-1] == 'F'))
            { TimerOFF = millis(); flag_OFF = 1; digitalWrite(pinRelay1, LOW); }
            
          }
          if((isisms[n-6] == 'A') and (isisms[n-5] == 'L') and (isisms[n-4] == 'L'))
          {
            SendGPRS(0); delay(5000);
          }
          else
          {
            isisms.getBytes(Request, len);
            ELECHOUSE_cc1101.SendData(Request, len); //ELECHOUSE_cc1101.SendData(byte *txBuffer,byte size)
          }          
          nextLineIsMessage = false;
        }
      }
      for( int i = 0; i < sizeof(currentLine);  ++i )//Clear char array for next line of read
      { currentLine[i] = (char)0; }
      currentLineIndex = 0;
    }
    else
    { currentLine[currentLineIndex++] = lastCharRead; }
  }
}

void callCL()
{
  sprintf(Request, "BS%dCL%dREQ", BS, CL);
  ELECHOUSE_cc1101.SendData(Request, len); //ELECHOUSE_cc1101.SendData(byte *txBuffer,byte size)
  
  //-------------------------------------------- RF GET DATA FROM CLIENT
  delay(1000);
  ELECHOUSE_cc1101.SetReceive();
  while (!ELECHOUSE_cc1101.CheckReceiveFlag())
  {
    timeout++;
    if(timeout >= 5000) { repeat++; break; }
  }
  if (ELECHOUSE_cc1101.CheckReceiveFlag())
  {
    int len = ELECHOUSE_cc1101.ReceiveData(buffer);
    buffer[len] = '\0';
//      gprs.println((char *) buffer);
    repeat = 4;
  }  
}

void ReqCL(byte i)
{
  repeat = 0;

  if (repeat <= 3)
  { timeout = 0; callCL();
  }
  else 
  { repeat = 0; }    
}

void ReqALLCL()
{
  const int len = 15;
  byte Request[len] = "";
  for (CL = 1; CL <= MaxSlave; CL++)
  {
    if (repeat >= 3)
    {
      CL += 1;
      if (CL <= 0) CL = 1;
      repeat = 0;
    }
    if ((CL <= 0) or (CL > MaxSlave+1)) CL = 1;
    callCL();
  }
}

void SendGPRS(byte i)
{
  String str = "";
  switch(i)
  {
    case 0 :
      str = "api.thingspeak.com/update?api_key=0LUG7U53GWVHQ5YG&field1=" + String(NilaiTegangan);//+ "&field2=" + String(NilaiArus);
      break;
    case 1 :
      str = "api.thingspeak.com/update?api_key=0LUG7U53GWVHQ5YG&field3=" + String(NilaiTegangan1);//+ "&field4=" + String(NilaiArus1);
      break;
    case 2 :
      str = "api.thingspeak.com/update?api_key=0LUG7U53GWVHQ5YG&field5=" + String(NilaiTegangan2);//+ "&field6=" + String(NilaiArus2);
      break;
  }
  
  gprs.serialSIM800.print("AT+CREG?\r\n");
  delay(1000);

  gprs.serialSIM800.print("AT+CGATT?\r\n");
  delay(1000);

  gprs.serialSIM800.print("AT+CIPSHUT\r\n");
  delay(1000);

  gprs.serialSIM800.print("AT+CIPSTATUS\r\n");
  delay(2000);

  gprs.serialSIM800.print("AT+CIPMUX=0\r\n");
  delay(2000);
 
  gprs.serialSIM800.print("AT+CSTT=\"m2minternet\"\r\n");//start task and setting the APN,
  delay(1000);
 
  gprs.serialSIM800.print("AT+CIICR\r\n");//bring up wireless connection
  delay(3000);
 
  gprs.serialSIM800.print("AT+CIFSR\r\n");//get local IP adress
  delay(2000);
 
  gprs.serialSIM800.print("AT+CIPSPRT=0\r\n");
  delay(3000);

  gprs.serialSIM800.print("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"\r\n");//start up the connection
  delay(6000);
 
  gprs.serialSIM800.print("AT+CIPSEND=80\r\n");//begin send data to remote server
  delay(4000);
  
  gprs.serialSIM800.print(str);//begin send data to remote server
  delay(4000);

  gprs.serialSIM800.print((char)26); //sending
  gprs.serialSIM800.print('\r');
  delay(5000); //waitting for reply, important! the time is base on the condition of internet 
  gprs.serialSIM800.print('\r');
 
  gprs.serialSIM800.print("AT+CIPSHUT\r\n"); //close the connection
  delay(100);
}

void readarus()
{
  int sensitivity = 66;
  int offsetVoltage = 2500;
  char buffArus[5] = "0";
  float adcValue, Samples;

  for (int x = 0; x <= 50; x++)
  {
    Samples = analogRead(pinArus);
    adcValue = adcValue + Samples;
    delay(3);
  }

  adcValue = adcValue/50;
  float adcVoltage = (adcValue / 1024.0) * 5000;
  NilaiArus = ((adcVoltage - offsetVoltage) / sensitivity);
  if(NilaiArus < 1.3) NilaiArus = 0;
  Arus = dtostrf(NilaiArus, 2, 2, buffArus);
}

void readtegangan()
{
  char buffTeg[5] = "0";

  int adcValue = analogRead (pinTeg);
  float adcVoltage = (adcValue / 1024.0) * 5000;
  NilaiTegangan  = adcVoltage;
  Tegangan = adcValue;
//  Tegangan = dtostrf(adcValue, 2, 2, buffTeg);
}

//void waitrespon()
//{
//  inputan = "";
//  while(!gprs.available())
//  {}
//  while(gprs.available())
//  {
//    char charr = (char) gprs.read();
//    inputan += charr;
//  }
//  if(inputan != "") { inputComplete = true; }
//}

void modem_init()
{
  flagCPINout = 1;
//  gprs.println("Inisialisasi modem...");
//  for(int i = 0; i < 3; i++)
//  {
    gprs.serialSIM800.print("AT+CPIN=?\r\n"); delay(1000);
// //   waitrespon();
//    if(inputComplete)
//    {
//      for(int j = 0; j < inputan.length(); j++)
//      {
//        if((inputan[j - 4] == 'R') and (inputan[j - 3] == 'E') and
//        (inputan[j - 2] == 'A') and (inputan[j - 1] == 'D') and (inputan[i] == 'Y'))
//        { flagCPINout = 0; i = 200; }
//      }
//      inputComplete = false;
//    }
//    if (flagCPINout == 0) { i = 3; }
//  }
  
//  gprs.serialSIM800.print("AT+IPR=9600\r");
  gprs.sendCmdAndWaitForResp("AT+IPR=9600\r\n", "OK", TIMEOUT);
  delay(1000);
//  waitrespon();
  inputComplete = false;
  delay(1000);

  gprs.serialSIM800.print("ATE0\r\n");    //disable echo
  delay(1000);
//  waitrespon();
  inputComplete = false;
  delay(1000);

  gprs.serialSIM800.print("AT+CMGF=1\r\n");
  delay(100);
//  waitrespon();
  inputComplete = false;
  delay(1000);

  gprs.serialSIM800.print("AT+CNMI=2,2,0,0,0\r\n");
  delay(1000);
//  waitrespon();
  inputComplete = false;
  delay(500);

  gprs.serialSIM800.print("AT+COPS=2\r\n");
  delay(100);
//  waitrespon();
  inputComplete = false;
  delay(2500);

  gprs.serialSIM800.print("AT+CLTS=1\r\n");
  delay(100);
//  waitrespon();
  inputComplete = false;
  delay(1000);

  gprs.serialSIM800.print("AT+COPS=0\r\n");
  delay(100);
//  waitrespon();
  inputComplete = false;
  delay(10500);

  gprs.serialSIM800.print("AT+CLTS?\r\n");
  delay(100);
//  waitrespon();
  inputComplete = false;
  delay(1000);

  gprs.serialSIM800.print("AT+CGATT=1\r\n");
  delay(1000);
//  waitrespon();
  inputComplete = false;
  delay(500);

  gprs.serialSIM800.print("AT+CIPMUX=0\r\n");
  delay(1000);
//  waitrespon();
  inputComplete = false;
  delay(500);

  gprs.serialSIM800.print("AT+CREG=1\r\n");
  delay(1000);
//  waitrespon();
  inputComplete = false;
  delay(500);

  gprs.serialSIM800.print("AT&W\r\n");
  delay(1000);
//  waitrespon();
  inputComplete = false;
  delay(500);

//  flagCPAS = 0;
//  for (int j = 0; j < 10; j++)
//  {
    gprs.serialSIM800.print("AT+CPAS\r\n");
//    delay(1000);
////    waitrespon();
//    if (inputComplete)
//    {
//      for (int i = 0; i < inputan.length(); i++)
//      {
//        if ((inputan[i - 6] == 'C') and (inputan[i - 5] == 'P') and (inputan[i - 4] == 'A') and 
//        (inputan[i - 3] == 'S') and (inputan[i - 2] == ':') and (inputan[i - 1] == ' ') and (inputan[i] == '0'))
//        {
//          //gprs.serialSIM800.print(inputan);
//          //gprs.println("CPAS ok");
//          flagCPAS = 1;
//          i = 200;
//        }
//      }
//      //gprs.println(inputan);
//      // clear the string:
//      inputan = "";
//      inputComplete = false;
//      if (flagCPAS == 1) { j = 11; }
//      delay(200);
//    }
//  }

//-------------------------------------------- GET TIME MODEM
//  gprs.println("AT+CCLK?"); delay(150);
//  waitrespon();
//  if (inputComplete) {
//    //CCLK: "18/05/08,17:02:58
//    for (int i = 0; i < inputan.length(); i++)
//    {
//      if ((inputan[i - 3] == 'C') and (inputan[i - 2] == 'C') and (inputan[i - 1] == 'L') and (inputan[i] == 'K'))
//      {
//        //gprs.println(inputan);
//
//        JAM   = ((int)inputan[i + 13] - 48) * 10 + ((int)inputan[i + 14] - 48);
//        MENIT = ((int)inputan[i + 16] - 48) * 10 + ((int)inputan[i + 17] - 48);
//        DETIK = ((int)inputan[i + 19] - 48) * 10 + ((int)inputan[i + 20] - 48);
//
//        JAM_old = JAM;
//        i = 200;
//      }
//    }
//    //gprs.println(inputan);
//    inputan = "";
//    inputComplete = false;
//  }
}
