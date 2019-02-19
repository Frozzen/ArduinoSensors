
#include <AltSoftSerial.h>

AltSoftSerial altSerial;
#define SerialTxControl 11
#define RS485Receive     LOW
#define AltRsDataRx 8
#define AltRsDataTx 9
#define USED 10

void setup() {
  pinMode(SerialTxControl, OUTPUT);
  digitalWrite(SerialTxControl, RS485Receive);
  Serial.begin(38400);
  Serial.println("AltSoftSerial");
  altSerial.begin(38400);
  altSerial.println("Hello World");
}

void loop() {
  char c;

/*  if (Serial.available()) {
    c = Serial.read();
    altSerial.print(c);
  }*/
  if (altSerial.available()) {
    c = altSerial.read();
    Serial.print(c);
  }
}
