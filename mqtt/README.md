# описание как зарегить
sudo vi /etc/systemd/system/mqtt-forward@.service:
```
[Unit]
Description=Copying events from COM to mqtt
Requires=syslog.socket

[Service]
Type=simple
GuessMainPID=false
User=pi
Group=pi
#Environment=
ExecStart=/usr/bin/python mqtt-frwd.py %I
#activating=udev
WorkingDirectory=/home/pi

[Install]
WantedBy=multi-user.target
Alias=mqtt-forward.service
```

pi@water:~ $ cat /etc/udev/rules.d/95-mqtt-frwd.rules
```
KERNEL=="ttyUSB?", ENV{SYSTEMD_WANTS}="mqtt-forward@.service"
```


pi@water:~ $ sudo systemctl enable mqtt-forward@.service
приходится каждый час перезапускать сервис - он тиха падает

# отражаем события в mqtt
копируем данные из веток по устройствам в человечески названную ветку
- stat/house/
- астрономические вычисления stat/house/outdoor
 - утро, день, вечер, ночь - просто по часам
 - считать начало и конец сумерек - для выключения света
 ```
import datetime
from astral import Astral
city_name = 'New York'
a = Astral()
a.solar_depression = 'civil'
city = a[city_name]
print('Information for %s/%s\n' % (city_name, city.region))
timezone = city.timezone
print('Timezone: %s' % timezone)
print('Latitude: %.02f; Longitude: %.02f\n' % \
    (city.latitude, city.longitude))
sun = city.sun(date=datetime.date.today(), local=True)
print('Dawn:    %s' % str(sun['dawn']))
print('Sunrise: %s' % str(sun['sunrise']))
print('Noon:    %s' % str(sun['noon']))
print('Sunset:  %s' % str(sun['sunset']))
print('Dusk:    %s' % str(sun['dusk']))

 ```

# собираем данные активностит дома и определения пусто или нет
Вычисленные из исходников в дереве параметры лежать в дереве tele/house
данные в ветках храним либо чистая строка, либо json


- входная дверь
- определяем подключение к WiFi 
- включение/выключение выключателей 
- изменение нагрузок в розетках
- датчики дверей и окон
- датчики движения
