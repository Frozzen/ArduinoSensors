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

// serial rate
#define RATE 19200
#if 0
// set jdy40 config
#define JDY_40_SET 7
#endif
// SoftSerial pin
#define ALT_RS232_RX 8
#define ALT_RS232_TX 9

// нужно изменять между образцами - чтобы было поменьше коллизий
#define DO_MSG_RATE 5000
