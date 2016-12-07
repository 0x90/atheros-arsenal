#ifndef	__IF_OTUS_DEBUG__
#define	__IF_OTUS_DEBUG__

enum {
	OTUS_DEBUG_XMIT		= 0x00000001ULL,	/* basic xmit operation */
	OTUS_DEBUG_XMIT_DUMP	= 0x00000002ULL,	/* xmit dump */
	OTUS_DEBUG_RECV		= 0x00000004ULL,	/* basic recv operation */
	OTUS_DEBUG_TX_PROC	= 0x00000008ULL,	/* tx ISR proc */
	OTUS_DEBUG_RX_PROC	= 0x00000010ULL,	/* rx ISR proc */
	OTUS_DEBUG_RECV_ALL	= 0x00000020ULL,	/* trace all frames (beacons) */
	OTUS_DEBUG_INIT		= 0x00000040ULL,	/* initialization of dev */
	OTUS_DEBUG_DEVCAP	= 0x00000080ULL,	/* dev caps */
	OTUS_DEBUG_CMDS		= 0x00000100ULL,	/* commands */
	OTUS_DEBUG_CMDS_DUMP	= 0x00000200ULL,	/* command buffer dump */
	OTUS_DEBUG_RESET	= 0x00000400ULL,	/* reset processing */
	OTUS_DEBUG_STATE	= 0x00000800ULL,	/* 802.11 state transitions */
	OTUS_DEBUG_MULTICAST	= 0x00001000ULL,	/* multicast */
	OTUS_DEBUG_WME		= 0x00002000ULL,	/* WME */
	OTUS_DEBUG_CHANNEL	= 0x00004000ULL,	/* channel */
	OTUS_DEBUG_RATES	= 0x00008000ULL,	/* rates */
	OTUS_DEBUG_CRYPTO	= 0x00010000ULL,	/* crypto */
	OTUS_DEBUG_LED		= 0x00020000ULL,	/* LED */
	OTUS_DEBUG_USB_XFER	= 0x00040000ULL,	/* USB transactions */
	OTUS_DEBUG_RECV_STREAM	= 0x00080000ULL,	/* RX stream decoding */
	OTUS_DEBUG_ANY		= 0xffffffffffffffffULL
};
#define DPRINTF(sc, m, fmt, ...) do {			\
	if (sc->sc_debug & (m))				\
		printf(fmt, __VA_ARGS__);		\
} while (0)

#define	DODEBUG(sc, m)		((sc)->sc_debug & m)

#endif	/* __IF_OTUS_DEBUG__ */
