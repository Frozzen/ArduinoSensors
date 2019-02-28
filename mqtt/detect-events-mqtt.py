# -*- coding: utf-8 -*-
"""
собираем события - по mqtt чтобы определить что дома кто то есть
- появление на связи пользователя stat/house/users/+: topic : stat/house/users/vladimir, ON;22:24:33
- собираем выключатели topic /[a-z]+_lswitch/
- изменение расхода энергии в розетках: кроме газа: stat/house/heating/ENERGY
topic = ENERGY
{"Time": "2019-02-28T22:28:55",
 "ENERGY": {"TotalStartTime": "2019-02-06T18:52:41", "Total": 4.200, "Yesterday": 0.278, "Today": 0.414, "Period": 0,
            "Power": 0, "ApparentPower": 0, "ReactivePower": 0, "Factor": 0.00, "Voltage": 0, "Current": 0.000}}

stat/house/
данные берутся из дескрипторов

"""
import paho.mqtt.client as mqtt

import configparser, sys, time
import syslog

__Connected = False

reflect = {}



def main_subscribe(client):
    global reflect
    for tpc in reflect.keys():
        decode = tpc.encode('ascii')
        client.subscribe(decode)


###################################
def __on_connect(client, userdata, flags, rc):
    if rc == 0:
        syslog.syslog(syslog.LOG_NOTICE, "reflect-mqtt Connected to broker")
        global __Connected
        __Connected = True
        main_subscribe(client)
    else:
        print("Connection failed", rc, flags)


def __on_message(client, userdata, msg):
    global reflect
    if msg.topic in reflect:
        msg_info = client.publish(reflect[msg.topic], msg.payload, retain=True)


def __on_disconnect(client, userdata, flags, rc):
    global __Connected  # Use global variable
    __Connected = False
    # client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))


def __on_subscribe(client, userdata, flags, rc):
    pass
    # client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))


def main(argv):
    global __Connected, reflect  # Use global variable
    syslog.syslog(syslog.LOG_NOTICE, "reflect-mqtt on started")
    config = configparser.RawConfigParser()
    config.optionxform = str
    config.read('mqtt-frwd.ini')
    reflect = dict(config.items('reflect'))

    client = mqtt.Client(clean_session=True)  # , userdata=None, protocol=MQTTv311, transport=”tcp”)
    client.username_pw_set(username=config['MQTT']['user'], password=config['MQTT']['pass'])
    client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))
    client.on_connect = __on_connect
    client.on_message = __on_message
    client.on_subscribe = __on_subscribe
    client.on_disconnect = __on_disconnect
    client.loop_start()  # start the loop

    while __Connected != True:  # Wait for connection
        time.sleep(0.1)
    while __Connected == True:  # Wait for connection
        time.sleep(0.1)

    client.disconnect()
    client.loop_stop()
    syslog.syslog(syslog.LOG_ERR, "detect-events-mqtt  stopped **")


if __name__ == '__main__':
    syslog.openlog(logoption=syslog.LOG_PID, facility=syslog.LOG_DAEMON)
    try:
        main(sys.argv)
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR, "** exception %s" % (str(e)))
