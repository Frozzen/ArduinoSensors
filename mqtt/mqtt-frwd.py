# -*- coding: utf-8 -*-
"""
отправляем данные с com на mqtt. данные просто шлются с устройства.
    udev send sys/devices/platform/soc/20980000.usb/usb1/1-1/1-1.2/1-1.2:1.0/ttyUSB0/tty/ttyUSB0

    mqtt-frwd.py /dev/ttyUSB0

"""
import sys
import time

import serial

from mymqtt import MyMQTT

MAX_LINE_LENGTH = 150
__Connected = False

import syslog

DEBUG = False
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
    while True:
        ch = ser.read()
        if ch == b':':
            break
        time.sleep(0.001)
    line = ':'

    for i in range(MAX_LINE_LENGTH):
        ch = ser.read()
        if ch == b'\n':
            break
        if ch != b'\r':
            line += "".join(map(chr, ch))
    if ch != b'\n':
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
        try:
            line, result = read_line_serial(ser)
        except serial.SerialException as e:
            if 'disconnected' in e.args[0]:
                syslog.syslog(syslog.LOG_ERR, "mqtt-frwd error %s **" % (str(e),))
                return
            continue
        # print line,result
        if __dump_msg_cnt:
            __dump_msg_cnt = False
            client.publish('tele/' + TOPIC_START + "bus/msg_cnt", str(msg_cnt).encode('utf-8'), retain=True)

        if result == 'Ok':
            topic, val = line.split('=')
            if DEBUG:
                print("t:%s = %s" % (topic, val))
            if "log" == topic:
                client.publish('log/' + TOPIC_START, val)
                continue

            client.publish('stat/' + TOPIC_START + topic, val.encode('utf-8'), retain=True)
            topic = topic.lower()
            if topic in domotizc:
                tval = '{ "idx" : %s, "nvalue" : 0, "svalue": "%s" }' % (domotizc[topic], val.strip())
                client.publish("domoticz/in", tval.encode('utf-8'))
                __dump_msg_cnt = True
            msg_cnt += 1
            continue
        __dump_msg_cnt = True
        if result == 'BadCS':
            bad_cs_cnt += 1
            client.publish('tele/' + TOPIC_START + "bus/errors", str(bad_cs_cnt).encode('utf-8'), retain=True)
        elif result == 'NotForMe':
            not_for_me_cnt += 1
            client.publish('tele/' + TOPIC_START + "bus/not4me", str(not_for_me_cnt).encode('utf-8'), retain=True)
        elif result == 'BadData':
            bad_data_cnt += 1
            client.publish('tele/' + TOPIC_START + "bus/bad_data", str(bad_data_cnt).encode('utf-8'), retain=True)


def main(argv):
    global __Connected  # Use global variable
    syslog.syslog(syslog.LOG_NOTICE, "mqtt-frwd on %s started" % (argv[1]))
    mqtt = MyMQTT()
    TOPIC_START = mqtt.config['MQTT']['topic_head']
    domotizc = dict(mqtt.config.items('Domotizc'))
    port = argv[1].split('/')[-1]
    ser = serial.Serial(port='/dev/' + port, baudrate=mqtt.config['COM']['baudrate'])
    mqtt.connect()
    main_loop(TOPIC_START, mqtt.client, ser, domotizc)

    syslog.syslog(syslog.LOG_ERR, "mqtt-frwd on %s stopped **" % (argv[1]))
    ser.close()

if __name__ == '__main__':
    syslog.openlog(logoption=syslog.LOG_PID, facility=syslog.LOG_DAEMON)
    main(sys.argv)
