#!/bin/sh

insmod lana.ko
insmod sd_rr.ko
insmod fb_dummy.ko

../usr/fbctl add fb1 dummy
../usr/fbctl add fb2 dummy
../usr/fbctl add fb3 dummy
../usr/fbctl bind fb1 fb2
../usr/fbctl bind fb2 fb3

opcontrol --reset
opcontrol --shutdown
rm /root/.oprofile/daemonrc
opcontrol --init
if [ $# -eq 0 ] ; then
	opcontrol --setup --vmlinux=../../../linux-2.6/vmlinux --separate=cpu --event=CPU_CLK_UNHALTED:100000:0:1:0
else
	opcontrol --setup --vmlinux=../../../linux-2.6/vmlinux --separate=cpu --event=$@
fi
opcontrol --status
opcontrol --start

insmod testskb.ko

opcontrol --dump
opreport -l -p ./
opcontrol --shutdown

rmmod testskb

../usr/fbctl unbind fb2 fb3
../usr/fbctl unbind fb1 fb2
../usr/fbctl rm fb3
../usr/fbctl rm fb2
../usr/fbctl rm fb1

echo "-1" > /proc/net/lana/ppesched

sleep 1

rmmod fb_dummy
rmmod sd_rr
rmmod lana

echo "+++ done +++"

