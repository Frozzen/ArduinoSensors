#include <Arduino.h>
#define RATE 38400
#include "arduSensUtils.h"
/*
 * измеритель температуры + 4 контакта на замыкание
 * - температурных датчиков ds1820 может быть любое количество
 *  шлет только изменение опрос 30сек
 *   ArduStatHHHH/DS1820-XXXXXXXXXXXXXXX/INFO/resolution=val - один раз при старте
 *   ArduStatHHHH/DS1820-XXXXXXXXXXXXXXX/value=val раз в 30 сек при изменении
 *   ArduStatHHHH/light=val раз в 0.1 сек освещенность
 * - контакты опрашивает раз 100мсек
 *   ArduStatHHHH/latch-1=val раз в 0.1 сек при изменении
 *   ArduStatHHHH/latch-4=val раз в 0.1 сек при изменении
 *  шлет только изменения
 * - раз в 5 секунд шлет keeralive
 *    ArduStatHHHH/INFO/alive=cnt раз в 5
 *    ArduStatHHHH/INFO/count=val - один раз при старте
 * - все сточки закрываются контрольной суммой через :
 */
#define DO_MSG_RATE 500

// счетчик keepalive
uint8_t s_time_cnt = 0;

void setup(void)
{
  // start serial485 port
  Serial.begin(RATE);
  setupArduSens();
}

/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  doTestContacts();
  if((s_time_cnt % DO_MSG_RATE) == 0) {
    if(!doSendTemp())
      doAlive();    
  }

  delay(100);
  s_time_cnt++;
}
