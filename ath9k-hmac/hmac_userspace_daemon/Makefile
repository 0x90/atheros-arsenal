all: hybrid_tdma_csma_mac_x86

hybrid_tdma_csma_mac_x86:
	g++ hybrid_tdma_csma_mac.c -I /usr/include/libnl3/ -DCONFIG_LIBNL30 -o hmac_userspace_daemon -levent -lnl-genl-3 -lnl-3 -lzmq -lstdc++ -lpthread
clean:
	rm hmac_userspace_daemon
