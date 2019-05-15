# Переслать сообщения из com порта в mqtt


## командные опции
* port - по умолчанию "/dev/ttyUSB0"
* baud - скорость по умолчанию 38400
* "s,stdout", "Log to stdout")
*  ("d,debug", "Enable debugging")
*  ("p,port", "com port", cxxopts::value<std::string>(port))
*  ("b,baud", "com port speed", cxxopts::value(baud));

## конфигурационный файл
используется файл "mqttfrwd.json" в текущей папке
используется
* "COM", "topic_head" - корневой mqtt topic
* "MQTT", "server"
* "MQTT", "user" "MQTT", "pass"

## MQTT сообщения
log/com2mqtt - lwt или время старта
mqtt::message willmsg(topic_head + "log/com2mqtt", "com2mqtt terminated", 1, true);


## сообщение на com
пример сообщения
```
:01log={"type":"device_connected","message":"ArduStat02/latch-2"};f0
:01log={"type":"device_connected","message":"ArduStat02/latch-3"};ef
:01ArduStat02/light=15.5;8d
:01ArduStat02/DS1820-28ffff45ff0c0257/temp=21.5 ;7c
```
подсчет контрольной суммы пакета
```
int csorg = strtol(ss.c_str(), &e, 16);
uint8_t cs = 0;
for (auto it = str.begin(); it < str.end()-3; ++it) {
    cs += (int8_t)*it;
}
cs = 0xff - cs;
```
