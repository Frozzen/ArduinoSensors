/**
 * блок управления свечами для Pajero
 *
 * ** TODO перенести контроль питания на A1 и сделать выход шины I2C
 *
 * при влючении питания если алгоритм позволяет включает реле. большие токи делаем 
 * логика попрощще чтобы уменьшить верятность ошибки
 * мониторит температуру двигателя и зажигает лампочки, на старте проверяет чтобы аккум был не севший
 * 
 * напряжение измерять только в начале работы.  Если напряжение аккумулятора меньше 10.5 вольт не включать свечи
 * фильтровать темепературу фильтром кальмана - исключать изменение быстрее 1 град за отсчет
 * в конечном цикле измерять температуру раз в секунду
 * 
 * свечи включены 
 * 120 сек на -20град, 
 * 0сек если двигатель теплее 40 град 
 * время погасания лампочки прогрева до старта зависит от температуры двигателя - 1/3 времени работы свечей
 * лампочка двигатель холодный < 40 град
 * лампочка перегрев > 105 град
 * включаются при включении зажигания
 * питание подается при On
 ** выключение On выключает сразу
 * 
 * резистор на  LED
 * - зеленый 100ohm
 * - красный 1k
 * - синий 5к
 * 
 * клемы под винт/зажим:
 * - управление реле
 * - земля
 * - 12V
 * 
 * штырьковая клема
 * - ключ On 
 * - LED зеленый свечи
 * - LED красный свечи
 * - LED перегрев
 * - LED холодный
 * - 
 */

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
//#include <PPanel.h>

//#define USE_I2C_LED
//#define DEBUG

// PINS
// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 2
// провод управления реле свечей
#define RELAY        6
// LED
#define COLD         7
#define GREEN_CANDLE 8 
#define RED_CANDLE   9
#define OVERHEAT     10

#define AC_VOLT      A1

// предельные температуры
const int OVERHEAT_TEMP = 105;
const int COLD_TEMP = 40;
const float LOWEST_TEMP = -25.0;
// максимальное время горения свечей
const long MAX_CANDLE_BURN_TIME  = 120000;
// низкое напряжение не включать свечи 
const float ACC_LOW = 10.5;
// задержка цикла опроса в милисекундах 
const int CANDLE_CHECK_TIME = 200;

// состояния FSM  
enum eStates {
  START_STATE,
  RED_STATE,
  GREEN_STATE,
  OFF_STATE
};

// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

int candle_state = START_STATE;    // -1 начальное, по получению температуры будет решение включать или нет
long candle_on_time = 0, // время которое реле включено в milisecs сек
    red_time = 0;        // время которое будет гореть красная лампочка свечей
float s_temp = 0;      // температура за прошлый цикл измерений
bool s_tempOk = true;

float getAccVolts()
{
    return analogRead(AC_VOLT) * 35.5 / 1024.0 + 0.59;
}

void setup(void)
{
  // start serial port
  Serial.begin(9600);
  Serial.println("Pajero Candle Control Block");

  // конфишурим провод управления реле и выключаем его 
  pinMode(RELAY, OUTPUT); 
#ifndef USE_I2C_LED  
  pinMode(OVERHEAT, OUTPUT); 
  pinMode(COLD, OUTPUT); 
  pinMode(RED_CANDLE, OUTPUT); 
  pinMode(GREEN_CANDLE, OUTPUT); 
#endif  
  pinMode(AC_VOLT, INPUT);
  digitalWrite(AC_VOLT, LOW); // disable pull-up

  // выключаем все
  digitalWrite(RELAY, LOW);
#ifndef USE_I2C_LED
  digitalWrite(OVERHEAT, HIGH);
  digitalWrite(COLD, HIGH);
  digitalWrite(RED_CANDLE, HIGH);
  digitalWrite(GREEN_CANDLE, HIGH);
#endif  
  
  // Start up the library
  sensors.begin();
  if(sensors.getDeviceCount() == 0) {
    s_tempOk = false;  // TODO зажечь обе лампочки - синюю и красную
    s_temp = 20.0;
  } else {
    sensors.requestTemperatures(); // Send the command to get temperatures
    s_tempOk = true;
    s_temp = sensors.getTempCByIndex(0);  
  }

#ifdef USE_I2C_LED
  // отдаем по i2c состояние свечей и напряжение сети по старту
  Wire.begin(I2C_SLAVE_ADDRESS_TEMP_VOLT);                // join i2c bus with address #2
  Wire.onReceive (receiveEvent);  // interrupt handler for incoming messages
  Wire.onRequest (requestEvent);  // interrupt handler for when data is wanted
#endif

  // низкое напряжение - не даем включать реле
  float volts = getAccVolts();
  if(volts < ACC_LOW) {
    candle_state = OFF_STATE;
    candle_on_time = 0;
    red_time = 0;
  } else 
    candle_state = START_STATE;
}

/**
 * вычисляем время работы в зависимости от температуры
 */
float calcTime(float temp)
{
  float res = map(temp, LOWEST_TEMP, COLD_TEMP, MAX_CANDLE_BURN_TIME, 0); // сделать более быстро растущую функцию
  
  if(res < 0)
    return 0.0;
  return res;
}

void loop(void)
{
  float temp;
  delay(CANDLE_CHECK_TIME);

  // сразу после запуска определяем надо включать свечи и время 
  if(candle_state == START_STATE) {
    candle_on_time = calcTime(s_temp);
    red_time = candle_on_time / 3;
    if(candle_on_time > 0.0)
      candle_state = RED_STATE;
    else
      candle_state = OFF_STATE;
  } 
  // горит красная лампочка - надо ждать - заводить рано
  else if(candle_state == RED_STATE) {
    candle_on_time -= CANDLE_CHECK_TIME;
    red_time -= CANDLE_CHECK_TIME;
    if(red_time <= 0) {
      red_time = 0;
      candle_state = GREEN_STATE;
    }
  } 
  // прогрев свечей на работающем движке
  else if(candle_state == GREEN_STATE) {
    candle_on_time -= CANDLE_CHECK_TIME;
    if(candle_on_time <= 0) {
      candle_state = OFF_STATE;
      candle_on_time = 0;
    }
  } 
  else if(candle_state == OFF_STATE) {
    // ждем в среднем секунду вносим случайность чтобы разбить блокирование шины
    delay(1000 + random(CANDLE_CHECK_TIME, 500));
    if(sensors.getDeviceCount() == 0) {
      s_tempOk = false;
    } else {
      sensors.requestTemperatures(); // Send the command to get temperatures
      s_tempOk = true;
      temp = sensors.getTempCByIndex(0);  
    }

    // фильтр для Кальмана отбрасываем неправильные измерения
    if(abs(temp - s_temp) < 5.0) {
      s_temp = temp;  
    }
  }

  // включаем и выключаем реле каждый оборот - 
  digitalWrite(RELAY, (candle_state == RED_STATE || candle_state == GREEN_STATE) ? HIGH : LOW); 
  
  digitalWrite(OVERHEAT, (s_tempOk && (s_temp > OVERHEAT_TEMP)) ? HIGH : LOW); 
  digitalWrite(COLD, (s_tempOk && (s_temp < COLD_TEMP)) ? HIGH : LOW); 

  digitalWrite(RED_CANDLE, (candle_state == RED_STATE) ? HIGH : LOW); 
  digitalWrite(GREEN_CANDLE, (candle_state == GREEN_STATE) ? HIGH : LOW); 
#ifdef USE_I2C_LED  
  // не заливать шину данными - оправлять при изменении и раз в 1 секунд  + rand(0.5)
  byte t = ((s_tempOk && (s_temp > OVERHEAT_TEMP)) ? PANEL::MASK_OVERHEAT : 0) | 
          ((s_tempOk && (s_temp < COLD_TEMP)) ? PANEL::MASK_COLD : 0) | 
        ((candle_state == RED_STATE) ? PANEL::MASK_RED_CANDLE : 0) |
        ((candle_state == GREEN_STATE) ? PANEL::MASK_GREEN_CANDLE : 0);
  PPanel::writeLED(t);
#endif
}

#ifdef USE_I2C_LED  
byte command;
//***************************************************************************
// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void receiveEvent(int)
{
  command = Wire.read(); // receive byte subaddress
  switch(command) {
  case RCB::I2C_CMD_RELE_SET: 
    candle_state = RED_STATE;
    candle_on_time = min(Wire.read() * 1000, MAX_CANDLE_BURN_TIME);
    red_time = candle_on_time / 3;
    break;
  default: ;
  } 
}

//***************************************************************************
void requestEvent ()
{
  switch (command)
     {
      case RCB::I2C_CMD_TEMP: 
        Wire.write((char)s_temp);
        break;
      case RCB::I2C_CMD_VOLT: {
        byte volt = getAccVolts() * 10;
        Wire.write(volt);
        }
        break;
      case RCB::I2C_CMD_STATE: 
        Wire.write((s_tempOk ? 0 : RCB::BAD_TEMP_STATE) | 
                  (candle_state == RED_STATE ? RCB::RED_LED_CANDLE_STATE : 0) |
                  (candle_state == GREEN_STATE ? RCB::GREEN_LED_CANDLE_STATE : 0));
        break;      
      default:;
     } 
}
#endif
