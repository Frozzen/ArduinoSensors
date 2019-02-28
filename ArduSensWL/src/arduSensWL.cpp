#include <Arduino.h>
#include <AltSoftSerial.h>

#include "arduSensUtils.h"
#include <CircularBuffer.h>


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
#define ADDR_TO ":" DEVICE_NO
#define SEND_DATA_CMD ADDR_TO ADDR_FROM SEND_CMD ";"
#define CONF_DATA_CMD ADDR_TO ADDR_FROM CONF_CMD ";"
#define ALIVE_DATA_CMD ADDR_TO ADDR_FROM ALIVE_CMD ";"
// команды :020101; :020102; :020103;


#define RATE 38400
#define AltRATE_START 9600
#define AltRATE 19200

#define ALT_RS232_RX 8
#define ALT_RS232_TX 9
#define JDY_40_SET 7

#define DEVICE_NO "02"
// счетчик keepalive
uint8_t s_time_cnt = 0;
#define DO_MSG_RATE 500
struct sBuf {
  char b[60];
} s_buf;
CircularBuffer<struct sBuf, 10> buffer;

AltSoftSerial altSerial;

// установили канал скоррость режим
void setupJDY_40()
{
  pinMode(JDY_40_SET, OUTPUT);
  altSerial.begin(AltRATE_START);
  digitalWrite(JDY_40_SET, LOW);

  altSerial.print("AT+BAUD=6\r\n");
  altSerial.readString();
  altSerial.print("AT+RFC=023\r\n");
  altSerial.readString();
  digitalWrite(JDY_40_SET, HIGH);
  delay(500);
  altSerial.begin(AltRATE);
}

void setup(void)
{
  Serial.begin(RATE);
  setupJDY_40();
  Serial.println("ArduSensWL");
}

void sendToServer(String &r)
{
  buffer.push(*(sBuf*)r.c_str());
}

/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  doTestContacts();
  if((s_time_cnt % DO_MSG_RATE) == 0) {
    doSendTemp();
  }
  {
    String cmd = altSerial.readString();    
    if(cmd == SEND_DATA_CMD) {
      while (!buffer.isEmpty())
        altSerial.println (buffer.pop().b);
        altSerial.println(":");
    } /*else if(cmd == CONF_DATA_CMD) {
      confArduSens();
      while (!buffer.isEmpty())
        altSerial.println (buffer.pop().b);      
        altSerial.println(":");      
    } */else if(cmd == ALIVE_DATA_CMD) {
      doAlive();
      altSerial.println (buffer.pop().b);
      altSerial.println(":");
    }
  }
  s_time_cnt++;
  delay(100);
}