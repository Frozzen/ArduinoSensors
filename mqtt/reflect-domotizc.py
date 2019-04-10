# -*- coding: utf-8 -*-
"""
отражаем данные с аппаратных веток MQTT на человеческие.
cmnd/house/
stat/house/
данные берутся из дескрипторов

"""
import fcntl, os
import json

import paho.mqtt.client as mqtt

import configparser, sys, time
import syslog

from mymqtt import MyMQTT

class DomotizcMQTT(MyMQTT):
    def __init__(self):
        super(DomotizcMQTT, self).__init__()
        self.domotizc = dict(self.config.items('Domotizc'))
        self.connected = False

        ###################################
        def __on_connect(client, userdata, flags, rc):
            if rc == 0:
                syslog.syslog(syslog.LOG_NOTICE, "reflect-mqtt Connected to broker")
                global __Connected
                userdata.onnected = True
                userdata.main_subscribe()
            else:
                print("Connection failed", rc, flags)

        def __on_message(client, userdata, msg):
            topic = msg.topic
            if topic in userdata.domotizc:
                tval = '{ "idx" : %s, "nvalue" : 0, "svalue": "%s" }' % (userdata.domotizc[topic], msg.payload.decode("utf-8"))
                client.publish("domoticz/in", tval)

        def __on_disconnect(client, userdata, flags, rc):
            userdata.onnected = False
            # client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))

        self.client.on_connect = __on_connect
        self.client.on_message = __on_message
        self.client.on_disconnect = __on_disconnect

    def  main_subscribe(self):
        for tpc in self.domotizc.keys():
            self.client.subscribe(tpc)

def main(argv):
    mqtt = DomotizcMQTT()
    syslog.syslog(syslog.LOG_NOTICE, "reflect-domotizc on started")

    mqtt.connect()  # start the loop

    while mqtt.connected != True:  # Wait for connection
        time.sleep(0.1)
    while mqtt.connected == True:  # Wait for connection
        time.sleep(0.1)

    mqtt.close()
    syslog.syslog(syslog.LOG_ERR, "reflect-domotizc  stopped **" )

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
    main(sys.argv)
    try:
        main(sys.argv)
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR, "** exception %s" % (str(e)))
