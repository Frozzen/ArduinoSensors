#include <Arduino.h>
#include <SoftwareSerial.h>
#define AltRATE 19200

#define ALT_RS232_RX 8
#define ALT_RS232_TX 9
#define JDY_40_SET 7


//AltSoftSerial altSerial;
SoftwareSerial altSerial(ALT_RS232_RX, ALT_RS232_TX);
static void getConfig(const char*str)
{
char buf[20] = {0};
  altSerial.print(str);
  for(uint8_t i =0; i <2; ++i) {
    while(!altSerial.available());
    altSerial.readBytesUntil('\n', buf, sizeof(buf));
    Serial.println(buf); Serial.print(":");
  }
}
// установили канал скоррость режим
void setupJDY_40()
{
  pinMode(ALT_RS232_RX, INPUT);
  pinMode(ALT_RS232_TX, OUTPUT);
  pinMode(JDY_40_SET, OUTPUT);
#ifdef FISRT_INIT
#define AltRATE_START 9600
  altSerial.begin(AltRATE_START);
  digitalWrite(JDY_40_SET, HIGH);
  digitalWrite(JDY_40_SET, LOW);

  delay(100);
  altSerial.print("AT+BAUD6\r\n");
  delay(100);
  Serial.print(altSerial.readString());
  altSerial.print("AT+RFC001\r\n");
  delay(100);
  Serial.print(altSerial.readString());
  delay(100);
  digitalWrite(JDY_40_SET, HIGH);
#endif
  altSerial.begin(AltRATE);
  // отчет о текущий конфигурации
  digitalWrite(JDY_40_SET, LOW);
  return;
  delay(500);
  getConfig("AT+BAUD\n");
  getConfig("AT+RFID\n");
  getConfig("AT+DVID\n");
  getConfig("AT+RFID\n");
  getConfig("AT+RFC\n");
  getConfig("AT+PWOE\n");
  getConfig("AT+CLSS\n");
  digitalWrite(JDY_40_SET, HIGH);
}
