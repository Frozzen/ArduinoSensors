#include <Arduino.h>
/**
* Берется счетчик с импульсным выходом. Один импульс на 10л воды. #36
* На ардуине импульсы заводятся на прерывания. В цикле 0.1 сек они проверяются если изменились, 
  то каждые 5 минут записываются в eprom и передаются в брокера.  
  Там же могут быть термометры - на подвал и воду. отличается от ArduSens
* показывает расход на 7 сегментнике - ddd.d куб.м  DS3231
*  нужен RTC, там же и память. записывать значение в eeprom. RTC_DS1307
* сделать кнопку показать на дисплее значение 
* = используемые pin =
  2 - interrupt int0 
  ONE_WIRE_BUS 3
  TM1637 4,5 - display
  SerialTxControl? 10
  PIN6 - показать данные на дисплее
  PIN7-PIN9 для датчиков состояния
  A4(SDA),A5(SCL) - I2C

* = MQTT topic =
* стандартные от ArduSens. меняется название датчика и добавляется сообщение
* HHHH - номер платы 
* XXXXXXXXXXXXXXX адрес датчика 
* N номер контакта
* ArduWaterHHHH/water/value=val - сколько насчитано литров/10
* ArduWaterHHHH/INFO/count=val - число термометров один раз при старте 
* ArduWaterHHHH/DS1820-XXXXXXXXXXXXXXX/INFO/resolution=val - один раз при старте * ArduWaterHHHH/DS1820-XXXXXXXXXXXXXXX/value=val - температура. раз в 60 сек при изменении 
* ArduWaterHHHH/latch-N/value=val - состояние контакта раз в 0.1 сек 
* ArduWaterHHHH/INFO/alive=cnt раз в 60 сек счетчик на каждое сообщение +1(%256) 
* - все сточки закрываются контрольной суммой через :
 */
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS1307.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TM1637Display.h>

#define DO_MSG_RATE 600
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 3
// Module connection pins (Digital Pins)
#define CLK 4
#define DIO 5
// чило сканируемых pin начиная с PIN4-PIN8
#define IN_PIN_START 6
#define IN_PIN_COUNT 3
// pin для управления передачей по rs485
#define SerialTxControl 10 
 
#define TEMPERATURE_PRECISION 9
#define DEVICE_NO "0001"
#define RATE 38400
#define MAX_DS1820_COUNT 2
#define PRIORITY_485 4

// Setup  instancees to communicate with devices
RtcDS1307<TwoWire> Rtc(Wire);
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

TM1637Display display(CLK, DIO);

// arrays to hold device addresses
uint8_t s_therm_count = 0;
// счетчик keepalive
uint8_t s_cnt = 0;
DeviceAddress s_thermometer[MAX_DS1820_COUNT];
float s_last_temp[MAX_DS1820_COUNT];

uint8_t s_last_input_pin[IN_PIN_COUNT];

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
      Serial.read();
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
 * подсчет контрольной суммы
 */
void doCS(String &r)
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
// получаем разрешение устройства по Т
void doResolution(DeviceAddress &dev)
{
  String r("ArduWater" DEVICE_NO "/DS1820-");
  r += getAddrString(dev);
  r += "/INFO/resolution=" +String(sensors.getResolution(dev), DEC);
  doCS(r);
}

/// сформировать пакет что устройство живо
void doAlive()
{
  String r = ("ArduWater" DEVICE_NO "/INFO/alive=") + String(s_cnt++,DEC);
  doCS(r);
}

/// отразить ParasitePower
void doPowerf()
{
  String r("ArduWater" DEVICE_NO "/INFO/ParasitePower=");
  if (sensors.isParasitePowerMode()) 
    r += String("ON");
    else r += String("OFF");
  doCS(r);
}

/// послать конфигурацию на хоста
void doConfig()
{
  // Start up the library
  sensors.begin();
  s_therm_count = sensors.getDeviceCount();
  String r("ArduWater" DEVICE_NO "/INFO/count=");
  r = r + String(s_therm_count, DEC);
  doCS(r);

  // method 1: by index ***
  for(uint8_t ix = 0; ix < s_therm_count; ++ix ) {
    if (!sensors.getAddress(s_thermometer[ix], ix)) 
      memset(&s_thermometer[ix], 0, sizeof(DeviceAddress));
    else {
      // set the resolution to 9 bit per device
      sensors.setResolution(s_thermometer[ix], TEMPERATURE_PRECISION);
      doResolution(s_thermometer[ix]);
      s_last_temp[ix] = -200;
    }
  }
  for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix) {
      pinMode(IN_PIN_START+ix, INPUT); 
      digitalWrite(IN_PIN_START+ix, HIGH);
  }
  // report parasite power requirements
  doPowerf();
}

//--------------------------------------------------------------
/// формируем строку с температурой
void doThermData(DeviceAddress &dev, float tempC)
{
  String r("ArduWater" DEVICE_NO "/DS1820-");
  r += getAddrString(dev);
  r += "/value="+String(tempC, 2);
  doCS(r);
}

/// формируем строку с состоянием контакта
void doContact(uint8_t pin, bool val)
{
  String r("ArduWater" DEVICE_NO "/latch-");
  r += String(pin, DEC);
  r += "/value=" +String(val, DEC);
  doCS(r);
}

//------------------------------------------------------
/// послать в шину изменения в контакотах
void   doSendContacts(){
  for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix ) {
      uint8_t val = digitalRead(IN_PIN_START+ix); 
      if(val == s_last_input_pin[ix]) continue;
      doContact(ix, val);
      s_last_input_pin[ix] = val;
  }
}

/// послать в шину изменение в температуре
void doSendTemp()
{
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures();

  // print the device information
  for(uint8_t ix = 0; ix < s_therm_count; ++ix ) {
    float tempC = sensors.getTempC(s_thermometer[ix]);
    if(tempC == s_last_temp[ix]) continue;
    doThermData(s_thermometer[ix], tempC); 
    s_last_temp[ix] = tempC;
  }

}

//------------------------------------
void setup(void)
{
  randomSeed(analogRead(0));
  // start serial485 port
  serial485.begin(9600);

  // locate devices on the bus
  doConfig();
}
uint16_t state = 0;
/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  if((state % DO_MSG_RATE) == 0) {
    doAlive();
    doSendTemp();
  }
  doSendContacts();
  state++;

  delay(100);
}
