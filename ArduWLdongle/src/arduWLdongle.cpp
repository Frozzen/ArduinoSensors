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

  if (Serial.available()) {
    char c;
    altSerial.write(c = Serial.read());
    Serial.write(c);
  }
  if (altSerial.available()) {
    Serial.write(altSerial.read());
  }
}
