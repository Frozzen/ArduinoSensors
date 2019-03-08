#include <Arduino.h>
/**
* Берется счетчик с импульсным выходом. Один импульс на 10л воды. #36
* На ардуине импульсы заводятся на прерывания. В цикле 0.1 сек они проверяются если изменились, 
  то каждые 5 минут записываются в eprom и передаются в брокера.  
  Там же могут быть термометры - на подвал и воду. отличается от ArduSens
* показывает расход на 7 сегментнике - ddd.d куб.м  DS3231
*  нужен RTC, там же и память. записывать значение в eeprom. RTC_DS1307
* сделать кнопку показать на дисплее значение 


* = MQTT topic =
* стандартные от ArduSens. меняется название датчика и добавляется сообщение
* HHHH - номер платы 
* XXXXXXXXXXXXXXX адрес датчика 
* N номер контакта
* ArduWaterHHHH/pressure=val - давление воды на входные
* ArduWaterHHHH/burrel_full=true|false - бочка переполнена
* ArduWaterHHHH/burrel_empty=true|false - бочка пустая
* ArduWaterHHHH/water_tap=true|false|none - входной кран true открыт,none-неисвестно,false=закрыт
* ArduWaterHHHH/water=val - сколько насчитано литров/10
* ArduWaterHHHH/INFO/count=val - число термометров один раз при старте 
* ArduWaterHHHH/DS1820-XXXXXXXXXXXXXXX/INFO/resolution=val - один раз при старте 
* ArduWaterHHHH/DS1820-XXXXXXXXXXXXXXX/temp=val - температура. раз в 60 сек при изменении 
* ArduWaterHHHH/latch-N=val - состояние контакта раз в 0.1 сек 
* ArduWaterHHHH/INFO/alive=cnt раз в 60 сек счетчик на каждое сообщение +1(%256) 
* - все сточки закрываются контрольной суммой через :
 */
#include <Wire.h>
#include <RtcDS1307.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TM1637Display.h>
#include <Bounce2.h>


#define DO_MSG_RATE 1800

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS D2
// Module connection pins (Digital Pins)
#define DISPLAY_CLK D4
#define DISPLAY_DIO D3

// выход со счетчика
#define WATER_COUNTER_PIN D5
// показать/убрать показания на дисплее
#define DISPLAY_BUTTON_PIN D6
// кнопка открыть/закрыть воду
#define OPEN_WATER_BUTTON_PIN D7 
// кран открыт
#define IS_TAP_OPEN D9
// кран закрыт
#define IS_TAP_CLOSE D10
// закрыть кран
#define OPEN_TAP_CMD D11
// открыть кран
#define CLOSE_TAP_CMD D12

// бочка полная
#define IS_BURREL_FULL A7
// бочка пустая
#define IS_BURREL_EMPTY A6
// PIN  A4(SDA),A5(SCL) - I2C - display
// вода на полу - LED протечка
#define IS_FLOOR_WET A2
// LED кран закрыт
#define WATER_PRESSURE A1 //the YELLOW pin of the Sensor is connected with A2 of Arduino/Catduino

#define TEMPERATURE_PRECISION 9
#define DEVICE_NO "0001"
#define MAX_DS1820_COUNT 2
#define PRIORITY_485 4

// адрес куда отправляется сообщение :01 хост
#define HOST_ADDR ":01"
#define SENSOR_NAME "ArduWater"

// время на дребезг контактов
#define TIME_TO_RELAX 5
// адреса в eeprom
#define EERPOM_ADDR_COUNT 0

#define LOG_MESSAGE ":01log={\"type\":\"device_connected\",\"message\":\""
#define LOG_MESSAGE_END "\"}"

#include "../../rs485.cpp"

// Setup  instancees to communicate with devices
RtcDS1307<TwoWire> Rtc(Wire);
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
 
Bounce waterClick,                // счетчик воды
       displayClick,           // кнопка показать значения
       freeClick[IN_PIN_COUNT];  // неприсвоенные ножки на потом

// arrays to hold device addresses
uint8_t s_therm_count = 0;
// счетчик keepalive
uint8_t s_cnt = 0;
DeviceAddress s_thermometer[MAX_DS1820_COUNT];
float s_last_temp[MAX_DS1820_COUNT];
// значение счетчика воды в 10 литрах
uint32_t s_water_count = 0, s_water_count_rtc;
// показывать на дисплее счетчик
bool s_displayMode = true;
// счетчик времени - для задержанных операций
uint16_t s_time_cnt = 0;

////////////////////////////////////////////////////////

/// сформировать пакет что устройство живо
void doAlive()
{
  String r = (HOST_ADDR SENSOR_NAME DEVICE_NO "/INFO/alive=") + String(s_cnt++,DEC);
  sendToServer(r);
}

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

//------------------------------------------------------
/// послать в шину изменения в контакотах
void   doTestContacts(){
  if(checkButtonChanged(waterClick)) {
    ++s_water_count;
    String r(HOST_ADDR SENSOR_NAME DEVICE_NO "/water=");
    r += String(s_water_count, DEC);    
    sendToServer(r);
  }
  if(checkButtonChanged(displayClick)) {
      s_displayMode ^= true;
  }
  for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix ) {
    boolean changed = freeClick[ix].update(); 
    if ( changed ) {
        uint8_t value = freeClick[ix].read();
        String r(HOST_ADDR SENSOR_NAME DEVICE_NO "/latch-");
        r += String(ix, DEC);
        r += "=" +String(value, DEC);
        sendToServer(r);
      }  
    }
}

/*macro definition of sensor*/
// Использовал программу, которую предоставил продавец. Заметил, что в покое он показывает отрицательное значение, 
// равное -3.35 кПа. я не могу понять, с чем связана данная цифра.
double get_water_pressure()
{

  int raw = analogRead(SENSOR);
  float voltage = (float) raw * 5.0 / 1024.0;     // voltage at the pin of the Arduino
  Serial.println("Pressure is");
  float pressure_kPa = (voltage - 0.5) / 4.0 * 1.200;          // voltage to pressure
  return pressure_kPa;
}

/// послать в шину изменение в температуре
bool doSendTemp()
{
  {
    float pressure = get_water_pressure();
    String r(HOST_ADDR SENSOR_NAME DEVICE_NO "/pressure=");
    r += "/temp="+String(pressure, 2);
    sendToServer(r);
  }
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  if(s_therm_count == 0)
    return false;
  sensors.requestTemperatures();

  // print the device information
  bool updated = false;
  for(uint8_t ix = 0; ix < s_therm_count; ++ix ) {
    float tempC = sensors.getTempC(s_thermometer[ix]);
    // формируем строку с температурой
    String r(HOST_ADDR SENSOR_NAME DEVICE_NO "/DS1820-");
    r += getAddrString(s_thermometer[ix]);
    r += "/temp="+String(tempC, 2);
    sendToServer(r);
    s_last_temp[ix] = tempC;
    updated = true;
  }
  return updated;
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
  // start serial485 port
  Serial.begin(RATE);
  // TODO сконфигурировать выходы для управления мотором и датчиками
  pinMode(IS_TAP_OPEN, INPUT); 
  pinMode(IS_TAP_CLOSE, INPUT); 
  pinMode(OPEN_TAP_CMD, OUTPUT); 
  pinMode(CLOSE_TAP_CMD, OUTPUT); 
  pinMode(IS_BURREL_FULL, INPUT); 
  pinMode(IS_BURREL_EMPTY, INPUT); 
  pinMode(LED_WATER_CLOSED, OUTPUT); 
  pinMode(IS_FLOOR_WET, INPUT); 
  pinMode(WATER_PRESSURE, INPUT); 

  Rtc.Begin();
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed s_time_cnt
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 
  Rtc.GetMemory(EERPOM_ADDR_COUNT, (uint8_t*)&s_water_count_rtc, (uint8_t)sizeof(s_water_count_rtc));
  s_water_count = s_water_count_rtc;
  // конфигурирую входные pin для кнопок
  iniInputPin(waterClick, WATER_COUNTER_PIN); 
  iniInputPin(displayClick, DISPLAY_BUTTON_PIN); 
  {
    String r(LOG_MESSAGE SENSOR_NAME DEVICE_NO "/water");
    r += LOG_MESSAGE_END;
    sendToServer(r);
  }
  
  for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix) {
    iniInputPin(freeClick[ix], IN_PIN_START+ix); 
    String r(LOG_MESSAGE SENSOR_NAME DEVICE_NO "/latch-");
    r += String(ix, DEC);
    r += LOG_MESSAGE_END;
    sendToServer(r);
  }

  // locate devices on the bus
  // Start up the library
  sensors.begin();
  s_therm_count = sensors.getDeviceCount();
  if(s_therm_count == 0)
    return;
  // method 1: by index ***
  for(uint8_t ix = 0; ix < s_therm_count; ++ix ) {
    if (!sensors.getAddress(s_thermometer[ix], ix)) 
      memset(&s_thermometer[ix], 0, sizeof(DeviceAddress));
    else {
      // set the resolution to 9 bit per device
      sensors.setResolution(s_thermometer[ix], TEMPERATURE_PRECISION);
      String r(LOG_MESSAGE SENSOR_NAME DEVICE_NO "/DS1820-");
      r += getAddrString(s_thermometer[ix]);
      r += LOG_MESSAGE_END;
      sendToServer(r);
      s_last_temp[ix] = -200;
    }
  }
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
