#include <Arduino.h>
//#include <AltSoftSerial.h>
#include <SoftwareSerial.h>

#define RATE 38400
extern void setupJDY_40();
//extern AltSoftSerial altSerial;
extern SoftwareSerial altSerial;

void setup(void)
{
  Serial.begin(RATE);
  setupJDY_40();
  Serial.println("ArduSens WL dongle");
}

/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  char c;

  if (Serial.available()) {
    c = Serial.read();
    altSerial.print(c);
    //Serial.print(c);
  }
  if (altSerial.available()) {
    c = altSerial.read();
    Serial.print(c);
  }
}
