#include <Arduino.h>
#include <Wire.h>
#include <Bounce2.h>
#include <RtcDS1307.h>
#include <TM1637Display.h>

#include "ardudev.h"
/// TODO занести в NVRAM значение для воды, 
/// TODO занести в NVRAM текущее время
/// TODO открыть закрыть кран раз в 2 недели ночью

#define TIME_TO_RELAX 5
enum eTapStates {
  eTapNone,
  eTapWorking,
  eTapStop, 
  eTapOpen, 
  eTapClose
} s_fsm_tap_state;
uint32_t s_fsm_timeout = 0;

/// настроили кнопки которые нажимают
void iniInputPin(Bounce &bounce, uint8_t pin)
{
  pinMode(pin, INPUT); 
  digitalWrite(pin, HIGH);
  bounce.attach(pin); // Настраиваем Bouncer
  bounce.interval(TIME_TO_RELAX); // и прописываем ему интервал дребезга
}

/// проверили что кнопку отпустили
bool checkButtonChanged(Bounce &bounce)
{
  boolean changed = bounce.update(); 
  int value = HIGH;
  if ( changed ) {
      value = bounce.read();
      // Если значение датчика стало ЗАМКНУТО
      if ( value == LOW) {
        return true;
      }
    }  
  return false;
}
#define LOG_MESSAGE ":01log={\"type\":\"device_connected\",\"message\":\""
#define LOG_MESSAGE_END "\"}"

#define s_automatic_open_enabled 1

// Setup  instancees to communicate with devices I2C
RtcDS1307<TwoWire> Rtc(Wire);

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
extern Bounce s_input_pin[IN_PIN_COUNT];

Bounce  btn_open_water,    // кнопка открыть воду
        water_cnt,                // счетчик воды
        btn_display,           // кнопка показать значения        
        water_is_empty,
        water_is_full,
        isTapOpen,
        isTapClosed;

// значение счетчика воды в 10 литрах
uint32_t s_water_count = 0, s_water_count_rtc;
// показывать на дисплее счетчик
bool s_displayMode = true;
// счетчик времени - для задержанных операций
uint16_t s_time_cnt = 0;

////////////////////////////////////////////////////////
void sendToServer(const char *r)
{
char buf[4];
  Serial.print(r);
  uint8_t sum = 0;
  for(const char *str = r; *str; ++str)
    sum += *str;
  sum = 0xff - sum;
  snprintf(buf, sizeof(buf), ";%02x", sum);
  Serial.println(buf);
}

extern bool checkButtonChanged(Bounce &bounce);
bool isButtonChanged(Bounce &bounce, const char *title)
{
  boolean changed = bounce.update(); 
  if ( changed ) {
      uint8_t value = bounce.read();
      snprintf(s_buf, sizeof(s_buf), ADDR_STR "%s=%d", title, value);
      sendToServer(s_buf);
    }
    return changed;  
}

//------------------------------------------------------
/// послать в шину изменения в контактах
void   doTestWater() {

  if(checkButtonChanged(water_cnt)) {
    ++s_water_count;
    snprintf(s_buf, sizeof(s_buf),  ADDR_STR "/water=%ld", s_water_count);
    sendToServer(s_buf);
  }

  // переключить экранчик
  if(checkButtonChanged(btn_display)) {
      s_displayMode ^= true;
  }

  // запустить FSM на изменение ветниля
  if(checkButtonChanged(btn_open_water)) {
    s_fsm_tap_state = isTapOpen.read() ? eTapClose : eTapOpen;
  }
  // при появлении 1цы дать команду открыть кран - водзможно сделать блокировку этого
  if(isButtonChanged(water_is_empty, "barrel_empty") && 
    water_is_empty.read() && s_automatic_open_enabled)
      s_fsm_tap_state =  eTapOpen;
  // при появлении 1цы дать команду закрыть кран
  if(isButtonChanged(water_is_full, "barrel_full") && water_is_full.read())
    s_fsm_tap_state = eTapClose;
  isButtonChanged(isTapClosed, "tap_closed");
  isButtonChanged(isTapOpen, "tap_open");
}

/**
 * отправить на хоста давление
 * Использовал программу, которую предоставил продавец. 
 * Заметил, что в покое он показывает отрицательное значение, 
 * равное -3.35 кПа. я не могу понять, с чем связана данная цифра.
*/
void doWaterPressure()
{

  int raw = analogRead(WATER_PRESSURE);
  float voltage = (float) raw * 5.0 / 1024.0;     // voltage at the pin of the Arduino
  float pressure_kPa = (voltage - 0.5) / 4.0 * 1.200;          // voltage to pressure
  char str_press[6];
  dtostrf((float)pressure_kPa, 4, 1, str_press);
  snprintf(s_buf, sizeof(s_buf), ADDR_STR "/wpress=%s", str_press);
  sendToServer(s_buf);
}


/// показать текущее значение на дисплее. показываем ddd.ddd. 
/// если захотите учидеть старшие цифры - на mqtt или в подвал
void doDisplayValue()
{
  if(s_displayMode) {
    int16_t val = (s_water_count / 10) & 0x3fff;
    display.showNumberDec(val, true); 
  } else 
    display.clear();
}

char s_buf[MAX_OUT_BUFF];

void sendDeviceConfig(const char *dev)
{
    snprintf(s_buf, sizeof(s_buf), LOG_MESSAGE SENSOR_NAME DEVICE_NO "%s" LOG_MESSAGE_END, dev);
    sendToServer(s_buf);
}

extern void iniInputPin(Bounce &bounce, uint8_t pin);
//------------------------------------
void setup(void)
{
  display.clear();
  display.setBrightness(0x0f);
  Serial.begin(38400);

  iniInputPin(btn_open_water, BTN_OPEN_WATER);
  iniInputPin(water_cnt, WATER_COUNTER_PIN);
  iniInputPin(btn_display, BTN_DISPLAY);
  iniInputPin(water_is_full, IS_BURREL_FULL);
  iniInputPin(water_is_empty, IS_BURREL_EMPTY);
  iniInputPin(isTapOpen, IS_TAP_OPEN);
  iniInputPin(isTapClosed, IS_TAP_CLOSE);
  
  pinMode(OPEN_TAP_CMD, OUTPUT);
  pinMode(CLOSE_TAP_CMD, OUTPUT);
  pinMode(WATER_PRESSURE, INPUT);
  digitalWrite(OPEN_TAP_CMD, LOW);
  digitalWrite(CLOSE_TAP_CMD, LOW);
  digitalWrite(WATER_PRESSURE, HIGH);
  
  Rtc.Begin();
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed s_time_cnt
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 
  Rtc.GetMemory(EERPOM_ADDR_COUNT, (uint8_t*)&s_water_count_rtc, (uint8_t)sizeof(s_water_count_rtc));
  s_water_count = s_water_count_rtc;
  sendDeviceConfig("/water");
  sendDeviceConfig("/barrel_empty");
  sendDeviceConfig("/barrel_full");
  sendDeviceConfig("/tap_closed");
  sendDeviceConfig("/tap_open");

  snprintf(s_buf, sizeof(s_buf),  ADDR_STR "/water=%ld", s_water_count);
  sendToServer(s_buf);
}

/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  // обновляем дисплей 10 раз в сек
  doDisplayValue();
  doTestWater();

  // раз в 1 минуту послать alive или температуру
  if((s_time_cnt % DO_MSG_RATE) == 0) {
    doWaterPressure();
    // write in eeprom
    if(s_water_count_rtc != s_water_count) {
      Rtc.SetMemory(EERPOM_ADDR_COUNT, (uint8_t*)&s_water_count, (uint8_t)sizeof(s_water_count_rtc));
      s_water_count_rtc = s_water_count;
    }
  }
  // FSM для открытия и закрытия крана
  switch (s_fsm_tap_state)
  {
    case eTapWorking: {
      uint32_t delay = millis() - s_fsm_timeout;
      if(delay > TAP_TIMEOUT)
        s_fsm_tap_state = eTapStop;
      }
      break;
    case eTapStop:
      digitalWrite(OPEN_TAP_CMD, LOW);
      digitalWrite(CLOSE_TAP_CMD, LOW);      
      s_fsm_tap_state = eTapNone;
      break;
    case eTapNone:
    default:
    // команды открытия закрытия крана - работают только если предидущая работа завершена
    case eTapClose:
      if(s_fsm_tap_state != eTapNone)
        break;
      digitalWrite(OPEN_TAP_CMD, LOW);
      digitalWrite(CLOSE_TAP_CMD, HIGH);      
      s_fsm_timeout = millis();
      s_fsm_tap_state = eTapWorking;
      snprintf(s_buf, sizeof(s_buf), ADDR_STR "/wtapcnmd=0");
      sendToServer(s_buf);
      break;
    case eTapOpen:
      if(s_fsm_tap_state != eTapNone)
        break;
      digitalWrite(OPEN_TAP_CMD, HIGH);
      digitalWrite(CLOSE_TAP_CMD, LOW);      
      s_fsm_timeout = millis();
      s_fsm_tap_state = eTapWorking;
      snprintf(s_buf, sizeof(s_buf), ADDR_STR "/wtapcnmd=1");
      sendToServer(s_buf);
      break;    
  }
  /// TODO сделать цепочку из rs232 устройств получает в ALtSerial отправляет в Serial и назад
  /// TODO сделать управление командами команды не ко мне форвардить
  delay(100);
  s_time_cnt++;
}
