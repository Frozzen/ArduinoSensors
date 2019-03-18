#include <SoftwareSerial.h>

SoftwareSerial altSerial(8, 9);
void setup() {
  // put your setup code here, to run once:
#if 0
#define JDY_40_SET 7
  altSerial.begin(9600);
  pinMode(JDY_40_SET, OUTPUT);
  digitalWrite(JDY_40_SET, LOW);
    delay(100);
  altSerial.println("AT+BAUD6");
    delay(100);
  digitalWrite(JDY_40_SET, HIGH);  
#else
  altSerial.begin(19200);
#endif  
  Serial.begin(19200);
  Serial.println("jdy - 19200");
}

void loop() {
  #if 1
  // put your main code here, to run repeatedly:
  if(altSerial.available()) {
    char c = altSerial.read();
    Serial.print(c);
  }
/*  delay(10);
  if(Serial.available()) {
    char c = altSerial.read();
    altSerial.print(c);
  }*/
  #else
    delay(100);
    altSerial.print('.');
    Serial.print('.');
  #endif
  
}
