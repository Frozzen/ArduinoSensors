# -*- coding: utf-8 -*-
import time

import paho.mqtt.client as mqtt
import sys


def main(argv):
    client = mqtt.Client(clean_session=True, userdata=None, protocol=mqtt.MQTTv311, transport='tcp')
    client.username_pw_set(username='sysop', password='sysop')
    client.connect('localhost', port=1883)
    client.loop_stop()

    with open(argv[1], 'rt') as f:
        data = []
        for ln in f:
            data.append(ln.rstrip().split(' '))
    for i in range(1000):
        for topic, payload in data:
            client.publish('testing/' + topic, payload, retain=False)
            # print(topic, payload)
            time.sleep(0.001)

    client.disconnect()


if __name__ == '__main__':
    main(sys.argv)
