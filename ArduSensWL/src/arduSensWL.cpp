#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

#include "ardudev.h"
#include "arduSensUtils.h"
#include <CircularBuffer.h>

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
struct sBuf {
  char b[80];
};
CircularBuffer<struct sBuf, 8> s_buffer;

extern SoftwareSerial altSerial;
extern void setupJDY_40();

void setup(void)
{
  Serial.begin(RATE);
  setupJDY_40();
  setupArduSens();
  Serial.println("ArduSensWL");
}

void sendToServer(const char *r, bool now)
{
  Serial.println(r);
  if(now) {
#ifdef DEBUG    
      Serial.print (r);
      Serial.println(":");
#endif
      altSerial.println(r);
  } else {
    s_buffer.push(*(sBuf*)r);
  }
}

void sendBuffToHost()
{
    while (!s_buffer.isEmpty()) {
      sBuf bf = s_buffer.shift();
#ifdef DEBUG    
      Serial.print (b);
      Serial.println(":");
#endif
      altSerial.print(bf.b);
      altSerial.println(":");
  }   
}
/*
   Main function, calls the temperatures in a loop.
*/
unsigned long s_last_tick = 0;
void loop(void)
{
  if((millis() - s_last_tick) > 100) {
    s_last_tick = millis();
    doTestContacts();
    s_time_cnt++;
    if((s_time_cnt % DO_MSG_RATE) == 0) {
      doSendTemp(); // сложить температуру в буффер    
    }
  }
  if(!altSerial.available())
    return;
  char buf[10] = {0};
  altSerial.readBytesUntil('\n', buf, 10);
  Serial.print(buf);
  if(strcmp(SEND_DATA_CMD, buf) == 0) 
    sendBuffToHost();  
  else if(strcmp(CONF_DATA_CMD, buf) == 0) 
    setupArduSens();
  else if(strcmp(ALIVE_DATA_CMD, buf) == 0) 
    doAlive();
}
