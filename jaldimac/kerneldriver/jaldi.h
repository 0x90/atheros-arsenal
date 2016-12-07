/* JaldiMAC
 * GPL. 
 *
 * Written by Shaddi Hasan (shaddi@eecs.berkeley.edu)
 * Based strongly off ath9k, architecture inspired by i2400m.
 * 
 * GENERAL DRIVER ARCHITECTURE
 * 
 * THe jaldi driver is split into two major parts:
 *
 * 	- Device specific driver (so far only targets UBNT NSM5, running AR9280)
 *	- Device generic part (this part)
 *
 * Because of the ath9k heritage, this demarcation may be fuzzy at times. The 
 * device specific part of the driver handles tasks involving moving data
 * between the kernal and the device, and bus-related tasks such as probing,
 * hardware resets, and disconnecting. 
 *
 * The device generic part aims to implement a fairly normal layer, if thin, 
 * layer between Linux and the hardware. It appears as an Ethernet device for 
 * the sake of simplicity, but packets passed to this driver should follow the 
 * as-yet-to-be-determined JaldiMAC packet format. 
 */



#ifndef JALDI_H
#define JALDI_H

#include <linux/etherdevice.h>
#include <linux/device.h>
#include <linux/io.h>

#include "hw.h"
#include "debug.h"

/* Macro to expand scalars to 64-bit objects */
#define	ito64(x) (sizeof(x) == 1) ?			\
	(((unsigned long long int)(x)) & (0xff)) :	\
	(sizeof(x) == 2) ?				\
	(((unsigned long long int)(x)) & 0xffff) :	\
	((sizeof(x) == 4) ?				\
	 (((unsigned long long int)(x)) & 0xffffffff) : \
	 (unsigned long long int)(x))

#define	JALDI_TXQ_SETUP(sc, i)        ((sc)->tx.txqsetup & (1<<i))

#define SC_OP_INVALID                BIT(0)
#define SC_OP_BEACONS                BIT(1)
#define SC_OP_RXAGGR                 BIT(2)
#define SC_OP_TXAGGR                 BIT(3)
#define SC_OP_FULL_RESET             BIT(4)
#define SC_OP_PREAMBLE_SHORT         BIT(5)
#define SC_OP_PROTECT_ENABLE         BIT(6)
#define SC_OP_RXFLUSH                BIT(7)
#define SC_OP_LED_ASSOCIATED         BIT(8)
#define SC_OP_LED_ON                 BIT(9)
#define SC_OP_SCANNING               BIT(10)
#define SC_OP_TSF_RESET              BIT(11)
#define SC_OP_BT_PRIORITY_DETECTED   BIT(12)
#define SC_OP_BT_SCAN                BIT(13)

/*************************/
/* Descriptor Management */
/*************************/

#define JALDI_TXBUF_RESET(_bf) do {				\
		(_bf)->bf_stale = false;			\
		(_bf)->bf_lastbf = NULL;			\
		(_bf)->bf_next = NULL;				\
		memset(&((_bf)->bf_state), 0, 			\
			sizeof(struct jaldi_buf_state));	\
	} while (0)

#define JALDI_RXBUF_RESET(_bf) do {		\
		(_bf)->bf_stale = false;	\
	} while (0)

/***********/
/* RX / TX */
/***********/
#define JALDI_MAX_MPDU_LEN	1500 /* MTU, bytes */
#define JALDI_MAX_ANTENNA	3
#define JALDI_NUM_RXBUF		512
#define JALDI_NUM_TXBUF		512
#define JALDI_TX_ERROR        0x01


struct jaldi_wiphy;
struct jaldi_rate_table;

enum qos_type {
	JALDI_QOS_BULK,
	JALDI_QOS_LATENCY_SENSITIVE,
	JALDI_QOS_UNDEFINED,
};

enum jaldi_freq_band {
	JALDI_2GHZ = 0,
	JALDI_5GHZ,
};

enum jaldi_pkt_type {
        JALDI_PKT_TYPE_NORMAL = 0, /* all packets that are tx'd are of this type */
        JALDI_PKT_TYPE_CONTROL,    /* packets used to set an internal setting (not tx'd) */
};

struct jaldi_packet {
	struct jaldi_packet *next;
	struct jaldi_softc *sc;
	struct sk_buff *skb;
	enum jaldi_pkt_type type;
	struct jaldi_txq *txq;
	int datalen;
	char *data;
	s64 tx_time; /* the time at which this packet should be sent */
	int qos_type;
};

struct jaldi_buf_state {
	int bfs_nframes;
	u16 bfs_al;
	u16 bfs_frmlen;
	int bfs_seqno;
	int bfs_tidno;
	int bfs_retries;
	u8 bfs_type;
};

/* TIL: when experienced kernel devs write weird code they're probably doing it that 
 * way for a reason. */
#define bf_nframes      	bf_state.bfs_nframes
#define bf_al           	bf_state.bfs_al
#define bf_frmlen       	bf_state.bfs_frmlen
#define bf_retries      	bf_state.bfs_retries
#define bf_seqno        	bf_state.bfs_seqno
#define bf_tidno        	bf_state.bfs_tidno

struct jaldi_buf {
	struct list_head list;

	struct jaldi_buf *bf_lastbf;    /* for aggregation, the last bf */
	struct jaldi_buf *bf_next;

	void *bf_desc;			/* virtual addr of desc */
	dma_addr_t bf_daddr;		/* physical addr of desc */
	dma_addr_t bf_buf_addr;		/* physical addr of data buffer */
	dma_addr_t bf_dmacontext;
	struct sk_buff *bf_mpdu; 	/* MAC protocol data unit */
	
	bool bf_stale;
	bool bf_tx_aborted;

	struct jaldi_buf_state bf_state;
	
	u16 bf_flags;
};

struct jaldi_descdma {
	void *dd_desc;
	dma_addr_t dd_desc_paddr;
	u32 dd_desc_len;
	struct jaldi_buf *dd_bufptr;
};


/**********/
/*  MAC   */
/**********/
struct jaldi_tx {
	u16 seq_no;
	u32 txqsetup;
	int hwq_map[JALDI_WME_AC_VO+1]; // the five jaldi hw queues
	spinlock_t txbuflock;
	struct list_head txbuf;
	struct jaldi_txq txq[JALDI_NUM_TX_QUEUES];
	struct jaldi_descdma txdma;
};

struct jaldi_rx {
	u8 defant;
	u8 rxotherant;
	u32 *rxlink;
	unsigned int rxfilter;
	spinlock_t rxflushlock;
	spinlock_t rxbuflock;
	struct list_head rxbuf;
	struct jaldi_descdma rxdma;
	struct jaldi_buf *rx_bufptr;
};

struct jaldi_softc {
	struct device *dev;
	struct net_device *net_dev;

	/* Hardware related */
	struct tasklet_struct intr_tq; // jaldi_tasklet, general intr bottom
	struct jaldi_hw *hw; // from ath9k_hw, hw main struct
	void __iomem *mem; // see pci_iomap and lwn article
	int irq; // irq number...
	struct mutex mutex;
	spinlock_t sc_resetlock;
	spinlock_t sc_serial_rw;
	spinlock_t sc_pm_lock;

	struct hrtimer tx_timer;

	u32 intrstatus; // keep track of reason for interrupt
	u32 sc_flags; 
	bool hw_ready; // flag to see if hw is ready
	u16 ps_flags; /* powersave */
	unsigned long ps_usecount;
	bool ps_idle;
	u16 curtxpow; /* tx power (.5 dBm units) */
	u16 cachelsz;

	struct jaldi_channel *chans[2];
	struct jaldi_channel curchan;


	/* netdev */
	struct net_device_stats stats;
	int status;
	int rx_int_enabled;
	int tx_int_enabled;
	struct jaldi_packet *tx_queue; /* packets scheduled for sending */
	struct sk_buff *skb;
	spinlock_t sc_netdevlock;

	u8 macaddr[ETH_ALEN];

	/* tx/rx */
	struct jaldi_tx tx;
	struct jaldi_rx rx;
	u32 rx_bufsize;

	/* ops */
	// none at softc level yet...

	struct jaldi_debug debug;
};

static const struct net_device_ops jaldi_netdev_ops;

// PCI/AHB init 
int jaldi_pci_init(void);
void jaldi_pci_exit(void);
int jaldi_ahb_init(void);
void jaldi_ahb_exit(void);

/* netdev */
struct net_device *jaldi_init_netdev(void);
int jaldi_start_netdev(struct jaldi_softc *sc);
void jaldi_attach_netdev_ops(struct net_device *dev);
void jaldi_tasklet(unsigned long data);

int jaldi_hw_reset(struct jaldi_hw *hw, struct jaldi_channel *chan, bool bChannelChange);
bool jaldi_setpower(struct jaldi_softc *sc, enum jaldi_power_mode mode);
void jaldi_ps_wakeup(struct jaldi_softc *sc);

int jaldi_init_softc(u16 devid, struct jaldi_softc *sc, u16 subsysid, const struct jaldi_bus_ops *bus_ops);
int jaldi_init_device(u16 devid, struct jaldi_softc *sc, u16 subsysid, const struct jaldi_bus_ops *bus_ops);
void jaldi_deinit_device(struct jaldi_softc *sc);
int jaldi_init_interrupts(struct jaldi_softc *sc); 
struct jaldi_txq *jaldi_txq_setup(struct jaldi_softc *sc, int qtype, int subtype);
int jaldi_tx_setup(struct jaldi_softc *sc, int haltype);
void jaldi_tx_cleanup(struct jaldi_softc *sc);
void jaldi_rx_cleanup(struct jaldi_softc *sc);
struct sk_buff *jaldi_rxbuf_alloc(struct jaldi_softc *sc, u32 len);

int jaldi_descdma_setup(struct jaldi_softc *sc, struct jaldi_descdma *dd,
		      struct list_head *head, const char *name,
		      int nbuf, int ndesc, bool is_tx);
void jaldi_descdma_cleanup(struct jaldi_softc *sc, struct jaldi_descdma *dd,
			 struct list_head *head);

void jaldi_tx_cleanupq(struct jaldi_softc *sc, struct jaldi_txq *txq);
irqreturn_t jaldi_isr(int irq, void *dev);
int jaldi_rx_tasklet(struct jaldi_softc *sc, int flush);

#endif /* JALDI_H */

