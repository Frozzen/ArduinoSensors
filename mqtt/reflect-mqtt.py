# -*- coding: utf-8 -*-
"""
отражаем данные с аппаратных веток MQTT на человеческие.
cmnd/house/
stat/house/
данные берутся из дескрипторов

"""
import fcntl
import json

import paho.mqtt.client as mqtt

import configparser, sys, time
import syslog

__Connected = False

reflect = {}

def  main_subscribe(client):
    global  reflect
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
        data = msg.payload.decode("utf-8")
        topic_ = reflect[msg.topic]
        tpc = topic_.split('/')[-1]
        if tpc == 'ENERGY':
            js = json.loads(data)
            data = js['ENERGY']['Current']
            topic_ += "/Current"
        msg_info = client.publish(topic_, data, retain=True)

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
    syslog.syslog(syslog.LOG_ERR, "reflect-mqtt  stopped **" )

_fh_lock = 0
def run_once():
    global _fh_lock
    fh = open(os.path.realpath(__file__), 'r')
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except:
        os._exit(1)


if __name__ == '__main__':

    run_once()
    syslog.openlog(logoption=syslog.LOG_PID, facility=syslog.LOG_DAEMON)
    try:
        main(sys.argv)
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR, "** exception %s" % (str(e)))
