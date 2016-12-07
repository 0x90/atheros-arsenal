ath9k-watchdog
==============

OpenWRT-Package for resetting Atheros-Nodes on wifi hangs. Current Attitude Adjustment builds provide buggy ath9k-drivers.
* https://dev.openwrt.org/ticket/9654
* https://dev.openwrt.org/ticket/11862

Although these tickets seem to track generic race-conditions as of today (2012-02-06) wifi hangups correlate with

`ath: phy0: Could not stop RX, we could be confusing the DMA engine when we start RX up`

beeing present in the kernel ringbuffer (dmesg). This watchdog takes care of this problem by
* Dumping the kernel-ringbuffer to a file in /usr/lib/ath9k-watchdog
* Rebooting the router on occurance
* Periodically checking /usr/lib/ath9k-watchdog for crash-reports and uploading 'em.

Please note that this package cannot be re-used easily. It contains hardcoded IP-addresses, undeclared dependencies (luci, wget(-no-ssl) and fixed paths to files of other packages (lib_node.sh)
