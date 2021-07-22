#include <SPI.h>
#include <RH_RF95.h>
#include <EEPROM.h>

/* for feather32u4
*/
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7

/* for feather m0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
*/

#define RF95_FREQ 433.0
RH_RF95 rf95(RFM95_CS, RFM95_INT);

#define vibrationSensorPin 2
#define reedSensorPin 3
#define motiondetectorSensorPin 5
#define ledPin 6
#define VBATPIN A9
int brightness = 1;  // LED fényesség
  
float maxVoltage = 4.2;

int vibrationSensorPinState;
int reedSensorPinState;
int motiondetectorSensorPinState;

long debouncingTime = 50;
volatile unsigned long lastDebouncingMicros;

long alertTls = 5;
volatile unsigned long lastAlertTime;

long sabotageTls = 5;
volatile unsigned long lastSabotageTime;

enum {
  OK = 0,
  ALERT = 1,
  SABOTAGE = 2
};

volatile bool alertStatus = false;
volatile bool sabotageStatus = false;

#define unitIdLength 12
#define unitIdAddress 0
String unitId;

unsigned int sendTimeMin = 5;  // két állapotküldés közti minimális idő
unsigned int sendTimeMax = 7;  // két állapotküldés közti maximális idő
unsigned long lastSendTime;    // utolsó küldés időpontja
unsigned long nextSendTime;    // következő küldés időpontja

void setup() {
  pinMode(reedSensorPin, INPUT);
  pinMode(motiondetectorSensorPin, INPUT);
  
  pinMode(vibrationSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(vibrationSensorPin), sensorAlert_ISR, RISING);
  
//  pinMode(setupButtonPin, INPUT_PULLUP);
//  attachInterrupt(digitalPinToInterrupt(setupButtonPin), config_ISR, RISING);

  pinMode(ledPin, OUTPUT);
  
  Serial.begin(115200);
  delay(2000);
  Serial.println("Remote sensore starting...");
  
  /**
   * Read ID from eeprom
   */
  unitId = eepromReadString(unitIdAddress, unitIdLength);
  
  /**
   * Setup LoRa
   */
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(100);
  Serial.println("Feather LoRa TX Test!");
  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
 
  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");
 
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: ");
  Serial.println(RF95_FREQ);
  
  rf95.setTxPower(23, false);
  
  //Setup BandWidth, option: 7800,10400,15600,20800,31250,41700,62500,125000,250000,500000
  //Lower BandWidth for longer distance.
//  rf95.setSignalBandwidth(125000);

  //Setup Coding Rate:5(4/5),6(4/6),7(4/7),8(4/8)
//  rf95.setCodingRate4(5); //5-8
  
  //Setup Spreading Factor (6 ~ 12)
//  rf95.setSpreadingFactor(7);

  /*
  //Different Combination for distance and speed examples: 
  Example 1: Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on. Default medium range
    rf95.setSignalBandwidth(125000);
    rf95.setCodingRate4(5);
    rf95.setSpreadingFactor(7);
  Example 2: Bw = 500 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on. Fast+short range
    rf95.setSignalBandwidth(500000);
    rf95.setCodingRate4(5);
    rf95.setSpreadingFactor(7);
  Example 3: Bw = 31.25 kHz, Cr = 4/8, Sf = 512chips/symbol, CRC on. Slow+long range
    rf95.setSignalBandwidth(31250);
    rf95.setCodingRate4(8);
    rf95.setSpreadingFactor(9);
  Example 4: Bw = 125 kHz, Cr = 4/8, Sf = 4096chips/symbol, CRC on. Slow+long range
    rf95.setSignalBandwidth(125000);
    rf95.setCodingRate4(8);
    rf95.setSpreadingFactor(12); 
  */
  
  Serial.println("============================================================");
}

void loop()
{
  String serialMessage;
  while (Serial.available())
  {
    char c = Serial.read();
    if (c != '\n')
    {
      serialMessage += c;
    }
    
    delay(2);
  }
  
  if (serialMessage == "setupmode")
  {
    bool setupStatus = true;
    digitalWrite(ledPin, HIGH);
  
    while (setupStatus)  // setup mód
    {
      sendConfiguration();
      delay(100);
      
      String serialMessageIn;
      while (Serial.available())
      {
        char c = Serial.read();
        if (c != '\n')
        {
          serialMessageIn += c;
        }
        
        delay(2);
      }

      if (serialMessageIn == "normalmode")
      {
        setupStatus = false;
      }
      else
      {
        String newUnitId = serialMessageIn;
        if (newUnitId.length() > 0)
        {
          newUnitId.trim();
          newUnitId.replace(" ", "");
          if (newUnitId.length() > unitIdLength)
          {
            newUnitId = newUnitId.substring(0, unitIdLength);
          }
          if (unitId != newUnitId)
          {
            unitId = newUnitId;
            eepromWriteString(unitIdAddress, unitId);
          }
        }
      }
      serialMessageIn = "";
    }

    serialMessage = "";
    digitalWrite(ledPin, LOW);
  }

  long actualTime = millis();
  
  // szabotázs szenzorok kiolvasása
  reedSensorPinState = digitalRead(reedSensorPin);
  motiondetectorSensorPinState = digitalRead(motiondetectorSensorPin);
  // van-e szabotázs valamelyik szenzor szerint
  if (reedSensorPinState || motiondetectorSensorPinState)
  {
    sabotageStatus = true;
    lastSabotageTime = actualTime;
  }
  
  // ha riasztás van és már lejárt a Tls
  if (alertStatus && ((actualTime - lastAlertTime) >= (alertTls * 1000)))
  {
    alertStatus = false;
  }

  // ha szabotázs van és már lejárt a Tls
  if (sabotageStatus && ((actualTime - lastSabotageTime) >= (sabotageTls * 1000)))
  {
    sabotageStatus = false;
  }

  // elérkezett-e a küldési időpontja
  if ((lastSendTime > actualTime) || (actualTime >= nextSendTime))  // ha a belső óra átpördült vagy itt az idő a küldésre
  {
    analogWrite(ledPin, brightness);
    long startTime = millis();
    String payload = "Payload:" + getPayload();
    sendToRadio(payload);
    lastSendTime = millis();
    nextSendTime = lastSendTime + random(sendTimeMin * 1000, sendTimeMax * 1000);
//    Serial.print("Time: " + String(startTime)+ "\n");
    Serial.print("Unit ID: " + unitId + "\n");
    Serial.print("Alert state: " + String(alertStatus) + "\n");
    Serial.print("Sabotage state: " + String(sabotageStatus) + "\n");
    Serial.print("Battery voltage: " + String(measureBatteryVoltage()) + "\n");
//    Serial.print("Battery percentage: " + String(measureBatteryPercentage()) + "\n");
    Serial.print("Vibration sensor state: " + String(vibrationSensorPinState) + "\n");
    Serial.print("Reed sensor state: " + String(reedSensorPinState) + "\n");
    Serial.print("Motion sensor state: " + String(motiondetectorSensorPinState) + "\n");
//    Serial.print(payload + "\n");
//    Serial.print("Sending time: " + String(lastSendTime - startTime) + "msec"+ "\n");
//    Serial.print("Next send time: " + String(nextSendTime) + "\n");
    Serial.println("============================================================");
    digitalWrite(ledPin, LOW);
  }
}

void sensorAlert_ISR()
{
  if ((long)(micros() - lastDebouncingMicros) >= (debouncingTime * 1000))
  {
    alertStatus = true;
    lastDebouncingMicros = micros();
    lastAlertTime = millis();
  }
}

// Node configuration to send config software in setupmode
void sendConfiguration()
{
  String payload = "Configstatus:{\"unitId\":\"" + unitId + "\",\"unitIdLength\":\"" + String(unitIdLength) + "\",\"sendTimeMin\":\"" + String(sendTimeMin) + "\",\"RF95_FREQ\":\"" + String(RF95_FREQ) + "\",\"sendTimeMax\":\"" + String(sendTimeMax) + "\",\"alertTls\":\"" + String(alertTls) + "\",\"sabotageTls\":\"" + String(sabotageTls) + "\"}";
  Serial.print(payload + "\n");
}

String getPayload()
{
  //String payload = unitId + " " + String(alertStatus) + " " + String(sabotageStatus) + " "  + String(measureBatteryPercentage());
  String payload = "{\"unitId\":\"" + unitId + "\",\"alertStatus\":\"" + String(alertStatus) + "\",\"sabotageStatus\":\"" + String(sabotageStatus) + "\",\"measureBatteryPercentage\":\"" + String(measureBatteryPercentage()) + "\"}";
  return payload;
}

void sendToRadio(String payload)
{
  char copy[RH_RF95_MAX_MESSAGE_LEN];
  payload.toCharArray(copy, RH_RF95_MAX_MESSAGE_LEN);

  rf95.send(copy, sizeof(copy));
  delay(10);
  rf95.waitPacketSent();
}

float measureBatteryVoltage()
{
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;
  measuredvbat *= 3.3;
  measuredvbat /= 1024;
  return measuredvbat;
}

float measureBatteryPercentage()
{
  float percentage = (measureBatteryVoltage() / maxVoltage) * 100;
  
  return percentage;
}

String eepromReadString(char address, int readLength)
{
  char data[readLength];
  int len = 0;
  unsigned char k;
  k = EEPROM.read(address);
  while(k != 0 && len < readLength)
  {    
    k = EEPROM.read(address + len);
    data[len] = k;
    len++;
  }
  data[len] = 0;
  
  return String(data);
}

void eepromWriteString(char address, String data)
{
  int writeLength = data.length();
  int i;
  for (i = 0; i < writeLength; i++)
  {
    EEPROM.write(address + i, data[i]);
  }
  EEPROM.write(address + writeLength, 0);
}
