#!/bin/sh

if [ -f /usr/bin/lsb_release ]; then
	LSB_RED_ID=$(/usr/bin/lsb_release -i -s)
else
	LSB_RED_ID="Unknown"
fi

case $LSB_RED_ID in
"Ubuntu")
	mkdir -p /lib/udev/ /etc/udev/rules.d/
	cp udev/ubuntu/compat_firmware.sh /lib/udev/
	cp udev/50-compat_firmware.rules /etc/udev/rules.d/
        ;;
*)
	mkdir -p /lib/udev/ /lib/udev/rules.d/
	cp udev/compat_firmware.sh /lib/udev/
	cp udev/50-compat_firmware.rules /lib/udev/rules.d/
        ;;
esac

