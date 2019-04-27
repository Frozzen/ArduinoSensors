
#define MAX_OUT_BUFF 100
#define HOST_ADDR ":01"
#define ADDR_STR HOST_ADDR SENSOR_NAME DEVICE_NO
extern char s_buf[MAX_OUT_BUFF];  

#define SENSOR_NAME "ArduWater"
// команды с хоста
#define DEVICE_NO "03"
// установить время ":" DEVICE_NO SET_TIME "JUL 27 2019;23:00:00" 
#define SET_TIME "01"
// установить счетчик воды ":" DEVICE_NO SET_WATER "0000001000" 
#define SET_WATER "02"
// управление краном воды ":" DEVICE_NO SET_TAP "0" / "1" 
#define SET_TAP "03"

#define IN_PIN_COUNT 7
#define NO_TEMP

// кнопка открыть/закрыть воду
#define BTN_OPEN_WATER  2 
// Module connection pins (Digital Pins)
#define DISPLAY_DIO     3
#define DISPLAY_CLK     4

// выход со счетчика
#define WATER_COUNTER_PIN 5
// показать/убрать показания на дисплее
#define BTN_DISPLAY     6
// бочка пустая
#define IS_BURREL_EMPTY 7
// бочка полная
#define IS_BURREL_FULL  A3
// кран открыт
#define IS_TAP_OPEN     A2
// кран закрыт
#define IS_TAP_CLOSE    10
// закрыть кран
#define OPEN_TAP_CMD    11
// открыть кран
#define CLOSE_TAP_CMD   12

// PIN  A4(SDA),A5(SCL) - I2C - display
//the YELLOW pin of the Sensor is connected with A2 of Arduino/Catduino
#define WATER_PRESSURE A1 

// адреса в eeprom
#define EERPOM_ADDR_COUNT 0

#define USE_SERIAL
#define DO_MSG_RATE 50000
#define TAP_TIMEOUT 10000