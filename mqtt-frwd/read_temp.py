# -*- coding: utf-8 -*-
import sys, time

import configparser
import paho.mqtt.client as mqtt
import datetime

Connected = False


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
        global Connected  # Use global variable
        Connected = True  # Signal connection
    else:
        print("Connection failed", rc, flags)

temp_msg = {}
def on_mqtt_message(client, userdata, message):
    global temp_msg
    temp_msg[message.topic] = str(message.payload.decode("utf-8"))

def main(argv):
    global Connected
    global temp_msg

    config = configparser.ConfigParser()
    config.read('mqtt-frwd.ini')

    client = mqtt.Client(
        config['MQTT']['client'])  # , clean_session=True, userdata=None, protocol=MQTTv311, transport=”tcp”)
    client.username_pw_set(username=config['MQTT']['user'], password=config['MQTT']['pass'])
    client.on_connect = on_connect
    client.connect(config['MQTT']['server'], int(config['MQTT']['port']))
    client.loop_start()  # start the loop

    while Connected != True:  # Wait for connection
        time.sleep(0.1)
    client.on_message = on_mqtt_message
    client.subscribe("stat/house/bedroom/temperature")
    client.subscribe("stat/outdoor/temperature")
    while True:
        time.sleep(15*60)
        print("%s %s" % (datetime.datetime.now(), str(temp_msg)))


if __name__ == '__main__':
    main(sys.argv)