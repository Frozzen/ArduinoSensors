# -*- coding: utf-8 -*-
# !/usr/bin/python
"""
его можно запусчкать только на локальной подсети из 172.20.100 не пойшел

crontab -l
*/10 * * * *  /home/pi/domoticz/scripts/check_device_online.py 192.168.4.9 37 10 120
*/10 * * * *  /home/pi/domoticz/scripts/check_device_online.py 192.168.4.10 36 10 2700
"""
import base64
import datetime
import json
import subprocess
#   Title: check_device_online.py
#   Author: Chopper_Rob
#   Date: 25-02-2015
#   Info: Checks the presence of the given device on the network and reports back to domoticz
#   URL : https://www.chopperrob.nl/domoticz/5-report-devices-online-status-to-domoticz
#   Version : 1.6.2
import sys
import syslog
import time

import urllib2

from mymqtt import MyMQTT


# DO NOT CHANGE BEYOND THIS LINE


def domoticzstatus():
    json_object = json.loads(domoticzrequest(domoticzurl))
    status = 0
    switchfound = False
    if json_object["status"] == "OK":
        for i, v in enumerate(json_object["result"]):
            if json_object["result"][i]["idx"] == switchid:
                switchfound = True
                if json_object["result"][i]["Status"] == "On":
                    status = 1
                if json_object["result"][i]["Status"] == "Off":
                    status = 0
    if switchfound == False:
        syslog.syslog(syslog.LOG_ALERT,
                      "- Error. Could not find switch idx in Domoticz response. Defaulting to switch off.")
    return status


def domoticzrequest(url):
    request = urllib2.Request(url)
    request.add_header("Authorization", "Basic %s" % base64string)
    response = urllib2.urlopen(request)
    return response.read()


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        syslog.syslog(syslog.LOG_INFO, "Connected to broker")
        global __Connected  # Use global variable
        __Connected = True  # Signal connection
    else:
        syslog.syslog(syslog.LOG_INFO, "Connection failed %s %s" % (rc, flags))


################################################
syslog.openlog(logoption=syslog.LOG_PID, facility=syslog.LOG_DAEMON)
if len(sys.argv) != 5:
    syslog.syslog(syslog.LOG_ERR, "Not enough parameters. Needs %Host %Switchid %Interval %Cooldownperiod.")
    sys.exit(0)
__Connected = False

device = sys.argv[1]
switchid = sys.argv[2]
interval = 10
cooldownperiod = 120

if int(subprocess.check_output('ps x | grep \'' + sys.argv[0] + ' ' + device + '\' | grep -cv grep',
                               shell=True)) > 2:
    syslog.syslog(syslog.LOG_ALERT, "- script already running. exiting.")
    sys.exit(1)

# send data to mqtt
mqtt = MyMQTT()
mqtt_user = mqtt.config['check_device_online']['root'] + mqtt.config['check_device_online'][device]
domoticzserver = mqtt.config['Domotizc']['server']
domoticzusername = mqtt.config['Domotizc']['username']
domoticzpassword = mqtt.config['Domotizc']['password']
domoticzpasscode = ""  # ""Light/Switch Protection"

previousstate = -1
lastsuccess = datetime.datetime.now()
lastreported = -1
base64string = base64.encodestring('%s:%s' % (domoticzusername, domoticzpassword)).replace('\n', '')
domoticzurl = 'http://' + domoticzserver + '/json.htm?type=devices&filter=all&used=true&order=Name'

syslog.syslog(syslog.LOG_INFO, "- script started.")

lastreported = domoticzstatus()
if lastreported == 1:
    syslog.syslog(syslog.LOG_INFO, "- according to domoticz, " + device + " is online")
if lastreported == 0:
    syslog.syslog(syslog.LOG_INFO, "- according to domoticz, " + device + " is offline")

while 1:
    subprocess.call('ping -q -c1 -W 1 '+ device + ' > /dev/null', shell=True)
    # currentstate = subprocess.call('arping -c1 -w 1 ' + device + ' > /dev/null', shell=True)
    currentstate = subprocess.call('arp | grep  ' + device + ' | grep ether > /dev/null', shell=True)

    if currentstate == 0: lastsuccess = datetime.datetime.now()
    if currentstate == 0 and currentstate != previousstate and lastreported == 1:
        syslog.syslog(syslog.LOG_INFO, "- " + device + " online, no need to tell domoticz")
    if currentstate == 0 and currentstate != previousstate and lastreported != 1:
        if domoticzstatus() == 0:
            syslog.syslog(syslog.LOG_INFO, "- " + device + " online, tell domoticz it's back")
            # вставить нотификацию на MQTT
            mqtt.client.publish(mqtt_user, u"ON", retain=True);
            domoticzrequest(
                "http://" + domoticzserver + "/json.htm?type=command&param=switchlight&idx=" + switchid + "&switchcmd=On&level=0" + "&passcode=" + domoticzpasscode)
        else:
            syslog.syslog(syslog.LOG_INFO, "- " + device + " online, but domoticz already knew")
        lastreported = 1

    if currentstate == 1 and currentstate != previousstate:
        syslog.syslog(syslog.LOG_INFO, "- " + device + " offline, waiting for it to come back")

    if currentstate == 1 and (datetime.datetime.now() - lastsuccess).total_seconds() > float(
            cooldownperiod) and lastreported != 0:
        if domoticzstatus() == 1:
            syslog.syslog(syslog.LOG_INFO, "- " + device + " offline, tell domoticz it's gone")
            # вставить нотификацию на MQTT
            mqtt.client.publish(mqtt_user, u"OFF", retain=True);
            domoticzrequest(
                "http://" + domoticzserver + "/json.htm?type=command&param=switchlight&idx=" + switchid + "&switchcmd=Off&level=0" + "&passcode=" + domoticzpasscode)
        else:
            syslog.syslog(syslog.LOG_INFO, "- " + device + " offline, but domoticz already knew")
        lastreported = 0

    time.sleep(float(interval))

    previousstate = currentstate
