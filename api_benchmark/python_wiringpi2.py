#!/usr/bin/env python

import sys
import wiringpi2

io = wiringpi2.GPIO(wiringpi2.GPIO.WPI_MODE_PINS)
#io = wiringpi2.GPIO(wiringpi2.GPIO.WPI_MODE_GPIO)

PIN = 14
io.pinMode(PIN,io.OUTPUT)

count = int(sys.argv[1])

for x in range(count):
    io.digitalWrite(PIN,io.HIGH)
    io.digitalWrite(PIN,io.LOW)

