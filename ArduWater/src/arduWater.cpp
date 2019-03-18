#include <Arduino.h>
#include <Wire.h>
#include <RtcDS1307.h>
#include <TM1637Display.h>

#include "ardudev.h"
#include "arduSensUtils.h"


// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
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
 

// arrays to hold device addresses
uint8_t s_therm_count = 0;
// значение счетчика воды в 10 литрах
uint32_t s_water_count = 0, s_water_count_rtc;
// показывать на дисплее счетчик
bool s_displayMode = true;
// счетчик времени - для задержанных операций
uint16_t s_time_cnt = 0;

////////////////////////////////////////////////////////

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

//------------------------------------
void setup(void)
{
  display.clear();
  display.setBrightness(0x0f);
  Serial.begin(38400);
  setupArduSens();
  // TODO сконфигурировать выходы для управления мотором и датчиками
  pinMode(IS_TAP_OPEN, INPUT); 
  pinMode(IS_TAP_CLOSE, INPUT); 
  pinMode(OPEN_TAP_CMD, OUTPUT); 
  pinMode(CLOSE_TAP_CMD, OUTPUT); 
  pinMode(IS_BURREL_FULL, INPUT); 
  pinMode(IS_BURREL_EMPTY, INPUT); 
  pinMode(IS_FLOOR_WET, INPUT); 
  pinMode(WATER_PRESSURE, INPUT); 

  Rtc.Begin();
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed s_time_cnt
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 
  Rtc.GetMemory(EERPOM_ADDR_COUNT, (uint8_t*)&s_water_count_rtc, (uint8_t)sizeof(s_water_count_rtc));
  s_water_count = s_water_count_rtc;
  // конфигурирую входные pin для кнопок
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

  delay(100);
  s_time_cnt++;
}
