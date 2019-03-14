#include <Arduino.h>
#include <SoftwareSerial.h>
#define AltRATE 19200

#define rX 8
#define tX 9
#define JDY_40_SET 7


//AltSoftSerial altSerial;
SoftwareSerial altSerial(rX, tX);
static void getConfig(const char*str)
{
  delay(500);  
char buf[20] = {0};
  while(*str){
    delayMicroseconds(580);
    altSerial.write(*str++);
  }
  Serial.print(">");
  while(!altSerial.available());
  Serial.print(".");
  size_t ix = altSerial.readBytesUntil('\n', buf, sizeof(buf));
  Serial.write(buf, ix); 
  Serial.println("<");
}
// установили канал скоррость режим
void setupJDY_40()
{
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
  altSerial.begin(19200);
  // отчет о текущий конфигурации
  digitalWrite(JDY_40_SET, LOW);
  getConfig("AT+BAUD\r\n");
  getConfig("AT+RFID\r\n");
  getConfig("AT+DVID\r\n");
  getConfig("AT+RFID\r\n");
  getConfig("AT+RFC\r\n");
  getConfig("AT+POWE\r\n");
  getConfig("AT+CLSS\r\n");
  return;
  digitalWrite(JDY_40_SET, HIGH);
}
