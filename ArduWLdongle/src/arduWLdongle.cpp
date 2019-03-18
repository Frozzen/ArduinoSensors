#include <Arduino.h>
#include <SoftwareSerial.h>
#include "jdy40.h"

#define RATE 38400

void setup(void)
{
  Serial.begin(RATE);
  Serial.println("ArduSens WL dongle");
  jdy.init();
  jdy.put('#');
}

unsigned long s_last_tick = 0;
/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
#if 0
  char buf[100];
  if (Serial.available()) {
    Serial.write('<');
    size_t ix = Serial.readBytesUntil('\n', buf, sizeof(buf));
    altSerial.write(buf, ix);
    //Serial.write(buf, ix);
  }
  if (altSerial.available()) {
    Serial.write('>');
    size_t ix = altSerial.readBytesUntil('\n', buf, sizeof(buf));
    Serial.write(buf, ix);
  }
  #else
  if((millis() - s_last_tick) > 1000) {
    s_last_tick = millis();
    //jdy_send(":020101;\n\r");
    Serial.print('.'); 
    jdy.put(']');
  }
  char c;
  //delayMicroseconds(580);
  if (Serial.available()) {
    c = Serial.read();
    jdy.put(c);
  }
  if (jdy.status()) {
    c = jdy.get();
    Serial.write(c);
  }
  #endif
}
