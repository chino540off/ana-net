#!/bin/sh

insmod lana.ko
insmod sd_single.ko
insmod fb_dummy.ko
../usr/fbctl add fb1 dummy

opcontrol --shutdown
opcontrol --init
if [ $# -eq 0 ] ; then
	opcontrol --setup --vmlinux=../../linux-2.6/vmlinux --separate=cpu
else
	opcontrol --setup --vmlinux=../../linux-2.6/vmlinux --separate=cpu --event=$@
fi
opcontrol --status
opcontrol --start

insmod testskb.ko

opcontrol --dump
opreport -l -p ./
opcontrol --shutdown

rmmod testskb
../usr/fbctl rm fb1
echo "-1" > /proc/net/lana/ppesched

sleep 1

rmmod fb_dummy
rmmod sd_single
rmmod lana

echo "+++ done +++"

