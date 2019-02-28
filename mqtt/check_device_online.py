# -*- coding: utf-8 -*-
#!/usr/bin/python
"""
crontab -l
*/10 * * * *  /home/pi/domoticz/scripts/check_device_online.py 192.168.4.9 37 10 120
*/10 * * * *  /home/pi/domoticz/scripts/check_device_online.py 192.168.4.10 36 10 2700
"""
#   Title: check_device_online.py
#   Author: Chopper_Rob
#   Date: 25-02-2015
#   Info: Checks the presence of the given device on the network and reports back to domoticz
#   URL : https://www.chopperrob.nl/domoticz/5-report-devices-online-status-to-domoticz
#   Version : 1.6.2

import sys
import datetime
import time
import os
import subprocess
import urllib2
import json
import base64

# Settings for the domoticz server
import configparser
import paho.mqtt.client as mqtt

domoticzserver = "172.20.110.2:8080"
domoticzusername = "vovva"
domoticzpassword = "q1w2e3"
domoticzpasscode = "" # ""Light/Switch Protection"

# If enabled. The script will log to the file _.log
# Logging to file only happens after the check for other instances, before that it only prints to screen.
log_to_file = False

# The script supports two types to check if another instance of the script is running.
# One will use the ps command, but this does not work on all machine (Synology has problems)
# The other option is to create a pid file named _.pid. The script will update the timestamp
# every interval. If a new instance of the script spawns it will check the age of the pid file.
# If the file doesn't exist or it is older then 3 * Interval it will keep running, otherwise is stops.
# Please chose the option you want to use "ps" or "pid", if this option is kept empty it will not check and just run.
check_for_instances = "ps"

# DO NOT CHANGE BEYOND THIS LINE
if len(sys.argv) != 5:
    print ("Not enough parameters. Needs %Host %Switchid %Interval %Cooldownperiod.")
    sys.exit(0)
__Connected = False

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
        global __Connected  # Use global variable
        __Connected = True  # Signal connection
    else:
        print("Connection failed", rc, flags)

device = sys.argv[1]
switchid = sys.argv[2]
interval = sys.argv[3]
cooldownperiod = sys.argv[4]
previousstate = -1
lastsuccess = datetime.datetime.now()
lastreported = -1
base64string = base64.encodestring('%s:%s' % (domoticzusername, domoticzpassword)).replace('\n', '')
domoticzurl = 'http://' + domoticzserver + '/json.htm?type=devices&filter=all&used=true&order=Name'

# send data to mqtt
config = configparser.RawConfigParser()
config.optionxform = str
config.read('mqtt-frwd.ini')
client = mqtt.Client(clean_session=True)  # , userdata=None, protocol=MQTTv311, transport=”tcp”)
client.username_pw_set(username=config['MQTT']['user'], password=config['MQTT']['pass'])
client.on_connect = on_connect
client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))
client.loop_start()  # start the loop
mqtt_root = config['check_device_online']['root']+config['check_device_online'][device]
mqtt_root = mqtt_root.encode("ascii")
del  config
while __Connected != True:  # Wait for connection
    time.sleep(0.1)

strftime = datetime.datetime.now().strftime("%H:%M:%S")
if check_for_instances.lower() == "pid":
    pidfile = sys.argv[0] + '_' + sys.argv[1] + '.pid'
    if os.path.isfile(pidfile):
        print strftime + "- pid file exists"
        if (time.time() - os.path.getmtime(pidfile)) < (float(interval) * 3):
            print strftime + "- script seems to be still running, exiting"
            print strftime + "- If this is not correct, please delete file " + pidfile
            sys.exit(0)
        else:
            print strftime + "- Seems to be an old file, ignoring."
    else:
        open(pidfile, 'w').close()

if check_for_instances.lower() == "ps":
    if int(subprocess.check_output('ps x | grep \'' + sys.argv[0] + ' ' + sys.argv[1] + '\' | grep -cv grep',
                                   shell=True)) > 2:
        print (strftime + "- script already running. exiting.")
        sys.exit(0)


def log(message):
    print message
    if log_to_file == True:
        logfile = open(sys.argv[0] + '_' + sys.argv[1] + '.log', "a")
        logfile.write(message + "\n")
        logfile.close()


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
    if switchfound == False: print (
                strftime + "- Error. Could not find switch idx in Domoticz response. Defaulting to switch off.")
    return status


def domoticzrequest(url):
    request = urllib2.Request(url)
    request.add_header("Authorization", "Basic %s" % base64string)
    response = urllib2.urlopen(request)
    return response.read()


log(strftime + "- script started.")

lastreported = domoticzstatus()
if lastreported == 1:
    log(strftime + "- according to domoticz, " + device + " is online")
if lastreported == 0:
    log(strftime + "- according to domoticz, " + device + " is offline")

while 1 == 1:
    # currentstate = subprocess.call('ping -q -c1 -W 1 '+ device + ' > /dev/null', shell=True)
    currentstate = subprocess.call('arping -c1 -w 1 ' + device + ' > /dev/null', shell=True)

    if currentstate == 0: lastsuccess = datetime.datetime.now()
    if currentstate == 0 and currentstate != previousstate and lastreported == 1:
        log(strftime + "- " + device + " online, no need to tell domoticz")
    if currentstate == 0 and currentstate != previousstate and lastreported != 1:
        if domoticzstatus() == 0:
            log(strftime + "- " + device + " online, tell domoticz it's back")
	        # вставить нотификацию на MQTT
            client.publish(mqtt_root, "ON;"+strftime, retain=True);
            domoticzrequest(
                "http://" + domoticzserver + "/json.htm?type=command&param=switchlight&idx=" + switchid + "&switchcmd=On&level=0" + "&passcode=" + domoticzpasscode)
        else:
            log(strftime + "- " + device + " online, but domoticz already knew")
        lastreported = 1

    if currentstate == 1 and currentstate != previousstate:
        log(strftime + "- " + device + " offline, waiting for it to come back")

    if currentstate == 1 and (datetime.datetime.now() - lastsuccess).total_seconds() > float(
            cooldownperiod) and lastreported != 0:
        if domoticzstatus() == 1:
            log(strftime + "- " + device + " offline, tell domoticz it's gone")
	        # вставить нотификацию на MQTT
            client.publish(mqtt_root, "OFF;"+strftime, retain=True);
            domoticzrequest(
                "http://" + domoticzserver + "/json.htm?type=command&param=switchlight&idx=" + switchid + "&switchcmd=Off&level=0" + "&passcode=" + domoticzpasscode)
        else:
            log(strftime + "- " + device + " offline, but domoticz already knew")
        lastreported = 0

    time.sleep(float(interval))

    previousstate = currentstate
    if check_for_instances.lower() == "pid": open(pidfile, 'w').close()