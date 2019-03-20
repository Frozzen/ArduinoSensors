#include <Arduino.h>
#include <Wire.h>
#include <Bounce2.h>
#include <RtcDS1307.h>
#include <TM1637Display.h>

#include "ardudev.h"
#include "arduSensUtils.h"


// Module connection pins (Digital Pins)
#define DISPLAY_CLK 4
#define DISPLAY_DIO 3

// выход со счетчика
#define WATER_COUNTER_PIN 5
// показать/убрать показания на дисплее
#define DISPLAY_BUTTON_PIN 6
// кнопка открыть/закрыть воду
#define OPEN_WATER_BUTTON_PIN 7 
// кран открыт
#define IS_TAP_OPEN 9
// кран закрыт
#define IS_TAP_CLOSE 10
// закрыть кран
#define OPEN_TAP_CMD 11
// открыть кран
#define CLOSE_TAP_CMD 12

// бочка полная
#define IS_BURREL_FULL A7
// бочка пустая
#define IS_BURREL_EMPTY A6
// PIN  A4(SDA),A5(SCL) - I2C - display
// вода на полу - LED протечка
#define IS_FLOOR_WET A2
// LED кран закрыт
#define WATER_PRESSURE A1 //the YELLOW pin of the Sensor is connected with A2 of Arduino/Catduino

// адреса в eeprom
#define EERPOM_ADDR_COUNT 0

#define LOG_MESSAGE ":01log={\"type\":\"device_connected\",\"message\":\""
#define LOG_MESSAGE_END "\"}"

// Setup  instancees to communicate with devices
RtcDS1307<TwoWire> Rtc(Wire);

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
extern Bounce s_input_pin[IN_PIN_COUNT];

Bounce &waterClick = s_input_pin[WATER_COUNTER_PIN-5],                // счетчик воды
       &displayClick = s_input_pin[DISPLAY_BUTTON_PIN-5],           // кнопка показать значения
       &openTapClick = s_input_pin[OPEN_WATER_BUTTON_PIN-5],
       &isTapOpen = s_input_pin[IS_TAP_OPEN-5],
       &isTapClosed = s_input_pin[IS_TAP_CLOSE-5],
       &isBarrelFull = s_input_pin[IS_BURREL_FULL-5],
       &isBarrelEmpty = s_input_pin[IS_BURREL_EMPTY-5],
       &isFloorWet = s_input_pin[IS_FLOOR_WET-5];

// значение счетчика воды в 10 литрах
uint32_t s_water_count = 0, s_water_count_rtc;
// показывать на дисплее счетчик
bool s_displayMode = true;
// счетчик времени - для задержанных операций
uint16_t s_time_cnt = 0;

////////////////////////////////////////////////////////

extern bool checkButtonChanged(Bounce &bounce);
void isButtonChanged(Bounce &bounce, const char *title)
{
  boolean changed = bounce.update(); 
  if ( changed ) {
      uint8_t value = bounce.read();
      snprintf(s_buf, sizeof(s_buf), ADDR_STR "%s=%d", title, value);
      sendToServer(s_buf);
    }  
}

enum eTapStates {
  eTapNone,
  eTapWorking,
  eTapStop
} s_fsm_tap_state;
uint32_t s_fsm_timeout = 0;
//------------------------------------------------------
/// послать в шину изменения в контакотах
void   doTestWater(){
  if(checkButtonChanged(waterClick)) {
    ++s_water_count;
    snprintf(s_buf, sizeof(s_buf),  ADDR_STR "/water=%d", s_water_count);
    sendToServer(s_buf);
  }
  if(checkButtonChanged(displayClick)) {
      s_displayMode ^= true;
  }
  if(checkButtonChanged(openTapClick)) {
    if(isTapOpen.value) {
      digitalWrite(OPEN_TAP_CMD, HIGH);    
      digitalWrite(CLOSE_TAP_CMD, LOW);    
    } else  {
      digitalWrite(CLOSE_TAP_CMD, HIGH);    
      digitalWrite(OPEN_TAP_CMD, LOW);    
    }
    s_fsm_tap_state = eTapWorking;
    s_fsm_timeout = millis();
  }
  isButtonChanged(isBarrelEmpty, "barrel_empty");
  isButtonChanged(isBarrelFull, "barrel_full");
  isButtonChanged(isTapClosed, "tap_closed");
  isButtonChanged(isTapOpen, "tap_open");
  isButtonChanged(isFloorWet, "wet_floor");
}

/*macro definition of sensor*/
// Использовал программу, которую предоставил продавец. Заметил, что в покое он показывает отрицательное значение, 
// равное -3.35 кПа. я не могу понять, с чем связана данная цифра.
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

void sendDeviceConfig(const char *dev)
{
    snprintf(s_buf, sizeof(s_buf), LOG_MESSAGE SENSOR_NAME DEVICE_NO "%s" LOG_MESSAGE_END, dev);
    sendToServer(s_buf);
}

//------------------------------------
void setup(void)
{
  display.clear();
  display.setBrightness(0x0f);
  Serial.begin(38400);
  setupArduSens();
  
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
  sendDeviceConfig("/wet_floor");
}

/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  // контакты посылаем 10 раз в сек
  doTestContacts();
  // обновляем дисплей 10 раз в сек
  doDisplayValue();
  doTestWater();

  // раз в 1 минуту послать alive или температуру
  if((s_time_cnt % DO_MSG_RATE) == 0) {
    if(!doSendTemp())
      doAlive();    
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
      break;
    case eTapNone:
    default:
      break;
  }
  delay(100);
  s_time_cnt++;
}
