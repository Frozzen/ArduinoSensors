#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

#include "ardudev.h"
#include "arduSensUtils.h"
#include <CircularBuffer.h>

/*
 * измеритель температуры + 4 контакта на замыкание
 * - температурных датчиков ds1820 может быть любое количество
 *  шлет только изменение опрос 30сек
 *   ArduStatHHHH/DS1820-XXXXXXXXXXXXXXX/temp=val раз в 30 сек при изменении
 *   ArduStatHHHH/larch-1=val раз в 0.1 сек контакт
 *   ArduStatHHHH/light=val раз в 0.1 сек освещенность
 * - контакты опрашивает раз 100мсек
 *   ArduStatHHHH/latch-1=val раз в 0.1 сек при изменении
 *   ArduStatHHHH/latch-4=val раз в 0.1 сек при изменении
 *  шлет только изменения
 * - раз в 5 секунд шлет keeralive
 *    ArduStatHHHH/INFO/alive=cnt раз в 5
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
  Serial.println("ArduSensWL");
}

void sendToServer(const char *r, bool now)
{
  if(now) {
#ifdef DEBUG    
      Serial.print (r);
      Serial.println(":");
#endif
      altSerial.print(r);
      altSerial.println(":");
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
    doTestContacts();
    s_time_cnt++;
    if((s_time_cnt % DO_MSG_RATE) == 0) {
      doSendTemp(); // сложить температуру в буффер    
    }
  }
  char buf[10];
  altSerial.readBytesUntil('\n', buf, 10);
  if(strcmp(SEND_DATA_CMD, buf) == 0) 
    sendBuffToHost();  
  else if(strcmp(CONF_DATA_CMD, buf) == 0) 
    setupArduSens();
  else if(strcmp(ALIVE_DATA_CMD, buf) == 0) 
    doAlive();
}
