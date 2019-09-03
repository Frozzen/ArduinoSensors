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
#define DEBUG

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
const long START_CANDLE_BURN_TIME  = 1000;
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
#define ERR_TMO 1000
int s_temp_error_time = ERR_TMO;  // счетчик времени светить обоими лампами если нет термометра

float getAccVolts()
{
  int v = analogRead(AC_VOLT);
  Serial.print(" acc adc:"); Serial.println(v);    
  return v * 24.5 / 1024.0 + 0.59;
}

void setup(void)
{
  // start serial port
  Serial.begin(9600);
  Serial.println("Pajero Candle Control Block");

  // конфишурим провод управления реле и выключаем его 
  pinMode(RELAY, OUTPUT); 

  pinMode(OVERHEAT, OUTPUT); 
  pinMode(COLD, OUTPUT); 
  pinMode(RED_CANDLE, OUTPUT); 
  pinMode(GREEN_CANDLE, OUTPUT); 

  pinMode(AC_VOLT, INPUT);
  digitalWrite(AC_VOLT, LOW); // disable pull-up

  // выключаем все
  digitalWrite(RELAY, LOW);

  digitalWrite(OVERHEAT, HIGH);
  digitalWrite(COLD, HIGH);
  digitalWrite(RED_CANDLE, HIGH);
  digitalWrite(GREEN_CANDLE, HIGH);

  
  // Start up the library
  sensors.begin();
  if(sensors.getDeviceCount() == 0) {
    s_tempOk = false;  
    s_temp = 20.0;
    s_temp_error_time = ERR_TMO; 
    Serial.println("no temp sensor");
  } else {
    sensors.requestTemperatures(); // Send the command to get temperatures
    s_tempOk = true;
    s_temp = sensors.getTempCByIndex(0);  
    Serial.print("temp:"); Serial.println(s_temp);
    s_temp_error_time = 0; 
  }

  delay(500);
  // низкое напряжение - не даем включать реле
  float volts = getAccVolts();
  Serial.print("acc volts:"); Serial.println(volts);
  if(volts < ACC_LOW || s_temp > COLD_TEMP) {
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
  // TODO сделать более быстро растущую функцию
  float res = map(temp, LOWEST_TEMP, COLD_TEMP, MAX_CANDLE_BURN_TIME, START_CANDLE_BURN_TIME); 
  
  if(res < 0)
    return 0;
  return res;
}

void loop(void)
{
  float temp;
  delay(CANDLE_CHECK_TIME);
   // включаем и выключаем реле каждый оборот - 
  digitalWrite(RELAY, (candle_state == RED_STATE || candle_state == GREEN_STATE) ? HIGH : LOW); 
  if(s_temp_error_time > 0) {
    s_temp_error_time--;
    digitalWrite(OVERHEAT, HIGH); 
    digitalWrite(COLD, HIGH ); 

  } else {
    digitalWrite(OVERHEAT, (s_temp > OVERHEAT_TEMP) ? HIGH : LOW); 
    digitalWrite(COLD, (s_temp < COLD_TEMP) ? HIGH : LOW); 
  }

  digitalWrite(RED_CANDLE, (candle_state == RED_STATE) ? HIGH : LOW); 
  digitalWrite(GREEN_CANDLE, (candle_state == GREEN_STATE) ? HIGH : LOW); 

  // сразу после запуска определяем надо включать свечи и время 
  switch(candle_state) {
    case START_STATE: {
      candle_on_time = calcTime(s_temp);
      red_time = candle_on_time / 3;
      if(candle_on_time > 0.0) {
        candle_state = RED_STATE; 
      } 
    } break;
    case RED_STATE:
      --candle_on_time;
      if(--red_time == 0)
        candle_state = GREEN_STATE;
      break;
    case GREEN_STATE:
      if(--candle_on_time == 0)
        candle_state = OFF_STATE;
      break;
     case OFF_STATE:
      delay(10000);
      if(sensors.getDeviceCount() == 0) {
        s_tempOk = false;  
        s_temp = 20.0;
        Serial.println("no temp sensor");
      } else {
        sensors.requestTemperatures(); // Send the command to get temperatures
        s_tempOk = true;
        s_temp = sensors.getTempCByIndex(0);  
        Serial.print("temp:"); Serial.println(s_temp);
        s_temp_error_time = 0; 
      }
     break;
  }

  Serial.print("candle_state:"); Serial.print(candle_state); Serial.print(" time="); Serial.print(candle_on_time); 
 }
