#include <Arduino.h>
#include <SoftwareSerial.h>

#define RATE 38400
extern void setupJDY_40();
extern SoftwareSerial altSerial;

void setup(void)
{
  Serial.begin(RATE);
  Serial.println("ArduSens WL dongle");
  setupJDY_40();
}

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
    char c;
  delayMicroseconds(580);
  if (Serial.available()) {
    c = Serial.read();
    altSerial.write(c);
    //Serial.write(c);
  }
  if (altSerial.available()) {
    c = altSerial.read();
    Serial.write(c);
  }
  #endif
}
