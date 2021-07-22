#include <SPI.h>
#include <RH_RF95.h>

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

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("Recevier starting...");
 
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
}
 
void loop()
{
  if (rf95.available())
  {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
 
    if (rf95.recv(buf, &len))
    {
      Serial.println((char*)buf);
    }
    else
    {
      Serial.println("Receive failed");
    }
  }
}
