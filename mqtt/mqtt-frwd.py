# -*- coding: utf-8 -*-
import sys, os, time
import serial
import paho.mqtt.client as mqtt

import configparser

def read_line_serial(ser):
    line = ""
    ok = False
    cs = 0
    for i in range(100):
        l = ser.read()
        if l == '\n':
            break
        if l != '\r':
            line += l
    if line[:3] != ':01':
	    print line, " is not for me"
    str = line[:-3]
    cs = line[-2:]
    cs0 = 0
    for ch in str:
        cs0 += ord(ch)
    return (str[3:], ((cs0 & 0xff)) + int(cs, 16) == 255)

Connected = False
def on_connect(client, userdata, flags, rc):
 
    if rc == 0: 
        print("Connected to broker") 
        global Connected                #Use global variable
        Connected = True                #Signal connection 
    else:
        print("Connection failed", rc, flags)

def main(argv):
    global Connected                #Use global variable
    # ser = serial.Serial(
    #     port='COM5',\
    #     baudrate=9600,\
    #     parity=serial.PARITY_NONE,\
    #     stopbits=serial.STOPBITS_ONE,\
    #     bytesize=serial.EIGHTBITS,\
    #         timeout=0)
    config = configparser.ConfigParser()
    config.read('mqtt-frwd.ini')
    
    client = mqtt.Client(config['MQTT']['client']) # , clean_session=True, userdata=None, protocol=MQTTv311, transport=”tcp”)
    client.username_pw_set(username=config['MQTT']['user'], password=config['MQTT']['pass'])
    client.on_connect= on_connect
    client.connect(config['MQTT']['server'], port=int(config['MQTT']['port']))
    client.loop_start()        #start the loop
 
    while Connected != True:    #Wait for connection
        time.sleep(0.1)
    TOPIC_START = config['MQTT']['topic_head']
    ser = serial.Serial(port=config['COM']['port'], baudrate=config['COM']['baudrate'])
    errCnt = 0
    try:
        while True:
            line, result = read_line_serial(ser)
            if result != True:
		errCnt += 1
                print "-drop:%s" % line
                client.publish(TOPIC_START+"/errors",errCnt, retain=True)
                continue
            topic, val = line.split('=')
            client.publish(TOPIC_START+topic,val, retain=True)
	    print line
    except:
        raise
    client.disconnect()
    client.loop_stop()
    ser.close()
if __name__ == '__main__':
    main(sys.argv)
