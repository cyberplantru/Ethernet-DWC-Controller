char auth[] = "YourAuthToken"; // You should get Auth Token in the Blynk App.
byte arduino_mac[] = { 0x90, 0xA2, 0xDA, 0x20, 0xD6, 0x23 }; // You should get MAC address generated by https://ssl.crox.net/arduinomac/
/*
  Example code for the pH to I2C module v2.0
  
  http://www.cyber-plant.com
  by CyberPlant LLC, 24 December 2015
  This example code is in the public domain.
*/

#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <BlynkSimpleEthernet.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DS18B20.h>
#define pumpP1 5           // pump P1
#define pumpP2 6           // pump P2
#define pumpP3 7           // pump P3
#define X0 0.0
#define X1 2.0
#define X2 12.88
#define X3 80.0
#define alphaLTC 0.022 // The linear temperature coefficient

const byte SLAVE_ADDRESS = 9;

WidgetLED lan(V15);
WidgetLED led1(V16);

float A;
float B;
float C;

long pulseCount = 0;  //a pulse counter variable
long pulseCal = 0;
byte ECcal = 0;

unsigned long pulseTime, lastTime, duration, totalDuration;

unsigned int Interval = 1000;
long previousMillis = 0;
unsigned long Time;

float EC;
float temp = 25.0;
float tempManual = 25.0;
byte sensorLevel;

int sequence = 0;

int s = -1; // second

int tube = 55;
int z = 0;
float itrue;

int flag = 0;

const byte ONEWIRE_PIN = A3;
byte address[8];
OneWire onewire(ONEWIRE_PIN);
DS18B20 sensors(&onewire);

long Y0;
long Y1;
long Y2;
long Y3;
float EClow;
float EChigh;
float doseP1;
float doseP2;
float doseP3;
float LogP1;
float LogP2;
float LogP3;
int ECdelay;
int ECMode;
int Lock;

void setup()
{
  Blynk.begin(auth, "cloud.blynk.cc", 8442, arduino_mac);
  /*
  while (Blynk.connect() == false) {
    // Wait until connected
  }
  */
  Wire.begin(SLAVE_ADDRESS);
  Wire.onRequest(requestEvent); // register event
  Time = millis();
  pinMode(8, OUTPUT); // Relay
  pinMode(2, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
  sensors.begin();
  Search_sensors();
  Read_EE();
}

struct MyObject {
  long Y0;
  long Y1;
  long Y2;
  long Y3;
  float EClow;
  float EChigh;
  float doseP1;
  float doseP2;
  float doseP3;
  float LogP1;
  float LogP2;
  float LogP3;
  int ECdelay;
  int ECMode;
  int Lock;
};

void Read_EE()
{
  int eeAddress = 0;

  MyObject customVar;
  EEPROM.get(eeAddress, customVar);

  Y0 = (customVar.Y0);
  Y1 = (customVar.Y1);
  Y2 = (customVar.Y2);
  Y3 = (customVar.Y3);
  EClow = (customVar.EClow);
  EChigh = (customVar.EChigh);
  doseP1 = (customVar.doseP1);
  doseP2 = (customVar.doseP2);
  doseP3 = (customVar.doseP3);
  LogP1 = (customVar.LogP1);
  LogP2 = (customVar.LogP2);
  LogP3 = (customVar.LogP3);
  ECdelay = (customVar.ECdelay);
  ECMode = (customVar.ECMode);
  Lock = (customVar.Lock);
}
/*---------------------------------------Buttons------------------------------------------*/

BLYNK_WRITE(V21) // Locking EC calibration and nulling a logs
{
  Lock = param.asInt();
  SaveSet();
}

BLYNK_WRITE(V4) // Auto mode / manual
{
  ECMode = param.asInt();
  SaveSet();
}

/*---------------------------------------I/O of Pumps and nulling of Logs------------------------------------------*/

BLYNK_WRITE(V5) // Pump 1
{
  if (param.asInt() == 1 && Lock == 0)
  {
    LogP1 = 0;
  }
  else if (param.asInt() == 1 && Lock == 1)
  {
    PumpP1_On();
  }
  SaveSet();
}

BLYNK_WRITE(V6) // Pump 2
{
  if (param.asInt() == 1 && Lock == 0)
  {
    LogP2 = 0;
  }
  else if (param.asInt() == 1 && Lock == 1)
  {
    PumpP2_On();
  }
  SaveSet();
}

BLYNK_WRITE(V7) // Pump 3
{
  if (param.asInt() == 1 && Lock == 0)
  {
    LogP3 = 0;
  }
  else if (param.asInt() == 1 && Lock == 1)
  {
    PumpP3_On();
  }
  SaveSet();
}

/*---------------------------------------Auto EC correction setting------------------------------------------*/

BLYNK_WRITE(V8) // set high EC
{
  EChigh = (param.asDouble() / 10);
  z = 1;
}

BLYNK_WRITE(V9) // set low EC
{
  EClow = (param.asDouble() / 10);
  z = 2;
}

BLYNK_WRITE(V10) // set an interval of correction
{
  ECdelay = param.asInt();
  z = 3;
}

/*---------------------------------------Dosage for pumps------------------------------------------*/

BLYNK_WRITE(V11)
{
  doseP1 = param.asDouble() / 10;
  z = 4;
}

BLYNK_WRITE(V12)
{
  doseP2 = param.asDouble() / 10;
  z = 5;
}

BLYNK_WRITE(V13)
{
  doseP3 = param.asDouble() / 10;
  z = 6;
}

/*---------------------------------------I/O Relay------------------------------------------*/

BLYNK_WRITE(V17)
{
  byte r = param.asInt();
  digitalWrite(8, r);
}

/*---------------------------------------Calibration of EC sensor------------------------------------------*/

BLYNK_WRITE(V18) // Calibrate 2.00 uS
{
  if (Lock == 0 && flag == 0)
  {
    flag = 1;
    cal_1();
    flag = 0;
  }
  else if (Lock == 0 && flag == 1)
  {
    flag = 0;
  }
}

BLYNK_WRITE(V19)  // Calibrate 12.88 uS
{
  if (Lock == 0 && flag == 0)
  {
    flag = 1;
    cal_2();
    flag = 0;
  }
  else if (Lock == 0 && flag == 1)
  {
    flag = 0;
  }
}

BLYNK_WRITE(V20)
{
  if (Lock == 0 && flag == 0)
  {
    flag = 1;
    Reset_EC();
    flag = 0;
  }
  else if (Lock == 0 && flag == 1)
  {
    flag = 0;
  }
}

/*------------------------------------------Read sensors-------------------------------*/

void temp_read()
{
  sensors.request(address);
  while (!sensors.available());
  temp = sensors.readTemperature(address);
}

void ECread()  //graph function of read EC
{
  if (pulseCal > Y0 && pulseCal < Y1 )
  {
    A = (Y1 - Y0) / (X1 - X0);
    B = Y0 - (A * X0);
    C = (pulseCal - B) / A;
  }

  if (pulseCal > Y1 && pulseCal < Y2 )
  {
    A = (Y2 - Y1) / (X2 - X1);
    B = Y1 - (A * X1);
    C = (pulseCal - B) / A;
  }
  if (pulseCal > Y2)
  {
    A = (Y3 - Y2) / (X3 - X2);
    B = Y2 - (A * X2);
    C = (pulseCal - B) / A;
  }

  EC = (C / (1 + alphaLTC * (temp - 25.00)));
}

void onPulse() // EC pulse counter
{
  pulseCount++;
}

void Search_sensors() // search ds18b20 temperature sensor
{
  address[8];

  onewire.reset_search();
  while (onewire.search(address))
  {
    if (address[0] != 0x28)
      continue;

    if (OneWire::crc8(address, 7) != address[7])
    {
      temp = 25.0;
      break;
    }
  }
}

void cal_1()
{
  ECcal = 1;
  Blynk.virtualWrite(V0, "Cal.");
  Y1 = pulseCal / (1 + alphaLTC * (temp - 25.00));
  ECread();
  while (EC > X1)
  {
    Y1++;
    ECread();
  }
  SaveSet();
  ECcal = 0;
}

void cal_2()
{
  ECcal = 1;
  Blynk.virtualWrite(V0, "Cal.");
  Y2 = pulseCal / (1 + alphaLTC * (temp - 25.00));
  ECread();
  while (EC > X2)
  {
    Y2++;
    ECread();
  }
  SaveSet();
  ECcal = 0;
}

void Reset_EC()
{
  ECcal = 1;
  Blynk.virtualWrite(V0, "Res.");

  Y0 = 220;
  Y1 = 1245;
  Y2 = 5282;
  Y3 = 17255;

  SaveSet();
  ECcal = 0;
}

void requestEvent()
{

  struct
  {
    float temp;
    byte sensorLevel;
  } response;

  response.temp = temp;
  response.sensorLevel = sensorLevel;

  Wire.write ((byte *) &response, sizeof response);
}  // end of receiveEvent

void SaveSet()
{
  int eeAddress = 0;
  MyObject customVar = {
    Y0,
    Y1,
    Y2,
    Y3,
    EClow,
    EChigh,
    doseP1,
    doseP2,
    doseP3,
    LogP1,
    LogP2,
    LogP3,
    ECdelay,
    ECMode,
    Lock
  };
  EEPROM.put(eeAddress, customVar);
}

void PumpP1_On()
{
  pulseCount = pulseCal;
  digitalWrite (pumpP1, HIGH);
  delay (doseP1 * 60000 / tube);
  digitalWrite (pumpP1, LOW);
  LogP1 = (LogP1 + doseP1);
}
void PumpP2_On()
{
  pulseCount = pulseCal;
  digitalWrite (pumpP2, HIGH);
  delay (doseP2 * 60000 / tube);
  digitalWrite (pumpP2, LOW);
  LogP2 += doseP2;
}
void PumpP3_On()
{
  pulseCount = pulseCal;
  digitalWrite (pumpP3, HIGH);
  delay (doseP3 * 60000 / tube);
  digitalWrite (pumpP3, LOW);
  LogP3 = (LogP3 + doseP3);
}

void loop()
{
  if (ECcal == 0)
  {
    if (millis() - Time >= Interval)
    {
      Time = millis();
      if (lan.getValue()) {
        lan.off();
        BLYNK_LOG("LED1: off");
      } else {
        lan.on();
        BLYNK_LOG("LED1: on");
      }
      s++;
      sequence ++;
      if (sequence > 1)
        sequence = 0;

      if (sequence == 0)
      {
        pulseCount = 0; //reset the pulse counter
        attachInterrupt(0, onPulse, RISING); //attach an interrupt counter to interrupt pin 1 (digital pin #3) -- the only other possible pin on the 328p is interrupt pin #0 (digital pin #2)
      }
      if (sequence == 1)
      {
        detachInterrupt(0);
        pulseCal = pulseCount;
        temp_read();
        if (temp > 200 || temp < -20 )
        {
          temp = tempManual;

          Search_sensors();
        }

        sensorLevel = digitalRead(A2);
        led1.setValue(map (sensorLevel, 0, 1, 255, 0));
        ECread();

        Blynk.virtualWrite(V0, EC);
        Blynk.virtualWrite(V14, temp);
        Blynk.virtualWrite(V1, LogP1);
        Blynk.virtualWrite(V2, LogP2);
        Blynk.virtualWrite(V3, LogP3);

        if (z != 0)
        {
          Blynk.virtualWrite(V2, " ");
          Blynk.virtualWrite(V3, " ");
        }
        if (z == 1)
        {
          Blynk.virtualWrite(V0, EChigh);
          Blynk.virtualWrite(V1, "uS");
          SaveSet();
          z = 0;
        }
        if (z == 2)
        {
          Blynk.virtualWrite(V0, EClow);
          Blynk.virtualWrite(V1, "uS");
          SaveSet();
          z = 0;
        }
        if (z == 3)
        {
          Blynk.virtualWrite(V0, ECdelay);
          Blynk.virtualWrite(V1, "min");
          SaveSet();
          z = 0;
        }
        if (z == 4)
        {
          Blynk.virtualWrite(V0, doseP1);
          Blynk.virtualWrite(V1, "ml");
          SaveSet();
          z = 0;
        }
        if (z == 5)
        {
          Blynk.virtualWrite(V0, doseP2);
          Blynk.virtualWrite(V1, "ml");
          SaveSet();
          z = 0;
        }
        if (z == 6)
        {
          Blynk.virtualWrite(V0, doseP3);
          Blynk.virtualWrite(V1, "ml");
          SaveSet();
          z = 0;
        }
      }

      if ((s / 60) >= ECdelay)
      {
        if (EC < EClow && EChigh > EClow && EC > 0 && ECMode == 1)
        {
          ECcal = 1;
          PumpP1_On();
          PumpP2_On();
          PumpP3_On();
          SaveSet();
          ECcal = 0;
        }
        if (EC > EChigh  && EChigh > EClow && sensorLevel == 0 && ECMode == 1)
        {
          ECcal = 1;
          pulseCount = pulseCal;
          while ( sensorLevel == 0)
          {
            digitalWrite(8, HIGH);
          }
          digitalWrite(8, LOW);
          ECcal = 0;
        }
        s = 0; // nulling timer
      }
    }
  }
  Blynk.run();
}

/*-----------------------------------End loop---------------------------------------*/


