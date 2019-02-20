/**
 * конвертер 485 на USB. замена сгоревшему. работает только на примем из rs485
 */
#include <Arduino.h>
#include <AltSoftSerial.h>

AltSoftSerial altSerial;
#define SerialTxControl 11
#define RS485Receive     LOW
#define AltRsDataRx 8
#define AltRsDataTx 9
#define USED 10
#define USB_RS_SPEED 38400
#define RS485_SPEED 9600

void setup() {
  pinMode(SerialTxControl, OUTPUT);
  digitalWrite(SerialTxControl, RS485Receive);
  Serial.begin(USB_RS_SPEED);
  Serial.println("AltSoftSerial");
  altSerial.begin(RS485_SPEED);
  altSerial.println("Hello World");
}

void loop() {
  char c;
#ifdef TWO_SIDE_CONVERTER
  if (Serial.available()) {
    c = Serial.read();
    altSerial.print(c);
  }
  if (altSerial.available()) {
    c = altSerial.read();
    Serial.print(c);
  }
#else
  c = altSerial.read();
  if(altSerial.overflow())
    Serial.write('?');
  else 
    Serial.write(c);
#endif
}
