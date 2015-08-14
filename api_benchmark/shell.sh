#!/bin/bash

COUNT=$1

echo "11" > /sys/class/gpio/export > /dev/null
echo "out" > /sys/class/gpio/gpio11/direction

for (( i=1; i<=$COUNT; i++ ))
do
    echo 1 > /sys/class/gpio/gpio11/value
    echo 0 > /sys/class/gpio/gpio11/value
done
