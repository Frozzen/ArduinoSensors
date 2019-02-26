#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

void sendToServer(String &r);
String getAddrString(DeviceAddress &dev);
void setupArduSens();