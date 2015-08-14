#!/usr/bin/env python

import sys
import RPi.GPIO as GPIO

# wiringPi GPIO 11 == RPi.GPIO 23
PIN = 23

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BOARD)
GPIO.setup(PIN, GPIO.OUT)

count = int(sys.argv[1])

for x in range(count):
    GPIO.output(PIN, False)
    GPIO.output(PIN, True)


