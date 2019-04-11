#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

#include "ardudev.h"
#include "arduSensUtils.h"

// счетчик keepalive 
uint16_t s_time_cnt = 0;

SoftwareSerial altSerial(ALT_RS232_RX,ALT_RS232_TX);

void setup(void)
{
#ifdef USE_SERIAL
  Serial.begin(38400);
  Serial.println("ArduSensWL");
#endif
  
#ifdef USE_ALT_SERIAL
  altSerial.begin(ALT_SERIAL_RATE);
  altSerial.println("ArduSensWL");
#endif
  pinMode(JDY_40_SET, OUTPUT);
#ifdef INIT_JDY_40
  pinMode(JDY_40_SET, OUTPUT);
  digitalWrite(JDY_40_SET, LOW);
  delayMicroseconds(5000);
  altSerial.println("AT+BAUD"); Serial.print(altSerial.readString());
  altSerial.println("AT+RFID"); Serial.print(altSerial.readString());
  altSerial.println("AT+DVID"); Serial.print(altSerial.readString());
  altSerial.println("AT+RFC"); Serial.print(altSerial.readString());
  altSerial.println("AT+POWE"); Serial.print(altSerial.readString());
  altSerial.println("AT+CLSS"); Serial.print(altSerial.readString());
  altSerial.println("AT+BAUD"); Serial.print(altSerial.readString());
  altSerial.println("AT+BAUD4"); Serial.print(altSerial.readString());
  altSerial.println("AT+BAUD"); Serial.print(altSerial.readString());
  while(1);
#endif
  digitalWrite(JDY_40_SET, HIGH);
  setupArduSens();
}

void sendToServer(const char *r, bool now)
{
char buf[4];
#ifdef USE_SERIAL
  Serial.print(r);
#endif
#ifdef USE_ALT_SERIAL
  altSerial.print(r);
#endif
  uint8_t sum = 0;
  for(const char *str = r; *str; ++str)
    sum += *str;
  sum = 0xff - sum;
  snprintf(buf, sizeof(buf), ";%02x", sum);
#ifdef USE_SERIAL
  Serial.println(buf);
#endif
#ifdef USE_ALT_SERIAL
  altSerial.println(buf);
#endif
}

/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  doTestContacts();
  s_time_cnt++;
  if((s_time_cnt % DO_MSG_RATE) == 0) {
    doSendTemp(); // сложить температуру в буффер    
  }
  delay(100);
}
