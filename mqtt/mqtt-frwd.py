# -*- coding: utf-8 -*-
import sys, os, time
import serial
import paho.mqtt.client as mqtt

import configparser

MAX_LINE_LENGTH = 150
__Connected = False

import threading
import logging

#################################################################
def read_line_serial(ser, my_addr=':01'):
    """
    получить данные до перевода строки.
    строку из порта проверить что она послана на мой адрес, проверить CS
    'BadData' - за 150 символов не встретился перевод строки

    :param ser:

    :return: 'Ok', 'BadCS', 'NotForMe', 'BadData'
    """

    line = ""
    ok = False
    cs = 0
    # ждем ':' - начало строки
    for i in range(MAX_LINE_LENGTH):
        if ser.read() == ':':
            line = ':'
            break
    if line != ':':
        return ('', 'BadData')

    for i in range(MAX_LINE_LENGTH):
        l = ser.read()
        if l == '\n':
            break
        if l != '\r':
            line += l
    if l != '\n':
        return (line, 'BadData')

    if line[:3] != ':01':
	    return  (line[:-3], 'NotForMe')

    str = line[:-3]
    cs = line[-2:]
    cs0 = 0
    for ch in str:
        cs0 += ord(ch)
    return (str[3:], 'Ok' if ((cs0 & 0xff)) + int(cs, 16) == 255 else 'BadCS')

###################################
def on_connect(client, userdata, flags, rc):
 
    if rc == 0: 
        print("Connected to broker") 
        global __Connected                #Use global variable
        __Connected = True                #Signal connection
    else:
        print("Connection failed", rc, flags)

__dump_msg_cnt = False

##########################################
def main_loop(TOPIC_START, client, ser):
    """
    основной цикл приложения

    TODO устойчивость по отпаданию COM и MQTT
    TODO запустить по таймеру печать bus/msg_cnt

    :param TOPIC_START:
    :param client:
    :param ser:
    :return:
    """
    global __dump_msg_cnt
    msg_cnt = bad_cs_cnt = not_for_me_cnt = bad_data_cnt = 0
    while True:
        line, result = read_line_serial(ser)
        print line, result
        if __dump_msg_cnt:
            __dump_msg_cnt = False
            client.publish(TOPIC_START + "bus/msg_cnt", msg_cnt, retain=True)

        if result == 'Ok':
            topic, val = line.split('=')
            client.publish(TOPIC_START + topic, val, retain=True)
            msg_cnt += 1
            continue
        if result == 'BadCS':
            bad_cs_cnt += 1
            client.publish(TOPIC_START + "bus/errors", bad_cs_cnt, retain=True)
        elif result == 'NotForMe':
            not_for_me_cnt += 1
            client.publish(TOPIC_START + "bus/not4me", not_for_me_cnt, retain=True)
        elif result == 'BadData':
            bad_data_cnt += 1
            client.publish(TOPIC_START + "bus/bad_data", bad_data_cnt, retain=True)

def each_5_seconds():
  threading.Timer(5.0, each_5_seconds).start()
  global __dump_msg_cnt
  __dump_msg_cnt = True


def main(argv):
    global __Connected                #Use global variable
    config = configparser.ConfigParser()
    config.read('mqtt-frwd.ini')
    
    TOPIC_START = config['MQTT']['topic_head']
    ser = serial.Serial(port=config['COM']['port'], baudrate=config['COM']['baudrate'])

    client = mqtt.Client(config['MQTT']['client']) # , clean_session=True, userdata=None, protocol=MQTTv311, transport=”tcp”)
    client.username_pw_set(username=config['MQTT']['user'], password=config['MQTT']['pass'])
    client.on_connect= on_connect
    client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))
    client.loop_start()        #start the loop
 
    while __Connected != True:    #Wait for connection
        time.sleep(0.1)

    # each_5_seconds()
    main_loop(TOPIC_START, client, ser)
    client.disconnect()
    client.loop_stop()
    ser.close()

if __name__ == '__main__':
    # https://docs.python.org/3/howto/logging-cookbook.html
    logger = logging.getLogger('com-mqtt')
    logger.setLevel(logging.DEBUG)
    # create file handler which logs even debug messages
    fh = logging.FileHandler('mqtt-frwd.log')
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    fh.setFormatter(formatter)
    logger.addHandler(fh)

    try:
        main(sys.argv)
    except Exception as e:
        logger.fatal(e, exc_info=True)