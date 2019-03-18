#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

#include "ardudev.h"
#include "arduSensUtils.h"

/*
 * через JDY40 рабоает по опросу, на serial рабоатет - просто печать
 * измеритель температуры + 4 контакта на замыкание
 * - температурных датчиков ds1820 может быть любое количество
 *  шлет только изменение опрос 30сек
 *   ArduStatHH/DS1820-XXXXXXXXXXXXXXX/temp=val раз в 30 сек при изменении
 *   ArduStatHH/larch-1=val раз в 0.1 сек контакт
 *   ArduStatHH/light=val раз в 0.1 сек освещенность
 * - контакты опрашивает раз 100мсек
 *   ArduStatHH/latch-1=val раз в 0.1 сек при изменении
 *   ArduStatHH/latch-4=val раз в 0.1 сек при изменении
 *  шлет только изменения
 * - раз в 5 секунд шлет keeralive
 *    ArduStatHH/INFO/alive=cnt раз в 5
 */


// счетчик keepalive
uint16_t s_time_cnt = 0;
#define DO_MSG_RATE 500

SoftwareSerial altSerial(ALT_RS232_RX,ALT_RS232_TX);

void setup(void)
{
  Serial.begin(RATE);
  altSerial.begin(RATE);
  setupArduSens();
  Serial.println("ArduSensWL");
}

void sendToServer(const char *r, bool now)
{
  Serial.println(r);
  altSerial.println(r);
}

/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  doTestContacts();
  s_time_cnt++;
  if((s_time_cnt % DO_MSG_RATE) == 0) {
    doSendTemp(); // сложить температуру в буффер    
  }
  delay(100);
}
