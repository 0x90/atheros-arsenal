#ifndef	__IF_OTUSVAR_H__
#define	__IF_OTUSVAR_H__

#define	OTUS_CONFIG_INDEX		0
#define	OTUS_IFACE_INDEX		0

/*
 * The carl9170 firmware has the following specification:
 *
 * 0 - USB control
 * 1 - TX
 * 2 - RX
 * 3 - IRQ
 * 4 - CMD
 * ..
 * 10 - end
 */
enum {
	OTUS_BULK_TX,
	OTUS_BULK_RX,
	OTUS_BULK_IRQ,
	OTUS_BULK_CMD,
	OTUS_N_XFER
};

#define	OTUS_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define	OTUS_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define	OTUS_LOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)
#define	OTUS_UNLOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_mtx, MA_NOTOWNED)

/* XXX the TX/RX endpoint dump says it's 0x200, (512)? */
#define	OTUS_MAX_TXSZ		512
#define	OTUS_MAX_RXSZ		512
/* intr/cmd endpoint dump says 0x40 */
#define OTUS_MAX_CTRLSZ		64

#define	OTUS_STAT_INC(sc, s)	/* */
#define	OTUS_STAT_DEC(sc, s)	/* */

struct otus_softc {
	device_t		sc_dev;
	struct usb_device	*sc_udev;
	struct ifnet		*sc_ifp;
	struct mtx		sc_mtx;
	struct usb_xfer		*sc_xfer[OTUS_N_XFER];
	struct carl9170_firmware_info	fwinfo;
	uint64_t		sc_debug;
	int			sc_detaching;	/* About to detach */

	struct callout		sc_ping_callout;
	struct taskqueue	*sc_tq;

	/* Ping task - needs to be a taskqueue so we can sleep */
	struct task		sc_ping_task;

	/* Command queue handling */
	struct otus_cmd		sc_cmd[OTUS_CMD_LIST_COUNT];
	otus_cmdhead		sc_cmd_active;
	otus_cmdhead		sc_cmd_inactive;
	otus_cmdhead		sc_cmd_pending;
	otus_cmdhead		sc_cmd_waiting;
	int			sc_cur_rxcmdseq;

	/* Receive handling */
	struct mbuf		*sc_rx_m;
};

#endif	/* __IF_OTUSVAR__ */
