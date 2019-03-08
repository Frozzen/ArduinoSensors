#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Assign address manually. The addresses below will beed to be changed
// to valid device addresses on your bus. Device address can be retrieved
// by using either oneWire.search(deviceAddress) or individually via
const char *getAddrString(DeviceAddress &dev)
{
  static char s_buf[17];
  for (uint8_t i = 0; i < 8; i++)   {
    sprintf(s_buf+i*2, "%02x", (char)dev[i]);
  }
  s_buf[16] = 0;
  return s_buf;
}

/**
 * подсчет контрольной суммы
 */
void sendToServer(const char* r, bool now)
{
  uint8_t sum = 0;
  for(int8_t i = 0; i < (uint8_t)strlen(r); ++i)
    sum += r[i];
  sum = 0xff - sum;
  Serial.print(r);
  char buf[4];
  snprintf(buf, 4, ";%02x", sum);
  Serial.println(buf);
}
