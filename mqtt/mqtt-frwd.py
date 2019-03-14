# -*- coding: utf-8 -*-
"""
отправляем данные с com на mqtt. данные просто шлются с устройства.
    udev send sys/devices/platform/soc/20980000.usb/usb1/1-1/1-1.2/1-1.2:1.0/ttyUSB0/tty/ttyUSB0

    mqtt-frwd.py [--debug|--jdy40] /dev/ttyUSB0

"""
import argparse
import fcntl
import sys
import syslog
import time

import serial

from mymqtt import MyMQTT

MAX_LINE_LENGTH = 150
__Connected = False

DEBUG = False
ADDR_FROM = 0x1
SEND_CMD = 0x1

#################################################################
def read_line_serial(ser, my_addr=':01'):
    """
    получить данные до перевода строки.
    строку из порта проверить что она послана на мой адрес, проверить CS
    'BadData' - за 150 символов не встретился перевод строки

    :param my_addr:
    :param ser:

    :return: 'Ok', 'BadCS', 'NotForMe', 'BadData'
    """

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
            line += ch.decode('ascii')
    if ch != b'\n':
        return (line, 'BadData')
    # line = ser.readline()
    if line[:3] != my_addr:
        return (line[:-3], 'NotForMe')

    s = line[:-3]
    cs = line[-2:]
    cs0 = 0
    for ch in s:
        cs0 += ord(ch)
    try:
        res = ((cs0 & 0xff)) + int(cs, 16)
    except ValueError:
        return (s, 'BadCS')
    return (s[3:], 'Ok' if res == 255 else 'BadCS')


###################################
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
        global __Connected  # Use global variable
        __Connected = True  # Signal connection
    else:
        print("Connection failed", rc, flags)


class FrwdMQTT(MyMQTT):
    def __init__(self, jdy40):
        super(FrwdMQTT, self).__init__()
        self.dump_msg_cnt = False
        self.msg_cnt = self.bad_cs_cnt = self.not_for_me_cnt = self.bad_data_cnt = 0
        self.TOPIC_START = ''
        self.domitizc = {}
        self.jdy_dev_list = []
        self.TOPIC_START = self.config['MQTT']['topic_head']
        self.domotizc = dict(self.config.items('Domotizc'))
        if jdy40:
            self.jdy_dev_list = self.config['jdy-40']['dev_list'].split(',')

    ###################################
    def read_data(self, ser):
        """
        один оборот опроса данных

        :param self:
        :param ser:
        :return:
        """
        global DEBUG

        try:
            line, result = read_line_serial(ser)
        except (serial.SerialException, ValueError) as e:
            if DEBUG:
                print("mqtt-frwd error %s **" % (str(e),))
            if 'disconnected' in e.args[0]:
                syslog.syslog(syslog.LOG_NOTICE, "mqtt-frwd error %s **" % (str(e),))
                return
            return
        # print line,result
        if self.dump_msg_cnt:
            self.dump_msg_cnt = False
            self.client.publish('tele/' + self.TOPIC_START + "bus/msg_cnt", str(self.msg_cnt).decode('utf-8'),
                                retain=True)

        if result == 'Ok':
            topic, val = line.split('=')
            if DEBUG:
                print("t:%s = %s" % (topic, val))
            if "log" == topic:
                self.client.publish('log/' + self.TOPIC_START, val.decode('utf-8'))
                return

            self.client.publish('stat/' + self.TOPIC_START + topic, val.decode('utf-8'), retain=True)
            topic = topic.lower()
            if topic in self.domotizc:
                tval = '{ "idx" : %s, "nvalue" : 0, "svalue": "%s" }' % (self.domotizc[topic], val.strip())
                self.client.publish("domoticz/in", tval.decode('utf-8'))
                self.dump_msg_cnt = True
            self.msg_cnt += 1
            return
        self.dump_msg_cnt = True
        syslog.syslog(syslog.LOG_NOTICE, "*bad: %s:%s**" % (result, line))
        if DEBUG:
            print("*bad:%s = %s" % (result, line))
        if result == 'BadCS':
            self.bad_cs_cnt += 1
            self.client.publish('tele/' + self.TOPIC_START + "bus/errors", str(self.bad_cs_cnt).decode('utf-8'),
                                retain=True)
        elif result == 'NotForMe':
            self.not_for_me_cnt += 1
            self.client.publish('tele/' + self.TOPIC_START + "bus/not4me", str(self.not_for_me_cnt).decode('utf-8'),
                                retain=True)
        elif result == 'BadData':
            self.bad_data_cnt += 1
            self.mqtt.client.publish('tele/' + self.TOPIC_START + "bus/bad_data",
                                     str(self.bad_data_cnt).decode('utf-8'), retain=True)

    ##########################################
    def main_loop(self, ser):
        """
        основной цикл приложения

        устойчивость по отпаданию COM и MQTT
        запустить по таймеру печать bus/msg_cnt

        :param ser:
        :return:
        """
        while True:
            if len(self.jdy_dev_list) > 0:
                for DEVICE_NO in self.jdy_dev_list:
                    cmd = ":%02x%02x%02x;\r\n" % (DEVICE_NO, ADDR_FROM, SEND_CMD)
                    ser.write(cmd)
                    self.read_data(ser)
            else:
                self.read_data(ser)


def main(argv):
    global __Connected, DEBUG  # Use global variable

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-v", "--verbose", action="store_true",
                    help="increase output verbosity")
    parser.add_argument("-j", "--jdy40", action="store_true",
                    help="active polling by jdy40 dongle")
    parser.add_argument('serial')
    opts = parser.parse_args()
    DEBUG = opts.verbose
    port = '/dev/' + opts.serial.split('/')[-1]

    fh = open(port, 'r')
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        sys.exit(1)

    mqtt = FrwdMQTT(opts.jdy40)
    syslog.syslog(syslog.LOG_NOTICE, "mqtt-frwd on %s started" % (argv[1]))
    ser = serial.Serial(port=port, baudrate=mqtt.config['COM']['baudrate'], timeout=0.1)
    mqtt.connect()
    mqtt.main_loop(ser)

    syslog.syslog(syslog.LOG_NOTICE, "mqtt-frwd on %s stopped **" % (argv[1]))
    if DEBUG:
        print("mqtt-frwd on %s stopped **" % (argv[1]))
    ser.close()


if __name__ == '__main__':
    syslog.openlog(logoption=syslog.LOG_PID, facility=syslog.LOG_DAEMON)
    main(sys.argv)
