# ATH9K HMAC Hybrid TDMA/CSMA MAC 
```
+-----------+------------------------------------+
|           |                                    | Python HMAC Wrapper enables
|  User     |   HMAC Python Wrapper              | easy integration in python scripts
|  Space    |                                    | starts / stops / updates HMAC
|           |                                    | user-space daemon and schedule
|           +---------------^--------------------+ via ZMQ IPC
|                           |                    |
|                           | ZeroMQ             |
|                           |                    |
|                           |                    |  
|           +---------------v--------------------+  
|           |                                    | HMAC User Space Daemon  
|           |   HMAC User-Space Daemon           | schedules Software Queues  
|           |                                    | by sending Netlink Commands  
|           |                                    | to wake/sleep specific TIDs  
+-----------+---------------+--------------------+ of a Link identied through  
                            |                      MAC address  
                            |  Netlink  
                            |  
+-----------+---------------v--------------------+
|           |               |                    |
|  Kernel   |     cfg80211  |                    |
|  Space    |               |                    |
|           |               |                    |
|           +---------------v--------------------+
|           |               |                    |
|           |               |                    |
|           |     mac80211  |                    |
|           |               |                    |
|           +---------------v--------------------+
|           |               | MAC ao:f1:...      | ATH9k Traffic Identifier
|           |               +--+--+--+--+        | Software Queues
|           |     ath9k     |  |  |  |  |...     | HMAC pauses/unpauses Queues
|           |               |0 |1 |2 |3 |        | 7 Queues (TIDs per Link)
+-----------+---------------------------+--------+
```
For more details please refer to our Paper:
<https://arxiv.org/abs/1611.05376>

## HOW TO INSTALL ATH9K HMAC on Ubuntu Linux in 3 steps: 
### Download HMAC sources:
```
cd ~; mkdir hmac; cd hmac; git clone https://github.com/szehl/ath9k-hmac.git; 
```
### Install 3.12 kernel:
```
chmod +x ath9k-hmac/install_3.12_kernel.sh; ath9k-hmac/install_3.12_kernel.sh
```
Now reboot machine and choose 3.12 kernel during boot in the grub menu.

### Install HMAC driver

```
cd ~/hmac/ath9k-hmac/backports-3.12.8-1; make defconfig-ath9k; make -j4; sudo make install
```
Now again reboot your machine and **choose 3.12 kernel** during boot in the grub menu.
After Reboot the ATH9k-HMAC should be installed, you can check by typing
```
dmesg | grep HMAC
```
If the output is something like:
```
[    3.711813] ath: ATH9K HMAC ENABLED
```
Everything went well.

## HOW TO USE ATH9k HMAC
First you have to compile the hmac_userspace_daemon.
If you installed the ATH9k HMAC driver with the 3 step manual, you can simply use:
```
cd ~/hmac/ath9k-hmac/hmac_userspace_daemon; make;
```
Otherwise make sure that the file hmac_userspace_daemon/hybrid_tdma_csma_mac.c includes the correct nl80211.h you used during building the ATH9k HMAC driver.


You can now use the HMAC Python Wrapper to start the ATH9k HMAC.

To start a first example of the ATH9K HMAC MAC, please make sure, that the ATH9K WiFi interface you want to use is up (e.g. 
sudo ifconfig wlan0 up), rfkill does not block WiFi (e.g. sudo rfkill unblock all). Then open the file hmac_python_wrapper/hmac_example.py and configure your interface and targte Link address e.g.:

```
    # configuration of hybrid MAC example
    dstHWAddr = "34:13:e8:24:77:be" # STA destination MAC address
    total_slots = 10
    slot_duration = 20000
    iface = 'wlan0'
```
Afterwards you can start the example by typing:
```
cd ~/hmac/ath9k-hmac/; python hmac_python_wrapper/hmac_example.py
```
The Usual Output of the Wrapper should be like this:
```
root@earth:~/hmac/ath9k-hmac$ python hmac_python_wrapper/hmac_example.py
INFO - HMAC is running ...
INFO - [
INFO - 0: 
INFO - 1: 34:13:e8:24:77:be/1,
INFO - 2: 34:13:e8:24:77:be/1,
INFO - 3: 34:13:e8:24:77:be/1,
INFO - 4: 34:13:e8:24:77:be/1,
INFO - 5: 
INFO - 6: 
INFO - 7: 
INFO - 8: 
INFO - 9: 
INFO - ]

Debug = 0
Interface = wlan0
Slot Duration = 20000
Total number of slots in frame = 10
Config = 1,34:13:e8:24:77:be,1#2,34:13:e8:24:77:be,1#3,34:13:e8:24:77:be,1#4,34:13:e8:24:77:be,1Using init schedule w/:
#0: , #1: 34:13:e8:24:77:be,1, #2: 34:13:e8:24:77:be,1, #3: 34:13:e8:24:77:be,1, #4: 34:13:e8:24:77:be,1, #5: , #6: , #7: , #8: , #9: , nl80211 init called v2
Worker routine started ... ready to receive new configuration messages via ZMQ socket.
INFO - Update HMAC with new configuration ...
INFO - Send ctrl req message to HMAC: 5,34:13:e8:24:77:be,1#6,34:13:e8:24:77:be,1#7,34:13:e8:24:77:be,1#8,34:13:e8:24:77:be,1
Received new configuration update: 5,34:13:e8:24:77:be,1#6,34:13:e8:24:77:be,1#7,34:13:e8:24:77:be,1#8,34:13:e8:24:77:be,1
INFO - Received ctrl reply message from HMAC: OK
INFO - HMAC is updated ...
INFO - [
INFO - 0: 
INFO - 1: 
INFO - 2: 
INFO - 3: 
INFO - 4: 
INFO - 5: 34:13:e8:24:77:be/1,
INFO - 6: 34:13:e8:24:77:be/1,
INFO - 7: 34:13:e8:24:77:be/1,
INFO - 8: 34:13:e8:24:77:be/1,
INFO - 9: 
INFO - ]
Average slot duration: 19992.50
INFO - Stopping HMAC
INFO - Send ctrl req message to HMAC: 0,FF:FF:FF:FF:FF:FF,255#1,FF:FF:FF:FF:FF:FF,255#2,FF:FF:FF:FF:FF:FF,255#3,FF:FF:FF:FF:FF:FF,255#4,FF:FF:FF:FF:FF:FF,255#5,FF:FF:FF:FF:FF:FF,255#6,FF:FF:FF:FF:FF:FF,255#7,FF:FF:FF:FF:FF:FF,255#8,FF:FF:FF:FF:FF:FF,255#9,FF:FF:FF:FF:FF:FF,255
Received new configuration update: 0,FF:FF:FF:FF:FF:FF,255#1,FF:FF:FF:FF:FF:FF,255#2,FF:FF:FF:FF:FF:FF,255#3,FF:FF:FF:FF:FF:FF,255#4,FF:FF:FF:FF:FF:FF,255#5,FF:FF:FF:FF:FF:FF,255#6,FF:FF:FF:FF:FF:FF,255#7,FF:FF:FF:FF:FF:FF,255#8,FF:FF:FF:FF:FF:FF,255#9,FF:FF:FF:FF:FF:FF,255
INFO - Received ctrl reply from HMAC: OK
Received new configuration update: TERMINATE
INFO - Received ctrl reply from HMAC: OK
INFO - HMAC is stopped ...
Terminating ...
```

In the example, first the ATH9K HMAC is started with a initial configuration with 10 slots in which the slots 1-4 are only enabled for traffic which is destined to STA 34:13:e8:24:77:be, the TID MAP with 1 maps to 0b00000001 which means TID 1 is enabled, which means in turn best effort traffic is enabled all other TIDs are paused. The slots 0 and 5-9 are paused for all traffic.

In the second step of the example, the update functionality is called and the schedule is changed, now the slots 5-8 are used by Best Effort traffic which is destined to STA 34:13:e8:24:77:be, while the slots 0-4 and 9 are paused for everyone.

Finally in the last step of the example, the terminate functionality is called which first enables all TIDs of all STAs and second terminates the user-space daemon and therefore deactivates the ATH9k HMAC. 


#ATH9k Advanced Configuration

## How to install HMAC on other Linux distributions and Kernels:

Go to  https://www.kernel.org/pub/linux/kernel/projects/backports/stable
and look for the backports version that fits for your kernel, download and unpack  it.
```
e.g.
cd /tmp/
mkdir backports
cd backports
wget https://www.kernel.org/pub/linux/kernel/projects/backports/stable/v3.12.8/backports-3.12.8-1.tar.gz
tar -xzf backports-3.12.8-1.tar.gz
```
Download ATH9k-HMAC Patch for your kernel (hopefully we have the correct one for you, otherwise you have to adjust the patch on your own or switch to another kernel), apply patch to your backports source.
```
cd /tmp/
https://github.com/szehl/ath9k-hmac.git
cd backports/backports-3.12.8-1/
patch -t -p3 < ../../patch/ath9k-hmac/ath9k-hmac-backports-3.12.8-1.patch
```
Build and install the ATH9k-HMAC driver.
```
cd /tmp/backports/backports-3.12.8-1/
make defconfig-ath9k
make -j4
sudo make install
```
After Reboot the ATH9k-HMAC should be installed, you can check by typing
```
dmesg | grep HMAC
```
If the output is something like:
```
[    3.711813] ath: ATH9K HMAC ENABLED
```
Everything went well.


## Steer ATH9K HMAC driver directly (without Python Wrapper)
This can also be used as a first functional test without the Python wrapper. Just execute the following line in the hmac_userspace_daemon folder (replace wlan0 with your ATH9k WiFi interface, take sure it is up and rfkill is disabled (e.g. sudo rfkill unblock all)):
```
sudo ./hmac_userspace_daemon -i wlan0 -f 20000 -n 10 -c 1,b8:a3:86:96:96:8a,1#2,b8:a3:86:96:96:8a,1#3,b8:a3:86:96:96:8a,1#4,b8:a3:86:96:96:8a,1#6,ec:1f:72:82:09:56,1#7,ec:1f:72:82:09:56,1#8,ec:1f:72:82:09:56,1#9,ec:1f:72:82:09:56,1
```
If no error is shown and the daemon just prints out the current slot size, we are fine, the HMAC is working.


The HMAC user-space deaemon is using the following configuration parameters:
```
sudo ./hmac_userspace_daemon 
-i wlan0                     # ATH9k WiFi Interface on which HMAC schedule should be applied
-f 20000                     # Size of each slot in micro seconds
-n 10                        # Number of Slots
-c                           # Schedule, format: "Slotnumber","MAC Address of Destination","TID Bitmap"#
                             #e.g.:
1,b8:a3:86:96:96:8a,1#2,ec:1f:72:82:09:56,1#3,b8:a3:86:96:96:8a,1#4,b8:a3:86:96:96:8a,1#6,ec:1f:72:82:09:56,1#7,ec:1f:72:82:09:56,1#8,ec:1f:72:82:09:56,1#9,ec:1f:72:82:09:56,1
```
The example uses the following configuration: Interface: wlan0, Size of each Slot: 20ms, Number of Slots: 10 (SuperSlot = 200ms), Scheduler Konfiguration: first slot, Link with STA b8:a3:86:96:96:8a, TID MAP: 0b0000001 means TID 1 (Best Effort), '#' is used as seperator, second slot: Link with STA ec:1f:72:82:09:56, TID TID MAP: 0b0000001 means TID 1 (Best Effort), ... etc.

Note that if the schedule configuration contains no entry for a specific slot, global sleep mode (All ATH9k Software Queues are paused) is activated during this slot.

## 9. Contact
* Sven Zehl, TU-Berlin, zehl@tkn
* Anatolij Zubow, TU-Berlin, zubow@tkn
* Adam Wolisz, TU-Berlin, wolisz@tkn
* tkn = tkn.tu-berlin.de
* 

## 10. How to reference hMAC
Please use the following bibtex :

```
@techreport{zehl16hmac,
Title = {{ hMAC: Enabling Hybrid TDMA/CSMA on IEEE 802.11 Hardware} }},
Author = { Zehl, Sven and Zubow, Anatolij and Wolisz, Adam},
Year = {2016},
Number = {TKN-16-004},
Month = {November},
Institution = {Telecommunication Networks Group, Technische Universit\"at Berlin}
}
```
