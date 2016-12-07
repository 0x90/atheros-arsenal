Odin patch
==========

Adapted from the original patch here:

https://github.com/lalithsuresh/odin-driver-patches

Instructions
------------

Download your kernel sources (Ubuntu Trusty 14.04 kernel 3.13.0-39-generic)

```
  $: git clone git://kernel.ubuntu.com/ubuntu/linux.git
  $: git clone --reference linux git://kernel.ubuntu.com/ubuntu/ubuntu-trusty.git
```

Inspect the tags and find your current kernel version

```
  $: git tag -l Ubuntu-*
  $: git checkout -b my_kernel Ubuntu-3.13.0-39
```

Configure the kernel - i.e. Select the debugfs option for ath driver

```
  $: make menuconfig
```

Device drivers -> Network device support -> Wireless LAN -> Atheros wireless cards -> Atheros wireless debugging.
This will set the flag CONFIG_ATH_DEBUG. (alternatvly you can edit .config by hand). You can doublecheck the .config file

```
  $: cat .config | grep CONFIG_ATH
```

Build the kernel module

Copy the Module.symvers from your system into the kernel source root directory

```
  $: cd path_kernel_sources
  $: cp /usr/src/linux-headers-3.13.0-39-generic/Module.symvers .
```

Patch the htc_drv_debug.c file in path_kernel_src/drivers/net/wireless/ath/ath9k/

```
  $: patch < path_to_ath_patch/patch.patch
  $: make modules_prepare
  $: make modules SUBDIRS=drivers/net/wireless/ath
```

Remove and load the modules using the following script

```
#!/bin/bash

rmmod ath9k_htc
rmmod ath9k_common
rmmod ath9k_hw
rmmod ath
insmod ./drivers/net/wireless/ath/ath.ko 
insmod ./drivers/net/wireless/ath/ath9k/ath9k_hw.ko
insmod ./drivers/net/wireless/ath/ath9k/ath9k_common.ko
insmod ./drivers/net/wireless/ath/ath9k/ath9k_htc.ko
```

After that you should replug the wireless device. If you want to make those changes permanent you can install the ath9k_htc module directly by coping it into /lib/modules/KERNEL_VERSION/kernel/drivers/net/wireless/ath/ath9k/

Utilities
---------

Check info about a module

```
  $: modinfo MODULE
```

Monitor the log output for tracing errors

```
  $: tail -f /var/sys/syslog
```
