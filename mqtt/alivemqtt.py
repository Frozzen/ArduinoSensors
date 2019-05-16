# -*- coding: utf-8 -*-
"""
определяем состояние нашей mqtt управляющей сети - устройства
собрать все устройства из БД - json. показать их состояние на картинке - плазма - красная не работает, зеленая ок

https://habr.com/ru/sandbox/28540/

состояние получать из MQTT
только PING идет отдельно
держать все в памяти - отдавать данные по rest - json

•  список устройств. поля. делать суммарную оценку состояния устройства - число от .0-1. формировать устаревание по времени разное значение.
   • IP
   • ping скорость - % достаточной доступности
   • LWT
   • время жизни - uptime
      • адресно просмотреть весь статус
      • датчики последнее значение и когда
https://habr.com/ru/post/346306/ flask

"""
# -*- coding: utf-8 -*-
import re

"""
отражаем данные с аппаратных веток MQTT на человеческие.
cmnd/house/
stat/house/
данные берутся из дескрипторов

"""
import fcntl, os
import json

import paho.mqtt.client as mqtt
from BaseHTTPServer import BaseHTTPRequestHandler,HTTPServer

import configparser, sys, time
import syslog

from mymqtt import MyMQTT
def merge_two_dicts(x, y):
    z = x.copy()   # start with x's keys and values
    z.update(y)    # modifies z with y's keys and values & returns None
    return z

alive_obj = None

class HttpProcessor(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('content-type','text/json')
        self.end_headers()
        self.wfile.write(json.encoder(alive_obj.alive_data))

class AliveMQTT(MyMQTT):
    def __init__(self):
        super(AliveMQTT, self).__init__()
        with open('alivedb.json') as fp:
            self.alivedb = json.load(fp.read());
        with open('alive_data.json') as fp:
            self.alive_data = json.load(fp.read());
        self.connected = False

        ###################################
        def __on_connect(client, userdata, flags, rc):
            if rc == 0:
                syslog.syslog(syslog.LOG_NOTICE, "alivemqtt Connected to broker")
                global __Connected
                userdata.onnected = True
                userdata.main_subscribe()
            else:
                print("Connection failed", rc, flags)

        def __on_message(client, userdata, msg):
            lwt = re.search(r"tele/([A-z0-9\-_]+)/(LWT|STATE|SENSOR|UPTIME)", msg.topic)
            if lwt is not None:
                dev = lwt.group(1)
                if dev not in self.alivedb['mqttdevices']:
                    return
                if dev not in  self.alive_data:
                    self.alive_data[dev] = {}
                key = lwt.group(2)
                payload = msg.payload
                if key == 'LWT':
                    payload = '{"%s"}' % (payload,)
                # merge dictionary
                self.alive_data[dev][key] = merge_two_dicts(self.alive_data[dev][key],
                                                            json.load(payload))

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
    global alive_obj
    alive_obj = AliveMQTT()
    syslog.syslog(syslog.LOG_NOTICE, "alivemqtt on started")

    alive_obj.connect()  # start the loop

    # while mqtt.connected != True:  # Wait for connection
    #     time.sleep(0.1)
    serv = HTTPServer(("localhost", 8087), HttpProcessor)
    serv.serve_forever()
    # while mqtt.connected == True:  # Wait for connection
    #     time.sleep(0.1)

    alive_obj.close()
    syslog.syslog(syslog.LOG_ERR, "alivemqtt  stopped **" )

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
        syslog.syslog(syslog.LOG_ERR, "**alivemqtt exception %s" % (str(e)))
