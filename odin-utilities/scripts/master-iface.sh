#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Usage: master-iface.sh [path_scripts] [wlanX] [channel]"
    exit 1
fi

LOCAL_PATH=$1
WLAN_IFACE=$2
NUM_CHANNEL=$3
SERVICE='network-manager'

path_host_scripts="$LOCAL_PATH/spring-odin-patch/scripts"

if ps aux | grep -v grep | grep $SERVICE > /dev/null
then
    echo [+] Network manager is running
    echo [+] Turning nmcli wlan off --fix hostapd bug 1/2
    # Fix hostapd bug in Ubuntu 14.04
    nmcli nm wifi off
else
    echo [+] Network manager is stopped
fi

# Fix hostapd bug in Ubuntu 14.04
echo [+] Unblocking wlan --fix hostapd bug 2/2
rfkill unblock wlan
sleep 1

# Generate configuration for hostapd
echo [+] Creating hostapd configuration file
cd $path_host_scripts
python hostapd-cfg-generator.py "$WLAN_IFACE" "$NUM_CHANNEL" > hostapd_odin.cfg
sleep 1

# Start hostapd 
echo [+] Starting hostapd for setting $WLAN_IFACE in master mode
hostapd hostapd_odin.cfg &
sleep 3

# We only need hostapd for tunning the wireless iface and setting it in master mode, then we can kill it
echo [+] Stopping hostapd
pkill hostapd
