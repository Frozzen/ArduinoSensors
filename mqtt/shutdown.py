# -*- coding: utf-8 -*-
from gpiozero import Button 
import time, syslog 
import os 
syslog.openlog(facility=syslog.LOG_KERN)

stopButton = Button(23) # defines the button as an object and chooses GPIO 23

while True: 
     if stopButton.is_pressed: 
        time.sleep(1) 
        if stopButton.is_pressed: 
	    syslog.syslog(syslog.LOG_ALERT, "shutdown pressed")
            os.system("shutdown now -h") 
     time.sleep(1) 
