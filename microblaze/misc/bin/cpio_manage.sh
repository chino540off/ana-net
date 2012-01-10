#!/bin/sh
usage="Usage: cpio_manage.sh <unpack|pack> <initramfs.cpio.gz> <out/in dir>";
if test $# -lt 3 ; then
	echo $usage;
	exit
fi
if [ "`id -u`" != "0" ]; then
	echo "Not root?!"
	exit
fi
arg="$1"
ramfs="../$2"
dir="$3"
case "$arg" in
p|pa|pac|pack)
	cd $dir
	find . | cpio -o | gzip -c > $ramfs
	;;
u|un|unp|unpa|unpac|unpack)
	mkdir -p $dir
	cd $dir
	gunzip -c $ramfs | cpio -id
	;;
*)
	echo $usage;
	;;
esac
