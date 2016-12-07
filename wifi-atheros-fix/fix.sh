#! /bin/bash
cp -r /lib/firmware/ath10k/QCA6174/hw3.0/ backup/$(date '+%Y%m%d_%H.%m.%S.%N')hw3.0/
rm /lib/firmware/ath10k/QCA6174/hw3.0/* 2> /dev/null
cp board-2.bin  /lib/firmware/ath10k/QCA6174/hw3.0/board.bin
cp firmware-4.bin_WLAN.RM.2.0-00180-QCARMSWPZ-1 /lib/firmware/ath10k/QCA6174/hw3.0/firmware-4.bin
chmod +x /lib/firmware/ath10k/QCA6174/hw3.0/*

echo 'New Ahteros hw3.0 files: '
ls -la /lib/firmware/ath10k/QCA6174/hw3.0/
echo 'RESTART TO APPLY CHANGES!'