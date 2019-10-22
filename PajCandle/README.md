# блок управления свечами для Pajero
 
мониторит температуру двигателя и зажигает лампочки, на старте проверяет чтобы аккум был
не севший. напряжение измерять только в начале работы.  Если напряжение аккумулятора меньше 10.5 вольт не включать свечи


фильтровать темепературу фильтром кальмана - исключать изменение быстрее 1 град за отсчет
в конечном цикле измерять температуру раз в секунду

# вычисление времени включения свечей
 * от 180 сек на -20град, 
 * 0 сек если двигатель теплее 40 град 
 * функция логарифмическая - прикинуть кривизну
 *  время погасания лампочки прогрева до старта зависит от температуры двигателя - 1/3 времени работы свечей

## лампочки 
 * нет датчика температуры: зажечь обе лампочки - синюю и красную
 * лампочка двигатель холодный < 40 град
 * лампочка перегрев > 105 град еще не критичный перегрев
 * включаются обе при включении зажигания
 * питание подается при On
 * выключение On выключает сразу
 
## резистор на  LED
 - зеленый 100ohm
 - красный 1k
 - синий 5к

## клемы под винт/зажим:
- управление реле
- земля
- 12V
 
## штырьковая клема
 - ключ On 
 - LED зеленый свечи
 - LED красный свечи
 - LED перегрев
 - LED холодный

## предельные температуры
const int OVERHEAT_TEMP = 105;
const int COLD_TEMP = 40;
// максимальное время горения свечей
const long MAX_CANDLE_BURN_TIME  = 120000;
// низкое напряжение не включать свечи 
const float ACC_LOW = 10.5;
// задержка цикла опроса в милисекундах 
const int CANDLE_CHECK_TIME = 200;
 

# PINS выводы 
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

# развитие

## хотел сделать чтобы можно было запросить параметры из устройства.
 * - датчик температуры на двигателе
 * I2C slave 0x4
 * субадреса для мастера
 * 1 - wr включить реле
 * 2 - rd получить температуру
 * 3 - rd получить напряжение * 10v
 * 4 - rd получить состояние устройства
  *      0x1 - бит прогрев свечей до старта
  *      0x2 - бит прогрев свечей на работающем
  *      0x4 - бит = 0 датчик температуры исправен