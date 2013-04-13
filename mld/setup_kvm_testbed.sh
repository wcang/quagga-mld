#!/bin/bash
#script to setup 2 kvm virtual machine to perform quagga mld testing

start() {
#	service firewalld stop
	tunctl -u wcang -t tap_mld1
	tunctl -u wcang -t tap_mld2
	ifconfig tap_mld1 up
	ifconfig tap_mld2 up
	virsh start mld1
	virsh start mld2
}

stop() {
	virsh shutdown mld2
	virsh shutdown mld1
	tunctl -d tap_mld1
	tunctl -d tap_mld2
	#service firewalld start
}

case "$1" in
	start)
		start
		;;
	stop)
		stop
		;;
	restart)
		stop
		start
		;;
	*)
		echo "Usage $0 start|stop|restart"
		exit 1
		;;

esac
