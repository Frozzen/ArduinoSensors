#define SENSOR_NAME "ArduStat"
#define DEVICE_NO "02"
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define FOTO_SENSOR A1
// чило сканируемых pin начиная с PIN3-PIN7
#define IN_PIN_START 3
#define IN_PIN_COUNT 4
 
#define TEMPERATURE_PRECISION 9
#define MAX_DS1820_COUNT 2


// послать проверить жив ли
#define ALIVE_CMD "03"
// послать конфигурацию
#define CONF_CMD "02"
// послать состояние
#define SEND_CMD "01"
// адрес от кого запрос 01 сервер
#define ADDR_FROM "01"
// этот адрес надо менять по платам
#define SEND_DATA_CMD ":" DEVICE_NO ADDR_FROM SEND_CMD ";"
#define CONF_DATA_CMD ":" DEVICE_NO ADDR_FROM CONF_CMD ";"
#define ALIVE_DATA_CMD ":" DEVICE_NO ADDR_FROM ALIVE_CMD ";"
// команды :020101; :020102; :020103;

// serial rate
#define RATE 38400
// set jdy40 config
#define JDY_40_SET 7
// SoftSerial pin
#define ALT_RS232_RX 8
#define ALT_RS232_TX 9
