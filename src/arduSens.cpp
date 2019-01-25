#include <Arduino.h>
/*
 * измеритель температуры + 4 контакта на размыкание
 * - температурных датчиков ds1820 может быть любое количество
 *  шлет только изменение опрос 30сек
 *   ArduStatHHHH/DS1820-XXXXXXXXXXXXXXX/INFO/resolution=val - один раз при старте
 *   ArduStatHHHH/DS1820-XXXXXXXXXXXXXXX/value=val раз в 30 сек при изменении
 * - контакты опрашивает раз 100мсек
 *   ArduStatHHHH/latch-1/value=val раз в 0.1 сек при изменении
 *   ArduStatHHHH/latch-4/value=val раз в 0.1 сек при изменении
 *  шлет только изменения
 * - раз в 5 секунд шлет keeralive
 *    ArduStatHHHH/INFO/alive=1 раз в 5
 *    ArduStatHHHH/INFO/count=val - один раз при старте
 * - все сточки закрываются контрольной суммой через :
 */
// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9
#define DEVICE_NO "0001"
#define RATE 38400
#define MAX_DS1820_COUNT 8
#define IN_PIN_COUNT 4
#define SerialTxControl 10 

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
uint8_t s_therm_count = 0;
DeviceAddress s_thermometer[MAX_DS1820_COUNT];
float s_last_temp[MAX_DS1820_COUNT];

uint8_t s_last_input_pin[IN_PIN_COUNT];

//////////////////////////////////////////////////
// оборачиваем работы в RS485 только на передачу
#define RS485Transmit    HIGH
#define RS485Receive     LOW 
class Serial485 
{
  uint8_t priority;
  uint16_t char_time; // время передачи символа в мксек
  public:
    Serial485(): priority(5) {  
    }
  void println(String str);
  void begin(uint16_t rate) {
      Serial.begin(rate);
      char_time = 1200000 / rate;
      digitalWrite(SerialTxControl, RS485Receive); 
  }
} serial485;

void Serial485::println(String str)
{
   int from = (priority * char_time * 50) /1000+1, to = (priority * char_time * 80) /1000+1;
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
// sensors.getAddress(deviceAddress, index)
// DeviceAddress insideThermometer = { 0x28, 0x1D, 0x39, 0x31, 0x2, 0x0, 0x0, 0xF0 };
// DeviceAddress outsideThermometer   = { 0x28, 0x3F, 0x1C, 0x31, 0x2, 0x0, 0x0, 0x2 };

String getAddrString(DeviceAddress dev)
{
  String r;
  for (uint8_t i = 0; i < 8; i++)   {
    // zero pad the address if necessary
    if (dev[i] < 16) r += "0";
    r += String(dev[i], HEX);
  }
  return r;
}

String doCS(String r)
{
  uint8_t sum = 0;
  for(int8_t i = 0; i < r.length(); ++i)
    sum += r[i];
  sum = 0xff - sum;
  r += ":";
  if (sum < 16) r += "0";
  r += String(sum, HEX);
  return r;
}
////////////////////////////////////////////////////////

String doResolution(DeviceAddress dev)
{
  String r("ArduStat" DEVICE_NO "/DS1820-");
  r += getAddrString(dev);
  r += "/INFO/resolution=" +String(sensors.getResolution(dev), DEC);
  return doCS(r);
}

String doAlive()
{
  String r("ArduStat" DEVICE_NO "/INFO/alive=1");
  return doCS(r);
}

String doConfig()
{
  s_therm_count = sensors.getDeviceCount();
  String r("ArduStat" DEVICE_NO "/INFO/count=");
  r += String(s_therm_count, DEC);

  // method 1: by index
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
      pinMode(PIN3+ix, INPUT); 
      digitalWrite(PIN3+ix, HIGH);
  }
  return doCS(r);
}

String doPowerf()
{
  String r("ArduStat" DEVICE_NO "/INFO/ParasitePower=");
  if (sensors.isParasitePowerMode()) 
    r += String("ON");
    else r += String("OFF");
  return doCS(r);
}

//--------------------------------------------------------------
String doThermData(DeviceAddress dev, float tempC)
{
  String r("ArduStat" DEVICE_NO "/DS1820-");
  r += getAddrString(dev);
  r += "/value="+String(tempC, 2);
  return doCS(r);
}

String doContact(uint8_t pin, bool val)
{
  String r("ArduStat" DEVICE_NO "/latch-");
  r += String(pin, DEC);
  r += "/value=" +String(val, DEC);
  return doCS(r);
}

//------------------------------------------------------
void   doSendContacts(){
  for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix ) {
      uint8_t val = digitalRead(PIN3+ix); 
      if(val == s_last_input_pin[ix]) continue;
      doContact(ix, val);
      s_last_input_pin[ix] = val;
  }
}

void doSendTemp()
{
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures();

  // print the device information
  for(uint8_t ix = 0; ix < s_therm_count; ++ix ) {
    float tempC = sensors.getTempC(s_thermometer[ix]);
    if(tempC == s_last_temp[ix]) continue;
    serial485.println(doThermData(s_thermometer[ix], tempC)); 
    s_last_temp[ix] = tempC;
  }

}

//------------------------------------
void setup(void)
{
  randomSeed(analogRead(0));
  // start serial485 port
  serial485.begin(38400);

  // Start up the library
  sensors.begin();

  // locate devices on the bus
  serial485.println(doConfig());

  // report parasite power requirements
  serial485.println(doPowerf());
}

uint8_t state = 0;
/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  if((state & 0xf) == 0) {
    doAlive();
  }
  if(state == 0) {
    doSendTemp();
  }
  doSendContacts();
  state++;

  delay(100);
}
