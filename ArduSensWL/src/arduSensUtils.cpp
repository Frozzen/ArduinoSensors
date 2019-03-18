#include <Arduino.h>
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
 * - все сточки закрываются контрольной суммой через :
 */
// Include the libraries we need
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Bounce2.h>
#include "ardudev.h"
#include "arduSensUtils.h"


// время на дребезг контактов
#define TIME_TO_RELAX 5

#define LOG_MESSAGE HOST_ADDR "log={\"type\":\"device_connected\",\"message\":\""
#define LOG_MESSAGE_END "\"}"

#define ADDR_STR HOST_ADDR SENSOR_NAME DEVICE_NO
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

Bounce s_input_pin[IN_PIN_COUNT];

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
uint8_t s_therm_count = 0;
DeviceAddress s_thermometer[MAX_DS1820_COUNT];

char s_buf[MAX_OUT_BUFF];
////////////////////////////////////////////////////////
// Assign address manually. The addresses below will beed to be changed
// to valid device addresses on your bus. Device address can be retrieved
// by using either oneWire.search(deviceAddress) or individually via
const char *getAddrString(DeviceAddress &dev)
{
  static char _buf[17];
  for (uint8_t i = 0; i < 8; i++)   {
    sprintf(_buf+i*2, "%02x", (char)dev[i]);
  }
  _buf[16] = 0;
  return _buf;
}

/// сформировать пакет что устройство живо
void doAlive()
{
  snprintf(s_buf, sizeof(s_buf), ADDR_STR "/alive=%d", s_time_cnt++);
  sendToServer(s_buf, true);
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
  for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix ) {
    boolean changed = s_input_pin[ix].update(); 
    if ( changed ) {
        uint8_t value = s_input_pin[ix].read();
        snprintf(s_buf, sizeof(s_buf), ADDR_STR "/latch-%d=%d", ix, value);
        sendToServer(s_buf);
      }  
    }
}

/// послать в шину изменение в температуре
bool doSendTemp()
{
  bool updated = false;
  uint16_t adc_value = analogRead(FOTO_SENSOR);
  {
    // формируем строку с температурой
 		// Расчет напряжения во входе ADC
		double voltage = 5.0 - 5.0 * ((double)adc_value / 1024.0);
 		// Измерение сопротивления фоторезистора в делителе напряжения
		double resistance = (10.0 * 5.0) / voltage - 10.0;
 		// Вычисление освещенности в люксах		
		double illuminance = 255.84 * pow(resistance, -10/9);
    char str_temp[6];
    dtostrf((float)illuminance, 4, 1, str_temp);
    snprintf(s_buf, sizeof(s_buf), ADDR_STR "/light=%s", str_temp);
    sendToServer(s_buf);
    updated = true;
  }
   
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  if(s_therm_count == 0)
    return updated;
  sensors.requestTemperatures();

  // print the device information
  for(uint8_t ix = 0; ix < s_therm_count; ++ix ) {
    float tempC = sensors.getTempC(s_thermometer[ix]);
    if(tempC > 67.0)
      continue;
    // формируем строку с температурой
    char str_temp[6];
    dtostrf(tempC, -5, 1, str_temp);
    snprintf(s_buf, sizeof(s_buf), ADDR_STR "/DS1820-%s/temp=%s",
      getAddrString(s_thermometer[ix]), str_temp);
    sendToServer(s_buf);
    updated = true;
  }
  return updated;
}

//------------------------------------
void setupArduSens(void)
{
  snprintf(s_buf, sizeof(s_buf),LOG_MESSAGE SENSOR_NAME DEVICE_NO "/light" LOG_MESSAGE_END);
  sendToServer(s_buf, true);
 
   for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix) {
    iniInputPin(s_input_pin[ix], IN_PIN_START+ix); 
    snprintf(s_buf, sizeof(s_buf), LOG_MESSAGE SENSOR_NAME DEVICE_NO "/latch-%d" LOG_MESSAGE_END, ix);
    sendToServer(s_buf, true);
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
      snprintf(s_buf, sizeof(s_buf), LOG_MESSAGE SENSOR_NAME DEVICE_NO "/DS1820-%s" LOG_MESSAGE_END,
        getAddrString(s_thermometer[ix]));
      sendToServer(s_buf, true);
    }
  }
}
