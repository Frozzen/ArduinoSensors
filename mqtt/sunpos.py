# -*- coding: utf-8 -*-
import datetime
import astral
import ephem
import datetime


def _astral():
    city_name = 'New York'
    a = astral.Astral()
    a.solar_depression = 'nautical'
    city = astral.Location(('Home', None, 54, 83, 'Asia/Novosibirsk', 0))
    # city = a[city_name]
    print('Information for %s/%s\n' % (city_name, city.region))
    timezone = city.timezone
    print('Timezone: %s' % timezone)
    print('Latitude: %.02f; Longitude: %.02f\n' % \
        (city.latitude, city.longitude))
    sun = city.sun(date=datetime.date.today(), local=True)
    print('Dawn:    %s' % str(sun['dawn']))
    print('Sunrise: %s' % str(sun['sunrise']))
    print('Noon:    %s' % str(sun['noon']))
    print('Sunset:  %s' % str(sun['sunset']))
    print('Dusk:    %s' % str(sun['dusk']))

def _ephem():

    somewhere = ephem.Observer()
    somewhere.lat = '54'  # <== change me
    somewhere.lon = '83'  # <== and change me
    somewhere.elevation = 120
    print somewhere.date

    sun = ephem.Sun()
    r1 = somewhere.next_rising(sun)
    s1 = somewhere.next_setting(sun)

    somewhere.horizon = '-0:34'
    r2 = somewhere.next_rising(sun)
    s2 = somewhere.next_setting(sun)
    print ("Visual sunrise %s" % r1)
    print ("Visual sunset %s" % s1)
    print ("Naval obs sunrise %s" % r2)
    print ("Naval obs sunset %s" % s2)

_astral()