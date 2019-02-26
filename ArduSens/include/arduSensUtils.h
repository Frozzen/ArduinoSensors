#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DEVICE_NO "0001"

void sendToServer(String &r);
String getAddrString(DeviceAddress &dev);
void setupArduSens();
void doTestContacts();
bool doSendTemp();
void doAlive();
extern uint8_t s_time_cnt;