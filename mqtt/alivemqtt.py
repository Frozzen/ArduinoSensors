# -*- coding: utf-8 -*-
"""
определяем состояние нашей mqtt управляющей сети - устройства
собрать все устройства из БД - json. показать их состояние на картинке - плазма - красная не работает, зеленая ок
https://pypi.org/project/simple-monitor-alert/ https://github.com/Nekmo/simple-monitor-alert
https://github.com/netdata/netdata
https://jamesoff.github.io/simplemonitor/ https://github.com/jamesoff/simplemonitor

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

TODO давть всем команду cmnd/sonoff-F99B3A/STATUS 0 - получить конфигурацию
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

from http.server import BaseHTTPRequestHandler,HTTPServer

import configparser, sys, time
import syslog

from mymqtt import MyMQTT
def merge_two_dicts(x, y):
    z = x.copy()   # start with x's keys and values
    z.update(y)    # modifies z with y's keys and values & returns None
    return z

alive_obj = None

def _finditem(obj, key):
    if key in obj:
        return obj[key]
    for k, v in obj.items():
        if isinstance(v,dict):
            v = _finditem(v, key)
            if v is not None:
                return v
    return None

class HttpProcessor(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path=="/":
            str = json.dumps(alive_obj.alive_data)
        elif self.path == "/devlist":
            str = json.dumps(alive_obj.alivedb['mqttdevices'])
        elif self.path == "/all_status":
            for dev, type in alive_obj.alivedb['mqttdevices']:
                if type in ['sonoff', 'POW', 'sonoffT', 'sonoffT2', 'sonoffTH']:
                    alive_obj.client.publish('cmnd/%s/STATUS' % (dev,), '0')
            str = 'ok'
        elif  "/dev/" in self.path:
            dev = self.path.split('/')[2]
            if dev in alive_obj.alive_data:
                str = json.dumps(alive_obj.alive_data[dev])
            else:
                str = "no dev %s" % (dev, )
        elif  "/status/" in self.path:
            dev = self.path.split('/')[2]
            alive_obj.client.publish('cmnd/%s/STATUS' % (dev,), '0')
            str = 'ok'
        elif "/devkey/" in self.path:
            _, _, dev, key = self.path.split('/')
            if dev in alive_obj.alive_data:
                str = json.dumps(_finditem(alive_obj.alive_data[dev], key))
            else:
                str = "no dev %s" % (dev,)
        else:
            return
        self.send_response(200)
        self.send_header('content-type','text/json')
        self.end_headers()
        self.wfile.write(str.encode())

class AliveMQTT(MyMQTT):
    def search_device(self, dev):
        for it in self.alivedb['mqttdevices']:
            if it[0] == dev:
                return it[1]
        return None

    def __init__(self):
        super(AliveMQTT, self).__init__()
        with open('alivedb.json', 'r') as fp:
            self.alivedb = json.load(fp)
        self.alive_data = {}
        self.connected = False


        ###################################
        def __on_connect(client, userdata, flags, rc):
            if rc == 0:
                syslog.syslog(syslog.LOG_NOTICE, "alivemqtt Connected to broker")
                global __Connected
                userdata.onnected = True
                userdata.main_subscribe()
                print("Connected")
            else:
                print("Connection failed", rc, flags)

        def __on_message(client, userdata, msg):
            lwt = re.search(r"^tele/([A-z0-9\-_]+)/(LWT|STATE|SENSOR|UPTIME)$", msg.topic)
            if lwt is not None:
                dev = lwt.group(1)
                if self.search_device(dev) is None:
                    return
                if dev not in  self.alive_data:
                    self.alive_data[dev] = {}
                key = lwt.group(2)
                if key not in self.alive_data[dev]:
                    self.alive_data[dev][key] = {}
                payload = msg.payload.decode("utf-8", 'ignore')
                if key == 'LWT':
                    self.alive_data[dev][key] = payload
                else:
                    try:
                        self.alive_data[dev][key] = merge_two_dicts(self.alive_data[dev][key],
                                                                json.loads(payload))
                    except (TypeError, ValueError, json.JSONDecodeError) as e:
                        print("json err[%s][%s]=%s:%s" % (dev, key, payload, e))
                print("data[%s][%s]=%s" % (dev, key, json.dumps(self.alive_data[dev][key])))
            else:
                stat = re.search(r"^stat/([A-z0-9\-_]+)/(STATUS[0-9]+)$", msg.topic)
                if stat is None:
                    return
                dev = stat.group(1)
                if self.search_device(dev) is None:
                    return
                if dev not in  self.alive_data:
                    self.alive_data[dev] = {}
                key = stat.group(2)
                if key not in self.alive_data[dev]:
                    self.alive_data[dev][key] = {}
                payload = msg.payload.decode("utf-8", 'ignore')
                try:
                    self.alive_data[dev][key] = merge_two_dicts(self.alive_data[dev][key],
                                                            json.loads(payload))
                except (TypeError, ValueError, json.JSONDecodeError) as e:
                    print("json err[%s][%s]=%s:%s" % (dev, key, payload, e))
                print("data[%s][%s]=%s" % (dev, key, json.dumps(self.alive_data[dev][key])))

        def __on_disconnect(client, userdata, flags, rc):
            userdata.onnected = False
            # client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))

        self.client.on_connect = __on_connect
        self.client.on_message = __on_message
        self.client.on_disconnect = __on_disconnect

    def  main_subscribe(self):
        for tpc in ['tele/#', 'stat/#']:
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
    # try:
    #     main(sys.argv)
    # except Exception as e:
    #     syslog.syslog(syslog.LOG_ERR, "**alivemqtt exception %s" % (str(e)))
