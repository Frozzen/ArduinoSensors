#include <Arduino.h>
#include <AltSoftSerial.h>

#define AltRATE_START 9600
#define AltRATE 19200

#define ALT_RS232_RX 8
#define ALT_RS232_TX 9
#define JDY_40_SET 7


AltSoftSerial altSerial;

// установили канал скоррость режим
void setupJDY_40()
{
  pinMode(JDY_40_SET, OUTPUT);
  altSerial.begin(AltRATE_START);
  digitalWrite(JDY_40_SET, LOW);

  altSerial.print("AT+BAUD=6\r\n");
  altSerial.readString();
  altSerial.print("AT+RFC=023\r\n");
  altSerial.readString();
  digitalWrite(JDY_40_SET, HIGH);
  delay(500);
  altSerial.begin(AltRATE);
}
