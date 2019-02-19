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
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Bounce2.h>

#define DO_MSG_RATE 100
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define FOTO_SENSOR A1
// чило сканируемых pin начиная с PIN4-PIN8
#define IN_PIN_START 6
#define IN_PIN_COUNT 4
 
#define TEMPERATURE_PRECISION 9
#define DEVICE_NO "0001"
#define MAX_DS1820_COUNT 3

#define ADDR_TO ":01"
#define SENSOR_NAME "ArduStat"

// время на дребезг контактов
#define TIME_TO_RELAX 5

#define LOG_MESSAGE ":01log={\"type\":\"device_connected\",\"message\":\""
#define LOG_MESSAGE_END "\"}"

//  управления передачей по rs485
#define PRIORITY_485 5
#include "../../rs485.cpp"

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

Bounce s_input_pin[IN_PIN_COUNT];

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
uint8_t s_therm_count = 0, s_last_light = 0;
// счетчик keepalive
uint8_t s_time_cnt = 0;
DeviceAddress s_thermometer[MAX_DS1820_COUNT];
float s_last_temp[MAX_DS1820_COUNT];

////////////////////////////////////////////////////////

/// сформировать пакет что устройство живо
void doAlive()
{
  String r = (ADDR_TO SENSOR_NAME DEVICE_NO "/INFO/alive=") + String(s_time_cnt++, DEC);
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
  for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix ) {
    boolean changed = s_input_pin[ix].update(); 
    if ( changed ) {
        uint8_t value = s_input_pin[ix].read();
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
  bool updated = false;
  uint16_t adc_value = analogRead(FOTO_SENSOR);
  {
    // формируем строку с температурой
    String r(ADDR_TO SENSOR_NAME DEVICE_NO "/light=");
 		// Расчет напряжения во входе ADC
		double voltage = 5.0 - 5.0 * ((double)adc_value / 1024.0);
 		// Измерение сопротивления фоторезистора в делителе напряжения
		double resistance = (10.0 * 5.0) / voltage - 10.0;
 		// Вычисление освещенности в люксах		
		double illuminance = 255.84 * pow(resistance, -10/9);
    r += String(illuminance);
    s_last_light = adc_value;
    sendToServer(r);
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

//------------------------------------
void setup(void)
{
  // start serial485 port
  serial485.begin();
  {
    String r(LOG_MESSAGE SENSOR_NAME DEVICE_NO "/light" LOG_MESSAGE_END);
    sendToServer(r);
  }
 
   for(uint8_t ix = 0; ix < IN_PIN_COUNT; ++ix) {
    iniInputPin(s_input_pin[ix], IN_PIN_START+ix); 
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
  doTestContacts();
  if((s_time_cnt % DO_MSG_RATE) == 0) {
    if(!doSendTemp())
      doAlive();    
  }

  delay(100);
  s_time_cnt++;
}
