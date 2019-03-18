#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial altSerial(8, 9);

void setup(void)
{
  altSerial.begin(19200);
  Serial.begin(19200);
}

unsigned long s_last_tick = 0;
/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  if(altSerial.available()) {
    char c = altSerial.read();
    Serial.print(c);
  }
}
