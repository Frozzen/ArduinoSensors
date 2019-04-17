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
        for ln in f:
            topic, payload = ln.rstrip().split(' ')
            client.publish('testing/' + topic, payload, retain=False)
            print(topic, payload)
            time.sleep(0.05)

    client.disconnect()


if __name__ == '__main__':
    main(sys.argv)
