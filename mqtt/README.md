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
KERNEL=="ttyUSB0", ENV{SYSTEMD_WANTS}="mqtt-forward@.service /dev/ttyUSB0"
KERNEL=="ttyUSB1", ENV{SYSTEMD_WANTS}="mqtt-forward@.service /dev/ttyUSB1"
KERNEL=="ttyUSB2", ENV{SYSTEMD_WANTS}="mqtt-forward@.service /dev/ttyUSB2"
KERNEL=="ttyUSB3", ENV{SYSTEMD_WANTS}="mqtt-forward@.service /dev/ttyUSB3"
```


pi@water:~ $ sudo systemctl enable mqtt-forward@.service

