#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#ifndef DEVICE_NO
#define DEVICE_NO "02"
#endif

void sendToServer(String &r, bool now = false);
String getAddrString(DeviceAddress &dev);
void confArduSens();
void doTestContacts();
bool doSendTemp();
void doAlive();
extern uint8_t s_time_cnt;