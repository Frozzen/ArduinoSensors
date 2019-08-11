/*
  Blink

  Turns an LED on for one second, then off for one second, repeatedly.

A6 - Gnd
A7 - Value to LED
+5
  http://www.arduino.cc/en/Tutorial/Blink
*/

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  #if 0
  pinMode(A6, OUTPUT);
  pinMode(A7, INPUT);
  digitalWrite(A6, LOW);   // turn the LED on (HIGH is the voltage level)
  #else
  pinMode(3, OUTPUT);
  pinMode(2, INPUT);
  digitalWrite(3, HIGH);   // turn the LED on (HIGH is the voltage level)
  #endif
  Serial.begin(9600);
}

// the loop function runs over and over again forever
void loop() {
  #if 0
  int i = digitalRead(A7);
  #else
  int i = digitalRead(2);
  #endif
  Serial.print("a7:"); Serial.println(i);
  digitalWrite(LED_BUILTIN, i);   // turn the LED on (HIGH is the voltage level)
  delay(50);                       // wait for a second
}
