#include <Arduino.h>
/*
 * измеритель температуры + 4 контакта на замыкание
 * - температурных датчиков ds1820 может быть любое количество
 *  шлет только изменение опрос 30сек
 *   ArduStatHHHH/DS1820-XXXXXXXXXXXXXXX/INFO/resolution=val - один раз при старте
 *   ArduStatHHHH/DS1820-XXXXXXXXXXXXXXX/value=val раз в 30 сек при изменении
 * - контакты опрашивает раз 100мсек
 *   ArduStatHHHH/latch-1/value=val раз в 0.1 сек при изменении
 *   ArduStatHHHH/latch-4/value=val раз в 0.1 сек при изменении
 *  шлет только изменения
 * - раз в 5 секунд шлет keeralive
 *    ArduStatHHHH/INFO/alive=cnt раз в 5
 *    ArduStatHHHH/INFO/count=val - один раз при старте
 * - все сточки закрываются контрольной суммой через :
 */
// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Bounce2.h>

#define DO_MSG_RATE 600
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
// чило сканируемых pin начиная с PIN4-PIN8
#define IN_PIN_START 4
#define IN_PIN_COUNT 4
// pin для управления передачей по rs485
#define SerialTxControl 10 
 
#define TEMPERATURE_PRECISION 9
#define DEVICE_NO "0001"
#define RATE 38400
#define MAX_DS1820_COUNT 5
#define PRIORITY_485 5

#define ADDR_TO ":01"
#define SENSOR_NAME ADDR_TO "ArduStat"

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
uint8_t s_therm_count = 0;
// счетчик keepalive
uint8_t s_cnt = 0;
DeviceAddress s_thermometer[MAX_DS1820_COUNT];
float s_last_temp[MAX_DS1820_COUNT];

Bounce s_input_pin[IN_PIN_COUNT];

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
 * подсчет контрольной суммы
 */
void sendToServer(String &r)
{
  uint8_t sum = 0;
  for(int8_t i = 0; i < r.length(); ++i)
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
  String r(SENSOR_NAME DEVICE_NO "/DS1820-");
  r += getAddrString(dev);
  r += "/INFO/resolution=" +String(sensors.getResolution(dev), DEC);
  sendToServer(r);
}

/// сформировать пакет что устройство живо
void doAlive()
{
  String r = (SENSOR_NAME DEVICE_NO "/INFO/alive=") + String(s_cnt++,DEC);
  sendToServer(r);
}

/// отразить ParasitePower
void doPowerf()
{
  String r(SENSOR_NAME DEVICE_NO "/INFO/ParasitePower=");
  if (sensors.isParasitePowerMode()) 
    r += String("ON");
    else r += String("OFF");
  sendToServer(r);
}

/// послать конфигурацию на хоста
void doConfig()
{
  // Start up the library
  sensors.begin();
  s_therm_count = sensors.getDeviceCount();
  String r(SENSOR_NAME DEVICE_NO "/INFO/count=");
  r = r + String(s_therm_count, DEC);
  sendToServer(r);

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
  String r(SENSOR_NAME DEVICE_NO "/DS1820-");
  r += getAddrString(dev);
  r += "/temp="+String(tempC, 2);
  sendToServer(r);
}

/// формируем строку с состоянием контакта
void doContact(uint8_t pin, bool val)
{
  String r(SENSOR_NAME DEVICE_NO "/latch-");
  r += String(pin, DEC);
  r += "=" +String(val, DEC);
  sendToServer(r);
}

//------------------------------------------------------
/// послать в шину изменения в контакотах
void   doSendContacts(){
  for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix ) {
    boolean changed = s_input_pin[IN_PIN_START+ix].update(); 
    if ( changed ) {
        uint8_t value = s_input_pin[IN_PIN_START+ix].read();
        String r(SENSOR_NAME DEVICE_NO "/latch-");
        r += String(ix, DEC);
        r += "=" +String(value, DEC);
        sendToServer(r);
      }  
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
