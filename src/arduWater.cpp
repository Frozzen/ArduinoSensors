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


#define DO_MSG_RATE 600

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
// Module connection pins (Digital Pins)
#define DISPLAY_CLK 4
#define DISPLAY_DIO 3

// выход со счетчика
#define WATER_COUNTER_PIN 5
// показать/убрать показания на дисплее
#define DISPLAY_BUTTON_PIN 6
#define IN_PIN_START 7
// чило сканируемых pin 
#define IN_PIN_COUNT 3
// pin для управления передачей по rs485
#define SerialTxControl 10
// PIN  A4(SDA),A5(SCL) - I2C

#define TEMPERATURE_PRECISION 9
#define DEVICE_NO "0001"
#define RATE  38400
#define MAX_DS1820_COUNT 2
#define PRIORITY_485 4

// адрес куда отправляется сообщение :01 хост
#define ADDR_TO ":01"
#define SENSOR_NAME "ArduWater"

// время на дребезг контактов
#define TIME_TO_RELAX 5
// адреса в eeprom
#define EERPOM_ADDR_COUNT 0

#define LOG_MESSAGE ":01log={\"type\":\"device_connected\",\"message\":\""
#define LOG_MESSAGE_END "\"}"

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

//////////////////////////////////////////////////
// оборачиваем работы в RS485 только на передачу
#define RS485Transmit    HIGH
#define RS485Receive     LOW 
class Serial485 
{
  uint16_t char_time; // время передачи символа в мксек
  public:
    Serial485() {  
    }
  void println(String &str);
  void begin(uint16_t rate) {
      Serial.begin(rate);
      char_time = 1200000 / rate;
      digitalWrite(SerialTxControl, RS485Receive); 
  }
} serial485;

/**
 * применяем метод приоритетного избегания коллизий на шине 
 *  - если есть жанные в буфере подохжать случайное время зависящее от приоритета
 *  - считаем контрольную сумму пакета - чтобы страховаться от коллизий - тогда на приемнике они отбрасываются
 */
void Serial485::println(String &str)
{
   int from = (PRIORITY_485 * 50 * char_time) / 1000+1, to = (PRIORITY_485 * char_time * 80) /1000+1;
   uint8_t n = Serial.available();
   while(n > 0) {
      Serial.flush();
      delay(random(from, to));
      n = Serial.available();
   }
  digitalWrite(SerialTxControl, RS485Transmit);
  Serial.println(str);
  delay(str.length() * char_time / 1000+1); 
  digitalWrite(SerialTxControl, RS485Receive); 
}

// Assign address manually. The addresses below will beed to be changed
// to valid device addresses on your bus. Device address can be retrieved
// by using either oneWire.search(deviceAddress) or individually via
String getAddrString(DeviceAddress &dev)
{
  String r;
  for (uint8_t i = 0; i < 8; i++)   {
    // zero pad the address if necessary
    if (dev[i] < 16) r += "0";
    r += String(dev[i], HEX);
  }
  return r;
}

/**
 * подсчет контрольной суммы и послать на сервер
 */
void sendToServer(String &r)
{
  uint8_t sum = 0;
  for(uint8_t i = 0; i < (uint8_t)r.length(); ++i)
    sum += r[i];
  sum = 0xff - sum;
  r += ":";
  if (sum < 16) r += "0";
  r += String(sum, HEX);
  serial485.println(r);
}

////////////////////////////////////////////////////////

/// сформировать пакет что устройство живо
void doAlive()
{
  String r = (ADDR_TO SENSOR_NAME DEVICE_NO "/INFO/alive=") + String(s_cnt++,DEC);
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
    String r(ADDR_TO SENSOR_NAME DEVICE_NO "/water=");
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
        String r(ADDR_TO SENSOR_NAME DEVICE_NO "/latch-");
        r += String(ix, DEC);
        r += "=" +String(value, DEC);
        sendToServer(r);
      }  
    }
}

/// послать в шину изменение в температуре
bool doSendTemp()
{
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  if(s_therm_count == 0)
    return false;
  sensors.requestTemperatures();

  // print the device information
  bool updated = false;
  for(uint8_t ix = 0; ix < s_therm_count; ++ix ) {
    float tempC = sensors.getTempC(s_thermometer[ix]);
    if(tempC == s_last_temp[ix]) 
      continue;
    // формируем строку с температурой
    String r(ADDR_TO SENSOR_NAME DEVICE_NO "/DS1820-");
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
  randomSeed(analogRead(0));
  // start serial485 port
  serial485.begin(RATE);

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
