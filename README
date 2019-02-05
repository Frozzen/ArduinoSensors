В git разнес по веткам - их не нужно сливать - там разные проекты, хотя по именам не 
пересекаются. кроме readme

# железо
    * нужен RTC, там же и память. записывать значение в eeprom. ​exmpl1 ​exmpl2
    показывает расход на 7 сегментнике - ddd.d куб.м ​Часы на Arduino используя DS3231
    * Берется счетчик с импульсным выходом. Один импульс на 10л воды. #36
    На ардуине импульсы заводятся на прерывания. В цикле 0.1 сек они проверяются если изменились, то каждые 5 минут записываются в eprom и передаются в брокера. Там же могут быть термометры - на подвал и воду. отличается от ArduSens
    * сделать кнопку показать на дисплее значение 

# используемые pin
```
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
// Module connection pins (Digital Pins)
#define DISPLAY_CLK 3
#define DISPLAY_DIO 4

// выход со счетчика
#define WATER_COUNTER_PIN 5
// показать/убрать показания на дисплее
#define DISPLAY_BUTTON_PIN 6
#define IN_PIN_START 7
// чило сканируемых pin 
#define IN_PIN_COUNT 3
// pin для управления передачей по rs485
#define SerialTxControl 10
// PIN  A4(SDA),A5(SCL) - I2C
```
* A4(SDA),A5(SCL) - I2C

#MQTT topic

стандартные от ArduSens. меняется название датчика и добавляется сообщение

    HHHH - номер платы
    XXXXXXXXXXXXXXX адрес датчика
    N номер контакта 

    :01ArduWaterHHHH/water=val - сколько насчитано литров/10
    :01ArduWaterHHHH/INFO/count=val - число термометров один раз при старте
    :01ArduWaterHHHH/DS1820-XXXXXXXXXXXXXXX/INFO/resolution=val - один раз при старте 
    :01ArduWaterHHHH/DS1820-XXXXXXXXXXXXXXX/temp=val - температура. раз в 60 сек при изменении
    :01ArduWaterHHHH/latch-N=val - состояние контакта раз в 0.1 сек
    :01ArduWaterHHHH/INFO/alive=cnt раз в 60 сек счетчик на каждое сообщение +1(%256) 

