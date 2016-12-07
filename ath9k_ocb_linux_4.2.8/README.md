# ath9k_ocb_linux_4.2.8
This repository contains a modification of linux 4.2.8 ath9k driver. This modification support OCB mode for atheros cards which it use the ath9k driver. 

##Introduction
The source of the kernel 4.2.8 is [kernel 4.x](https://www.kernel.org/pub/linux/kernel/v4.x/) and the specific version
it is [linux-4.2.8.tar.xz](https://www.kernel.org/pub/linux/kernel/v4.x/linux-4.2.8.tar.xz) 15-Dec-2015 06:08 82M. 

It had only been modified the files:
- drivers/net/wireless/ath/ath9k/ani.c
- drivers/net/wireless/ath/ath9k/ath9k.h
- drivers/net/wireless/ath/ath9k/common-init.c
- drivers/net/wireless/ath/ath9k/debug.c
- drivers/net/wireless/ath/ath9k/htc_drv_init.c
- drivers/net/wireless/ath/ath9k/hw.c
- drivers/net/wireless/ath/ath9k/hw.h
- drivers/net/wireless/ath/ath9k/init.c
- drivers/net/wireless/ath/ath9k/main.c
- drivers/net/wireless/ath/ath9k/recv.c
- drivers/net/wireless/ath/regd.c

And the changes that it had been made,it was following the same changes that @github/lisovy had made in the
repository [CTU-IIG/802.11p-linux](https://github.com/CTU-IIG/802.11p-linux/commit/bf45e0160af428dac8893e48d506ac428fed16b2).

##Instructions

Copy the folder "drivers" in your onw kernel (4.2.8), it is the only way to install them. After this, you can compile the kernel
with the proper configuration.

- Default flags for ath9k driver modules, they could be find in:
  - **Device Drivers > Network device support > Wireless LAN > Atheros Wireless Cards**
- You must add the OCB mode support to the mac80211 subsytem. Activating the debugging mode of HT, STATION, IBSS and OCB. These flags could be find in:
  - **Networking support > Wireless > Select mac80211 debugging features**
    - Verbose station debugging - "CONFIG_MAC80211_STA_DEBUG=y"
    - Verbose HT debugging - "CONFIG_MAC80211_HT_DEBUG=y"
    - Verbose OCB debugging - "CONFIG_MAC80211_OCB_DEBUG=y"
    - Vebose IBSS debugging - "CONFIG_MAC80211_IBSS_DEBUG=y"

