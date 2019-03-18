#include <Arduino.h>
#include <SoftwareSerial.h>
#include "jdy40.h"

#define AltRATE 19200

#define rX 8
#define tX 9
#define JDY_40_SET 7

SoftwareSerial altSerial(rX, tX);
JDY40 jdy;

void JDY40::getConfig(const char*str)
{
  delay(20);  
  while(*str){
    altSerial.write(*str++);
  }
char buf[20] = {0};
  altSerial.readBytesUntil('\n', buf, sizeof(buf));
  Serial.println(buf); 
}


JDY40::JDY40() 
{
  pinMode(JDY_40_SET, OUTPUT);
}

// установили канал скоррость режим
void JDY40::setup()
{
#define AltRATE_START 9600
  altSerial.begin(AltRATE_START);
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
}

// установили канал скоррость режим
void JDY40::init()
{
  altSerial.begin(9600);
  // отчет о текущий конфигурации
  digitalWrite(JDY_40_SET, LOW);
  getConfig("AT+BAUD\r\n");
  getConfig("AT+RFID\r\n");
  getConfig("AT+DVID\r\n");
  getConfig("AT+RFC\r\n");
  getConfig("AT+POWE\r\n");
  getConfig("AT+CLSS\r\n");
  digitalWrite(JDY_40_SET, HIGH);
}

uint8_t JDY40::status()
{
  return altSerial.available();
}

char JDY40::get()
{
  delay(20);  
  return altSerial.read();
}

void JDY40::put(char c)
{
  delay(20);  
    altSerial.write(c);
}

void JDY40::send(const char* str)
{
    altSerial.write(":020101;\n\r");
}
