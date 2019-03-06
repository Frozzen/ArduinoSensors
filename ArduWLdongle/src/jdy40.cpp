#include <Arduino.h>
//#include <AltSoftSerial.h>
#include <SoftwareSerial.h>
#define AltRATE_START 9600
#define AltRATE 19200

#define ALT_RS232_RX 8
#define ALT_RS232_TX 9
#define JDY_40_SET 7


//AltSoftSerial altSerial;
SoftwareSerial altSerial(ALT_RS232_RX, ALT_RS232_TX);

// установили канал скоррость режим
void setupJDY_40()
{
  pinMode(JDY_40_SET, OUTPUT);
#ifdef FISRT_INIT
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
  digitalWrite(JDY_40_SET, LOW);
  delay(100);
  altSerial.print("AT+BAUD\r\n");
  delay(100);
  Serial.print(altSerial.readString());
  altSerial.print("AT+RFC\r\n");
  delay(100);
  Serial.print(altSerial.readString());
  delay(100);
  digitalWrite(JDY_40_SET, HIGH);
  altSerial.begin(AltRATE);
}
