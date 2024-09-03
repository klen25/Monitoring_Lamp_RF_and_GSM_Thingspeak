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

#define pinArus   A1
#define pinTeg    A0
#define pinRelay1 A2
#define pinRelay2 A3

#define CL 1

// ----------------- SENSOR -------------
float NilaiArus, NilaiTegangan;
String Arus, Tegangan;
// Tegangan
int z,zm,zk,zs;
int i,k,m,s;
unsigned int adcv;
int values[300];
float vac;
// Arus
int acs_ref_data = 512;
int acs_data = 512;
int acs_data_max = acs_ref_data;
int acs_data_min = acs_ref_data;
 
unsigned long delay_now = 0;
unsigned long delay_last = 0;
unsigned long delay_limit = 1000;


// ----------------- FLAG ---------------
int flag_reset;
byte flag_ON, flag_OFF;

// ------------------- RF ---------------
int len;
String buffer = "";

// ---------------- TIMER ---------------
#define TimerDemo 50000 // DEMO ON/OFF selama 50s
unsigned long TimerON, TimerOFF, currentMillis, previousMillis = 0;
const long interval = 1000;
int DETIK, MENIT, menit_old, JAM;

void setup()
{
  Serial.begin(9600);
  
  pinMode(pinArus, INPUT);
  pinMode(pinTeg, INPUT);
  pinMode(pinRelay1, OUTPUT);
  pinMode(pinRelay2, OUTPUT);
  digitalWrite(pinRelay1, LOW);
  digitalWrite(pinRelay2, LOW);

}

void(* reset_nano) (void)= 0;

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
      menit_old = MENIT;
      MENIT++;

      if (MENIT >= 60)
      {
        MENIT = 0;
        JAM++;
        if (JAM == 25)
        {
          JAM = 0;
        }
      }
    }
    readtegangan();
    readarus();

  }

  if (Serial.available() > 0)
  {
    buffer = Serial.readString();
    len = buffer.length();
    CekInput();
  }

  if (flag_reset == 1)
  {
    delay(2000);
    reset_nano();
  }
  if((flag_ON == 1) and (MENIT != menit_old) and (DETIK % TimerDemo == 0))
  { digitalWrite(pinRelay1, LOW); TimerON = 0; flag_ON = 0; }
  if((flag_OFF == 1) and (MENIT != menit_old) and (DETIK % TimerDemo == 0))
  { TimerOFF = 0; flag_OFF = 0; }
}

void CekInput()
{
  if((buffer[len-8] == 'C') and (buffer[len-7] == 'L') and (int(buffer[len-6]) - 48 == CL))
  {
    if((buffer[len-5] == 'R') and (buffer[len-4] == 'E') and (buffer[len-3] == 'Q'))
    { delay(1000); SendRF(); }
    if((buffer[len-5] == 'O') and (buffer[len-4] == 'N') and (buffer[len-3] == 'N'))
    { TimerON = millis();  flag_ON = 1; digitalWrite(pinRelay1, HIGH); }
    if((buffer[len-5] == 'O') and (buffer[len-4] == 'F') and (buffer[len-3] == 'F'))
    { TimerOFF = millis(); flag_OFF = 1; digitalWrite(pinRelay1, LOW); }
  }
}

void SendRF()
{
  readarus();
  readtegangan();
  delay(1000);

  char dataa[] = "";

  sprintf(dataa, "*CL%d|%s|%s", CL, Tegangan.c_str(), Arus.c_str()); 
  Serial.println(dataa); // buffer = variabel data, len = panjang data
}

void readarus()
{
  char buffArus[5] = "0";

    acs_data = analogRead(pinArus);
    // nilai tertinggi
    if(acs_data > acs_data_max){
      acs_data_max = acs_data;
    }
    // nilai terendah
    if(acs_data < acs_data_min){
      acs_data_min = acs_data;
    }
//    
//      Serial.print("Min: ");
//      Serial.println(acs_data_min);
//      
//      Serial.print("Max: ");
//      Serial.println(acs_data_max);

//      Serial.print("Fix: ");
      float yy = (((acs_data_max - acs_data_min)/1024.0)*5000)/1000;
//      Serial.println(yy);
      delay_last += delay_limit;
      Arus = dtostrf(yy, 2, 2, buffArus);
 
      acs_data_min = acs_ref_data;
      acs_data_max = acs_ref_data;
}

void readtegangan()
{
  char buffTeg[5] = "0";

  for(i=0;i<300;i++) {
  values[i] = analogRead (A0);
  if (values[i] >= z) {
  z = values[i];
  }
  }
  
  for(k=0;k<300;k++) {
  values[k] = analogRead (A0);
  if (values[k] >= zk) {
  zk = values[k];
  }
  }
  
  for(m=0;m<300;m++) {
  values[m] = analogRead (A0);
  if (values[m] >= zm) {
  zm = values[m];
  }
  }
  
  for(s=0;s<300;s++) {
  values[s] = analogRead (A0);
  if (values[s] >= zs) {
  zs = values[s];
  }
  }
  
  if((z >= zk)&&(z >= zm)&&(z >= zs)){
  adcv = z;
  vac = (z-512.30)/0.3673;
  }
  if((zk >= z)&&(zk >= zm)&&(zk >= zs)){
  adcv = zk;
  vac = (zk-512.30)/0.3673;
  }
  if((zm >= z)&&(zm >= zk)&&(zm >= zs)){
  adcv = zm;
  vac = (zm-512.30)/0.3673;
  }
  if((zs >= z)&&(zs >= zk)&&(zs >= zm)){
  adcv = zs;
  vac = (zs-512.30) / 0.3673;
  }
  
  float fix = (vac*0.7071)-20;
  Tegangan = dtostrf(fix, 2, 2, buffTeg);
//  Serial.print("Tegangan = "); Serial.println(Tegangan);

  //kembali ke nilai awal = 0
  z = 0;
  zk = 0;
  zm = 0;
  zs = 0;
}
