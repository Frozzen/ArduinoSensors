# -*- coding: utf-8 -*-
"""
заношу в MQTT астрономические события и события по расписанию

"""
import astral
import datetime, time
import schedule
import os
import fcntl

from mymqtt import MyMQTT


class MySunSched(object):
    def __init__(self):
        self.jobs = {}

    def _astral(self):
        """
        print('Dawn:    %s' % str(sun['dawn']))
        print('Sunrise: %s' % str(sun['sunrise']))
        print('Noon:    %s' % str(sun['noon']))
        print('Sunset:  %s' % str(sun['sunset']))
        print('Dusk:    %s' % str(sun['dusk']))

        :return: sun
        """
        a = astral.Astral()
        a.solar_depression = 'nautical'
        city = astral.Location(('Home', None, 54, 83, 'Asia/Novosibirsk', 0))
        # print('Information for %s/%s\n' % ('Novosibirsk', city.region))
        # print('Timezone: %s' % city.timezone)
        # print('Latitude: %.02f; Longitude: %.02f\n' % (city.latitude, city.longitude))
        sun = city.sun(date=datetime.date.today() + datetime.timedelta(days=1), local=True)
        return sun

    def sun_event(self, ev_type):
        if ev_type in self.jobs:
            schedule.cancel_job(self.jobs[ev_type])
        sun = self._astral()
        s = sun[ev_type].strftime("%H:%M:%S")
        self.jobs[ev_type] = schedule.every().day.at(s).do(self.sun_event, ev_type)
        #  send MQTT event
        mqtt = MyMQTT()
        mqtt.connect()
        topic = mqtt.config.get('astral', 'sun_topic')
        mqtt.client.publish(topic, ev_type, retain=True)
        mqtt.close()

    def day_event(self, ev_type):
        #  send MQTT event
        mqtt = MyMQTT()
        mqtt.connect()
        topic = mqtt.config.get('astral', 'day_topic')
        mqtt.client.publish(topic, ev_type, retain=True)
        mqtt.close()

    def start(self):
        self.sun_event('dawn')
        self.sun_event('sunrise')
        self.sun_event('sunset')
        self.sun_event('dusk')

        schedule.every().day.at('07:00').do(self.day_event, 'утро')
        schedule.every().day.at('10:00').do(self.day_event, 'день')
        schedule.every().day.at('19:00').do(self.day_event, 'вечер')
        schedule.every().day.at('23:00').do(self.day_event, 'ночь')


def main():
    MySunSched().start()
    while 1:
        schedule.run_pending()
        time.sleep(1)


_fh_lock = 0
if __name__ == '__main__':
    def run_once():
        global _fh_lock
        fh = open(os.path.realpath(__file__), 'r')
        try:
            fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except:
            os._exit(1)

    run_once()
    main()
