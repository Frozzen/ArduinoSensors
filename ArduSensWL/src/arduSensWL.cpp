#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

#include "ardudev.h"
#include "arduSensUtils.h"

// счетчик keepalive
uint16_t s_time_cnt = 0;
#define DO_MSG_RATE 5000

SoftwareSerial altSerial(ALT_RS232_RX,ALT_RS232_TX);

void setup(void)
{
  Serial.begin(RATE);
  altSerial.begin(RATE);
  setupArduSens();
  Serial.println("ArduSensWL");
}

void sendToServer(const char *r, bool now)
{
  Serial.println(r);
  altSerial.println(r);
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
