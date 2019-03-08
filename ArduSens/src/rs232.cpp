#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

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
