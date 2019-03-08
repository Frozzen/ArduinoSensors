#include <Arduino.h>
//#include <AltSoftSerial.h>
#include <SoftwareSerial.h>

#include "arduSensUtils.h"
#include <CircularBuffer.h>

//#define DEBUG

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
// послать проверить жив ли
#define ALIVE_CMD "03"
// послать конфигурацию
#define CONF_CMD "02"
// послать состояние
#define SEND_CMD "01"
// адрес от кого запрос 01 сервер
#define ADDR_FROM "01"
// этот адрес надо менять по платам
#define HOST_ADDR ":" DEVICE_NO
#define SEND_DATA_CMD HOST_ADDR ADDR_FROM SEND_CMD ";"
#define CONF_DATA_CMD HOST_ADDR ADDR_FROM CONF_CMD ";"
#define ALIVE_DATA_CMD HOST_ADDR ADDR_FROM ALIVE_CMD ";"
// команды :020101; :020102; :020103;

#define RATE 38400

#define ALT_RS232_RX 8
#define ALT_RS232_TX 9
#define JDY_40_SET 7


// счетчик keepalive
uint8_t s_time_cnt = 0;
#define DO_MSG_RATE 500
struct sBuf {
  char b[60];
} s_buf[8];
uint8_t s_buf_idx = 0;
CircularBuffer<struct sBuf*, 10> buffer;

//extern AltSoftSerial altSerial;
extern SoftwareSerial altSerial;
extern void setupJDY_40();

void setup(void)
{
  Serial.begin(RATE);
  setupJDY_40();
  Serial.println("ArduSensWL");
}

void sendToServer(String &r, bool now)
{
  if(now) {
#ifdef DEBUG    
      Serial.print (r);
      Serial.println(":");
#endif
      altSerial.print(r);
      altSerial.println(":");
  } else {
    strncpy(s_buf[s_buf_idx].b, r.c_str(), sizeof(sBuf));
    buffer.push(&(s_buf[s_buf_idx & 0x7]));
    ++s_buf_idx;
  }
}

void sendBuffToHost()
{
    while (!buffer.isEmpty()) {
      char *b = buffer.shift()->b;
#ifdef DEBUG    
      Serial.print (b);
      Serial.println(":");
#endif
      altSerial.print(b);
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
  
#ifdef DEBUG    
  String cmd = Serial.readString();    
#else  
  String cmd = altSerial.readString();    
#endif  
  Serial.print(cmd);
  if(cmd == SEND_DATA_CMD) 
    sendBuffToHost();  
  else if(cmd == CONF_DATA_CMD) 
    confArduSens();
  else if(cmd == ALIVE_DATA_CMD) 
    doAlive();
}
