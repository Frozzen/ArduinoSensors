# -*- coding: utf-8 -*-
import sys
import serial
import paho.mqtt.client as mqtt

import configparse

def read_line_serial(ser):
    line = ""
    ok = False
    cs = 0
    for i in range(100):
        l = ser.read()
        if l == '\n':
            break
        if l != '\r':
            line += l
    str, cs = line.split(':')
    cs0 = 0
    for ch in str:
        cs0 += ord(ch)
    return (str, ((cs0 & 0xff)) + int(cs, 16) == 255)

def main(argv):
    # ser = serial.Serial(
    #     port='COM5',\
    #     baudrate=9600,\
    #     parity=serial.PARITY_NONE,\
    #     stopbits=serial.STOPBITS_ONE,\
    #     bytesize=serial.EIGHTBITS,\
    #         timeout=0)
    config = configparser.ConfigParser()
    config.read('FILE.INI')

    ser = serial.Serial(port=config['COM']['port'], baudrate=config['COM']['baudrate'])
    client = mqtt.Client(config['MQTT']['client'])
    client.username_pw_set(username=config['MQTT']['server'], password=config['MQTT']['pass'])
    client.connect(config['MQTT']['server'])
    TOPIC_START = config['MQTT']['topic_head']
    try:
        for i in range(20):
            line, result = read_line_serial(ser)
            if result != True:
                continue
            topic, val = line.split('=')
            client.publish(TOPIC_START+topic,val)
    except:
        pass
    ser.close()
if __name__ == '__main__':
    main(sys.argv)