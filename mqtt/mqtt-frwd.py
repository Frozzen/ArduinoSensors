# -*- coding: utf-8 -*-
"""
отправляем данные с com на mqtt
    mqtt-frwd.py /dev/ttyUSB0
"""
import sys, os, time
import serial
import paho.mqtt.client as mqtt

import configparser

MAX_LINE_LENGTH = 150
__Connected = False

import syslog

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
        return (line[:-3], 'NotForMe')

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
        global __Connected  # Use global variable
        __Connected = True  # Signal connection
    else:
        print("Connection failed", rc, flags)


__dump_msg_cnt = False


##########################################
def main_loop(TOPIC_START, client, ser, domotizc):
    """
    основной цикл приложения

    устойчивость по отпаданию COM и MQTT
    запустить по таймеру печать bus/msg_cnt

    :param domotizc:
    :param TOPIC_START:
    :param client:
    :param ser:
    :return:
    """
    global __dump_msg_cnt
    msg_cnt = bad_cs_cnt = not_for_me_cnt = bad_data_cnt = 0
    while True:
        line, result = read_line_serial(ser)
        # print line,result
        if __dump_msg_cnt:
            __dump_msg_cnt = False
            client.publish(TOPIC_START + "bus/msg_cnt", str(msg_cnt), retain=True)

        if result == 'Ok':
            topic, val = line.split('=')
            client.publish(TOPIC_START + topic, val, retain=True)
            topic = topic.decode('utf-8').lower()
            if topic in domotizc:
                tval = '{ "idx" : %s, "value" : "%s", "svalue": "%s" }' % (domotizc[topic], val, val)
                client.publish("domoticz/in", tval)
                __dump_msg_cnt = True
            msg_cnt += 1
            continue
        __dump_msg_cnt = True
        if result == 'BadCS':
            bad_cs_cnt += 1
            client.publish(TOPIC_START + "bus/errors", str(bad_cs_cnt), retain=True)
        elif result == 'NotForMe':
            not_for_me_cnt += 1
            client.publish(TOPIC_START + "bus/not4me", str(not_for_me_cnt), retain=True)
        elif result == 'BadData':
            bad_data_cnt += 1
            client.publish(TOPIC_START + "bus/bad_data", str(bad_data_cnt), retain=True)


def main(argv):
    global __Connected  # Use global variable
    syslog.syslog(syslog.LOG_NOTICE, "mqtt-frwd on %s started" % (argv[1]))
    config = configparser.ConfigParser()
    config.read('mqtt-frwd.ini')
    TOPIC_START = config['MQTT']['topic_head']
    domotizc = dict(config.items('Domotizc'))
    port = argv[1].split('/')[-1]
    ser = serial.Serial(port='/dev/'+port, baudrate=config['COM']['baudrate'])
    client = mqtt.Client(clean_session=True)  # , userdata=None, protocol=MQTTv311, transport=”tcp”)
    client.username_pw_set(username=config['MQTT']['user'], password=config['MQTT']['pass'])
    client.on_connect = on_connect
    client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))
    client.loop_start()  # start the loop

    while __Connected != True:  # Wait for connection
        time.sleep(0.1)

    main_loop(TOPIC_START, client, ser, domotizc)

    client.disconnect()
    client.loop_stop()
    syslog.syslog(syslog.LOG_ERR, "mqtt-frwd on %s stopped **" % (argv[1]))
    ser.close()


if __name__ == '__main__':
    syslog.openlog(logoption=syslog.LOG_PID, facility=syslog.LOG_DAEMON)
    try:
	# udev send sys/devices/platform/soc/20980000.usb/usb1/1-1/1-1.2/1-1.2:1.0/ttyUSB0/tty/ttyUSB0
        main(sys.argv)
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR, "** exception %s" % (str(e)))
