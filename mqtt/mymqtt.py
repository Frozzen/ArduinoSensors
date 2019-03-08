# -*- coding: utf-8 -*-

import paho.mqtt.client as mqtt

import configparser


class MyMQTT(object):
    def __init__(self):
        self.__Connected = False
        self.config = configparser.RawConfigParser()
        self.config.optionxform = str
        self.config.read('mqtt-frwd.ini')
        self.client = mqtt.Client(clean_session=True, userdata=None, protocol=mqtt.MQTTv311, transport='tcp')
        self.client.username_pw_set(username=self.config['MQTT']['user'], password=self.config['MQTT']['pass'])

    def connect(self):
        self.client.connect(self.config['MQTT']['server'], port=int(self.config['MQTT']['port']))
        self.client.loop_start()

    def close(self):
        self.client.loop_stop()
        self.client.disconnect()
