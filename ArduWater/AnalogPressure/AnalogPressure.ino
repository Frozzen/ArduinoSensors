/*
  Analog Input

  Demonstrates analog input by reading an analog sensor on analog pin 0 and
  turning on and off a light emitting diode(LED) connected to digital pin 13.
  The amount of time the LED will be on and off depends on the value obtained
  by analogRead().

  The circuit:
  - potentiometer
    center pin of the potentiometer to the analog input 0
    one side pin (either one) to ground
    the other side pin to +5V
  - LED
    anode (long leg) attached to digital output 13
    cathode (short leg) attached to ground

  - Note: because most Arduinos have a built-in LED attached to pin 13 on the
    board, the LED is optional.

  created by David Cuartielles
  modified 30 Aug 2011
  By Tom Igoe

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/AnalogInput
*/

int sensorPin = A1;    // select the input pin for the potentiometer
int ledPin = 13;      // select the pin for the LED

void setup() {
  // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);
  Serial.begin(38400);
}

void loop() {
  // read the value from the sensor:
  int raw = analogRead(sensorPin);
  float voltage = (float) raw * 5.0 / 1024.0;     // voltage at the pin of the Arduino
  float pressure_kPa = (voltage - 0.5) / 4.0 * 1.200;          // voltage to pressure
  Serial.print(" value:"); Serial.println(pressure_kPa);
  // turn the ledPin on
  digitalWrite(ledPin, HIGH);
  // stop the program for <sensorValue> milliseconds:
  delay(raw);
  // turn the ledPin off:
  digitalWrite(ledPin, LOW);
  // stop the program for for <sensorValue> milliseconds:
  delay(raw);
}
