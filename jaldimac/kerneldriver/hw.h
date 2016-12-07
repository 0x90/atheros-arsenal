/* deals with hw; bare-bones implementation of ath9k_hw
 *
 * TODO:
 * - Bus operations (init/deinit)
 * - Firmware loading?
 * - PHY (channels, power, interface for manipulating hardware)
 */

#ifndef HW_H
#define HW_H

#include "reg.h"
#include "eeprom.h"
#include "phy.h"
#include "mac.h"


#define ATHEROS_VENDOR_ID	0x168c

#define AR5416_DEVID_PCI	0x0023
#define AR5416_DEVID_PCIE	0x0024
#define AR9160_DEVID_PCI	0x0027
#define AR9280_DEVID_PCI	0x0029
#define AR9280_DEVID_PCIE	0x002a
#define AR9285_DEVID_PCIE	0x002b
#define AR2427_DEVID_PCIE	0x002c
#define AR9287_DEVID_PCI	0x002d
#define AR9287_DEVID_PCIE	0x002e
#define AR9300_DEVID_PCIE	0x0030

#define AR5416_AR9100_DEVID	0x000b

#define	AR_SUBVENDOR_ID_NOG	0x0e11
#define AR_SUBVENDOR_ID_NEW_A	0x7065
#define AR5416_MAGIC		0x19641014

#define DEFAULT_CACHELINE	32

#define JALDI_AMPDU_LIMIT_MAX		(64 * 1024 - 1) /* Not doing frame agg... */
#define	JALDI_DEFAULT_NOISE_FLOOR	-95
#define JALDI_RSSI_BAD			-128
#define JALDI_WAIT_TIMEOUT		100000 /* (us) */
#define JALDI_TIME_QUANTUM		10
#define POWER_UP_TIME			10000
#define SPUR_RSSI_THRESH		40

#define MAX_RATE_POWER			63

#define JALDI_CLOCK_RATE_CCK		22
#define JALDI_CLOCK_RATE_5GHZ_OFDM	40
#define JALDI_CLOCK_RATE_2GHZ_OFDM	44
#define JALDI_CLOCK_FAST_RATE_5GHZ_OFDM 44


/* Register operation macros */
#define REG_WRITE(_hw, _reg, _val) \
	(_hw)->reg_ops->write((_hw), (_val), (_reg))

#define REG_READ(_hw, _reg) \
	(_hw)->reg_ops->read((_hw), (_reg))

#define ENABLE_REGWRITE_BUFFER(_hw)					\
	do {								\
		if (AR_SREV_9271(_hw))					\
			(_hw)->reg_ops->enable_write_buffer((_hw)); 	\
	} while (0)

#define DISABLE_REGWRITE_BUFFER(_hw)					\
	do {								\
		if (AR_SREV_9271(_hw))					\
			(_hw)->reg_ops->disable_write_buffer((_hw));	\
	} while (0)

#define REGWRITE_BUFFER_FLUSH(_hw)					\
	do {								\
		if (AR_SREV_9271(_hw))					\
			(_hw)->reg_ops->write_flush((_hw)); 		\
	} while (0)

/* Shift and mask (and vice versa)
 * "_f##_S" appends an "_S" to the passed field name; reg.h contains shift 
 * amounts for each field. */
#define SM(_v, _f)  (((_v) << _f##_S) & _f)
#define MS(_v, _f)  (((_v) & _f) >> _f##_S)

/* RMW: "Read modify write" */
#define REG_RMW(_a, _r, _set, _clr)    \
	REG_WRITE(_a, _r, (REG_READ(_a, _r) & ~(_clr)) | (_set))
#define REG_RMW_FIELD(_a, _r, _f, _v) \
	REG_WRITE(_a, _r, \
	(REG_READ(_a, _r) & ~_f) | (((_v) << _f##_S) & _f))
#define REG_READ_FIELD(_a, _r, _f) \
	(((REG_READ(_a, _r) & _f) >> _f##_S))
#define REG_SET_BIT(_a, _r, _f) \
	REG_WRITE(_a, _r, REG_READ(_a, _r) | _f)
#define REG_CLR_BIT(_a, _r, _f) \
	REG_WRITE(_a, _r, REG_READ(_a, _r) & ~_f)

#define DO_DELAY(x) do {			\
		if ((++(x) % 64) == 0)          \
			udelay(1);		\
	} while (0)

#define REG_WRITE_ARRAY(iniarray, column, regWr) do {                   \
		int r;							\
		for (r = 0; r < ((iniarray)->ia_rows); r++) {		\
			REG_WRITE(ah, INI_RA((iniarray), (r), 0),	\
				  INI_RA((iniarray), r, (column)));	\
			DO_DELAY(regWr);				\
		}							\
	} while (0)

// it's unclear what these do: document TODO
#define SM(_v, _f)  (((_v) << _f##_S) & _f)
#define MS(_v, _f)  (((_v) & _f) >> _f##_S)
/* End register r/w macros */


#define CHANNEL_CCK       0x00020
#define CHANNEL_OFDM      0x00040
#define CHANNEL_2GHZ      0x00080
#define CHANNEL_5GHZ      0x00100
#define CHANNEL_PASSIVE   0x00200
#define CHANNEL_DYN       0x00400
#define CHANNEL_HALF      0x04000
#define CHANNEL_QUARTER   0x08000
#define CHANNEL_HT20      0x10000
#define CHANNEL_HT40PLUS  0x20000
#define CHANNEL_HT40MINUS 0x40000

#define CHANNEL_A           (CHANNEL_5GHZ|CHANNEL_OFDM)
#define CHANNEL_B           (CHANNEL_2GHZ|CHANNEL_CCK)
#define CHANNEL_G           (CHANNEL_2GHZ|CHANNEL_OFDM)
#define CHANNEL_G_HT20      (CHANNEL_2GHZ|CHANNEL_HT20)
#define CHANNEL_A_HT20      (CHANNEL_5GHZ|CHANNEL_HT20)
#define CHANNEL_G_HT40PLUS  (CHANNEL_2GHZ|CHANNEL_HT40PLUS)
#define CHANNEL_G_HT40MINUS (CHANNEL_2GHZ|CHANNEL_HT40MINUS)
#define CHANNEL_A_HT40PLUS  (CHANNEL_5GHZ|CHANNEL_HT40PLUS)
#define CHANNEL_A_HT40MINUS (CHANNEL_5GHZ|CHANNEL_HT40MINUS)
#define CHANNEL_ALL				\
	(CHANNEL_OFDM|				\
	 CHANNEL_CCK|				\
	 CHANNEL_2GHZ |				\
	 CHANNEL_5GHZ |				\
	 CHANNEL_HT20 |				\
	 CHANNEL_HT40PLUS |			\
	 CHANNEL_HT40MINUS)

#define IS_CHAN_G(_c) ((((_c)->channelFlags & (CHANNEL_G)) == CHANNEL_G) || \
       (((_c)->channelFlags & CHANNEL_G_HT20) == CHANNEL_G_HT20) || \
       (((_c)->channelFlags & CHANNEL_G_HT40PLUS) == CHANNEL_G_HT40PLUS) || \
       (((_c)->channelFlags & CHANNEL_G_HT40MINUS) == CHANNEL_G_HT40MINUS))
#define IS_CHAN_OFDM(_c) (((_c)->channelFlags & CHANNEL_OFDM) != 0)
#define IS_CHAN_5GHZ(_c) (((_c)->channelFlags & CHANNEL_5GHZ) != 0)
#define IS_CHAN_2GHZ(_c) (((_c)->channelFlags & CHANNEL_2GHZ) != 0)
#define IS_CHAN_HALF_RATE(_c) (((_c)->channelFlags & CHANNEL_HALF) != 0)
#define IS_CHAN_QUARTER_RATE(_c) (((_c)->channelFlags & CHANNEL_QUARTER) != 0)
#define IS_CHAN_A_FAST_CLOCK(_hw, _c)			\
	((((_c)->channelFlags & CHANNEL_5GHZ) != 0) &&	\
	 ((_hw)->caps.hw_caps & JALDI_HW_CAP_FASTCLOCK))

/* Macros for checking chanmode */
#define IS_CHAN_5GHZ(_c) (((_c)->channelFlags & CHANNEL_5GHZ) != 0)
#define IS_CHAN_2GHZ(_c) (((_c)->channelFlags & CHANNEL_2GHZ) != 0)
#define IS_CHAN_B(_c) ((_c)->chanmode == CHANNEL_B)
#define IS_CHAN_HT20(_c) (((_c)->chanmode == CHANNEL_HT20)) 
#define IS_CHAN_HT40(_c) (((_c)->chanmode == CHANNEL_HT40PLUS) || \
			  ((_c)->chanmode == CHANNEL_HT40MINUS))
#define IS_CHAN_HT(_c) (IS_CHAN_HT20((_c)) || IS_CHAN_HT40((_c)))

#define BASE_ACTIVATE_DELAY     100
#define RTC_PLL_SETTLE_DELAY    100
#define INIT_CONFIG_STATUS      0x00000000
#define INIT_RSSI_THR           0x00000700

#define JALDI_HW_RX_HP_QDEPTH	16
#define JALDI_HW_RX_LP_QDEPTH	128

#define SPUR_DISABLE        	0
#define SPUR_ENABLE_IOCTL   	1
#define SPUR_ENABLE_EEPROM  	2
#define AR_EEPROM_MODAL_SPURS   5
#define AR_SPUR_5413_1      	1640
#define AR_SPUR_5413_2      	1200
#define AR_NO_SPUR      	0x8000
#define AR_BASE_FREQ_2GHZ   	2300
#define AR_BASE_FREQ_5GHZ   	4900
#define AR_SPUR_FEEQ_BOUND_HT40 19
#define AR_SPUR_FEEQ_BOUND_HT20 10

/* tx fifo thresholds
 * Single stream device AR9285 and AR9271 require 2 KB
 * to work around a hardware issue, all other devices
 * have can use the max 4 KB limit.
 */
#define MIN_TX_FIFO_THRESHOLD   0x1 /* 64 byte increments */
#define MAX_TX_FIFO_THRESHOLD   ((4096 / 64) - 1) /* 4KB */

/* calibration */
#define NUM_NF_READINGS		6

enum jaldi_pkt_type;
struct jaldi_softc; 

// closely based upon ath9k_hw_version (a9k/hw.h)
struct jaldi_hw_version {
	u32 magic;
	u32 devid;
	u16 subvendorid;
	u32 macVersion;
	u16 macRev;
	u16 phyRev;
	u16 analog5GhzRev;
	u16 analog2GhzRev; // don't think this is needed for us...
	u16 subsysid;
};

#define HT40_CHANNEL_CENTER_SHIFT	10
// channel mode flags -- from ath9k, many not needed here
#define CHANNEL_CW_INT    0x00002

enum jaldi_intr_type {
	JALDI_INT_RX = 0x00000001,
	JALDI_INT_RXDESC = 0x00000002,
	JALDI_INT_RXHP = 0x00000001,
	JALDI_INT_RXLP = 0x00000002,
	JALDI_INT_RXNOFRM = 0x00000008,
	JALDI_INT_RXEOL = 0x00000010,
	JALDI_INT_RXORN = 0x00000020,
	JALDI_INT_TX = 0x00000040,
	JALDI_INT_TXDESC = 0x00000080,
	JALDI_INT_TIM_TIMER = 0x00000100,
	JALDI_INT_BB_WATCHDOG = 0x00000400,
	JALDI_INT_TXURN = 0x00000800,
	JALDI_INT_MIB = 0x00001000,
	JALDI_INT_RXPHY = 0x00004000,
	JALDI_INT_RXKCM = 0x00008000,
	JALDI_INT_SWBA = 0x00010000,
	JALDI_INT_BMISS = 0x00040000,
	JALDI_INT_BNR = 0x00100000,
	JALDI_INT_TIM = 0x00200000,
	JALDI_INT_DTIM = 0x00400000,
	JALDI_INT_DTIMSYNC = 0x00800000,
	JALDI_INT_GPIO = 0x01000000,
	JALDI_INT_CABEND = 0x02000000,
	JALDI_INT_TSFOOR = 0x04000000,
	JALDI_INT_GENTIMER = 0x08000000,
	JALDI_INT_CST = 0x10000000,
	JALDI_INT_GTT = 0x20000000,
	JALDI_INT_FATAL = 0x40000000,
	JALDI_INT_GLOBAL = 0x80000000,
	JALDI_INT_BMISC = JALDI_INT_TIM |
		JALDI_INT_DTIM |
		JALDI_INT_DTIMSYNC |
		JALDI_INT_TSFOOR |
		JALDI_INT_CABEND,
	JALDI_INT_COMMON = JALDI_INT_RXNOFRM |
		JALDI_INT_RXDESC |
		JALDI_INT_RXEOL |
		JALDI_INT_RXORN |
		JALDI_INT_TXURN |
		JALDI_INT_TXDESC |
		JALDI_INT_MIB |
		JALDI_INT_RXPHY |
		JALDI_INT_RXKCM |
		JALDI_INT_SWBA |
		JALDI_INT_BMISS |
		JALDI_INT_GPIO,
	JALDI_INT_NOCARD = 0xffffffff
};

struct jaldi_channel {
	u16 channel; // MHz
	u16 center_freq; // MHz
	u16 hw_value; // HW-specific channel value (see hw.h, others). Used to index.
	u16 max_power;
	u32 channelFlags; // TODO: not needed?
	u32 chanmode; // 20MHz vs 40Mhz 
	/* TODO: Noise calibration data per channel may go here, as in a9k */
};

// a convenient way to refer to the freq we're on
struct chan_centers {
	u16 synth_center;
	u16 ctl_center;
	u16 ext_center;
};

enum jaldi_power_mode { 
	JALDI_PM_AWAKE = 0,
	JALDI_PM_FULL_SLEEP,
	JALDI_PM_NETWORK_SLEEP,
	JALDI_PM_UNDEFINED,
};

enum ser_reg_mode {
	SER_REG_MODE_OFF = 0,
	SER_REG_MODE_ON = 1,
	SER_REG_MODE_AUTO = 2,
};

enum jaldi_reset_type {
	JALDI_RESET_POWER_ON,
	JALDI_RESET_WARM,
	JALDI_RESET_COLD,
};

enum jaldi_opmode {
	JALDI_UNSPECIFIED,
	JALDI_MASTER,
	JALDI_CLIENT,
};

struct jaldi_bitrate {
	u16 bitrate;
	u16 hw_value;
};

enum jaldi_device_state {
	JALDI_HW_UNAVAILABLE,
	JALDI_HW_INITIALIZED,
};

enum jaldi_bus_type {
	JALDI_PCI,
	JALDI_AHB,
	JALDI_USB,
};

/* For hardware jaldimac supports, we only use the 11NA modes. The rest have
 * been left for the sake of completeness. */
enum wireless_mode {
	JALDI_MODE_11A = 0,
	JALDI_MODE_11G,
	JALDI_MODE_11NA_HT20,
	JALDI_MODE_11NG_HT20,
	JALDI_MODE_11NA_HT40PLUS,
	JALDI_MODE_11NA_HT40MINUS,
	JALDI_MODE_11NG_HT40PLUS,
	JALDI_MODE_11NG_HT40MINUS,
	JALDI_MODE_MAX,
};

/* NB: This is a direct copy of ath9k_hw_caps to avoid modifying low-level 
 * code as much as possible. 
 *
 * VEOL seems to be deprecated in ath9k; 80211 beacons are now generated in
 * software instead. Nonetheless, we keep track of the hw capability here. */
enum jaldi_hw_caps {
	JALDI_HW_CAP_MIC_AESCCM                 = BIT(0),
	JALDI_HW_CAP_MIC_CKIP                   = BIT(1),
	JALDI_HW_CAP_MIC_TKIP                   = BIT(2),
	JALDI_HW_CAP_CIPHER_AESCCM              = BIT(3),
	JALDI_HW_CAP_CIPHER_CKIP                = BIT(4),
	JALDI_HW_CAP_CIPHER_TKIP                = BIT(5),
	JALDI_HW_CAP_VEOL                       = BIT(6), /* Virt end-of-list (hw-generated beacons) */
	JALDI_HW_CAP_BSSIDMASK                  = BIT(7),
	JALDI_HW_CAP_MCAST_KEYSEARCH            = BIT(8),
	JALDI_HW_CAP_HT                         = BIT(9),
	JALDI_HW_CAP_GTT                        = BIT(10), /* Global transmit timeout */
	JALDI_HW_CAP_FASTCC                     = BIT(11), /* Fast channel change */
	JALDI_HW_CAP_RFSILENT                   = BIT(12),
	JALDI_HW_CAP_CST                        = BIT(13),
	JALDI_HW_CAP_ENHANCEDPM                 = BIT(14),
	JALDI_HW_CAP_AUTOSLEEP                  = BIT(15),
	JALDI_HW_CAP_4KB_SPLITTRANS             = BIT(16),
	JALDI_HW_CAP_EDMA			= BIT(17),
	JALDI_HW_CAP_RAC_SUPPORTED		= BIT(18),
	JALDI_HW_CAP_LDPC			= BIT(19),
	JALDI_HW_CAP_FASTCLOCK			= BIT(20),
	JALDI_HW_CAP_SGI_20			= BIT(21),
};

struct jaldi_hw_capabilities {
	u32 hw_caps; /* JALDI_HW_CAP_* from jaldi_hw_caps */
	DECLARE_BITMAP(wireless_modes, JALDI_MODE_MAX); /* JALDI_MODE_* */
	u16 total_queues;
	u16 low_5ghz_chan, high_5ghz_chan;
	u16 low_2ghz_chan, high_2ghz_chan;
	u16 rts_aggr_limit;
	u8 tx_chainmask;
	u8 rx_chainmask;
	u16 tx_triglevel_max;
	u16 reg_cap;
	u8 num_gpio_pins;
	u8 num_antcfg_2ghz;
	u8 num_antcfg_5ghz;
	u8 rx_hp_qdepth;
	u8 rx_lp_qdepth;
	u8 rx_status_len;
	u8 tx_desc_len;
	u8 txs_len;
};


struct jaldi_bus_ops {
	enum jaldi_bus_type type;
	void (*read_cachesize)(struct jaldi_softc *sc, int *cache_size);
	bool (*eeprom_read)(struct jaldi_softc *sc, u32 off, u16 *data);
};

/**
 * struct jaldi_register_ops - Register read/write operations
 *        (formerly ath_ops)
 * @read: Register read
 * @write: Register write
 *
 * The below are not used on the hardware jaldi supports:
 * @enable_write_buffer: Enable multiple register writes
 * @disable_write_buffer: Disable multiple register writes
 * @write_flush: Flush buffered register writes
 */
struct jaldi_register_ops {
	unsigned int (*read)(void *, u32 reg_offset);
	void (*write)(void *, u32 val, u32 reg_offset);
	void (*enable_write_buffer)(void *);
	void (*disable_write_buffer)(void *);
	void (*write_flush) (void *);
};

struct jaldi_hw_ops {
	bool (*macversion_supported)(u32 macversion);

	/* PHY ops */
	int (*rf_set_freq)(struct jaldi_hw *hw,
			   struct jaldi_channel *chan);
	void (*spur_mitigate_freq)(struct jaldi_hw *hw,
				   struct jaldi_channel *chan);
	void (*do_getnf)(struct jaldi_hw *hw, int16_t nfarray[NUM_NF_READINGS]);
	u32 (*compute_pll_control)(struct jaldi_hw *hw,
				   struct jaldi_channel *chan);
	void (*set_rfmode)(struct jaldi_hw *hw, struct jaldi_channel *chan);
	void (*olc_init)(struct jaldi_hw *hw);
	void (*rfbus_done)(struct jaldi_hw *hw);
	bool (*rfbus_req)(struct jaldi_hw *hw);
	void (*set_channel_regs)(struct jaldi_hw *hw, struct jaldi_channel *chan);

	/* MAC ops */ /* TODO: should move more of the hard-coded mac ops into here from hw.c */
	bool (*get_isr)(struct jaldi_hw *hw, enum jaldi_intr_type *masked);
	void (*rx_enable)(struct jaldi_hw *hw);
};

struct jaldi_hw {
	struct jaldi_hw_version hw_version;
	struct jaldi_softc *sc;
	
	struct jaldi_channel *curchan;
	struct jaldi_bitrate *cur_rate;
	enum jaldi_power_mode power_mode;
	enum jaldi_device_state dev_state;
	enum jaldi_opmode opmode;
	struct jaldi_hw_capabilities caps;
	u32 hw_flags; // generic hw flags

	/* Support for killing rf ("airplane mode") */
	u16 rfsilent;
	u32 rfkill_gpio;
	u32 rfkill_polarity;
	bool need_an_top2_fixup;

	union {
		struct ar5416_eeprom_def def;
		struct ar5416_eeprom_4k map4k;
		struct ar9287_eeprom map9287;
	} eeprom;

	/* Used to program the radio on non single-chip devices */
	u32 *analogBank0Data;
	u32 *analogBank1Data;
	u32 *analogBank2Data;
	u32 *analogBank3Data;
	u32 *analogBank6Data;
	u32 *analogBank6TPCData;
	u32 *analogBank7Data;
	u32 *addac5416_21;
	u32 *bank6Temp;

	u32 slottime; /* tx slot duration */
	u32 ifstime; /* interframe spacing time */
	u32 globaltxtimeout;

	u32 intr_txqs;
	u8 txchainmask;
	u8 rxchainmask;

	/* Gain stuff (olc) */
	u32 originalGain[22];
	int initPDADC;
	int PDADCdelta;

	enum jaldi_intr_type imask; /* the interrupts we care about */
	u32 imrs2_reg;
	u32 txok_interrupt_mask;
	u32 txerr_interrupt_mask;
	u32 txdesc_interrupt_mask;
	u32 txeol_interrupt_mask;
	u32 txurn_interrupt_mask;
	u32 intr_gen_timer_trigger;
	u32 intr_gen_timer_thresh;
	bool chip_fullsleep;

	struct jaldi_tx_queue_info txq[JALDI_NUM_TX_QUEUES];

	/* hw config (takes place of ath9k_ops_config) */
	bool rx_intr_mitigation;
	bool tx_intr_mitigation;
	bool is_pciexpress;
	int serialize_regmode;
	u8 ht_enable;
	u8 max_txtrig_level; /* tx fifo */
	u16 tx_trig_level;
	u8 analog_shiftreg;
	int spurmode;
	u16 spurchans[AR_EEPROM_MODAL_SPURS][2];
	bool disable_acks;	/* disable hw generated acks */
	bool disable_cs;	/* disable carrier sense */

	/* functions to control hw */
	struct jaldi_hw_ops ops;
	struct jaldi_register_ops *reg_ops;
	struct jaldi_bus_ops *bus_ops;
	const struct eeprom_ops *eep_ops; // This is more or less copied from ath9k
};

static inline struct jaldi_hw_ops *jaldi_get_hw_ops(struct jaldi_hw *hw) {
	return &hw->ops;
}

int jaldi_hw_init(struct jaldi_hw *hw);
void jaldi_hw_deinit(struct jaldi_hw *hw);
void jaldi_hw_init_global_settings(struct jaldi_hw *hw);

bool jaldi_hw_intrpend(struct jaldi_hw *hw);
enum jaldi_intr_type jaldi_hw_set_interrupts(struct jaldi_hw *hw, 
					enum jaldi_intr_type ints);
bool jaldi_hw_wait(struct jaldi_hw *hw, u32 reg, u32 mask, u32 val, u32 timeout);

void jaldi_hw_attach_phy_ops(struct jaldi_hw *hw);
void jaldi_hw_attach_mac_ops(struct jaldi_hw *hw);
void jaldi_hw_get_channel_centers(struct jaldi_channel *chan, 
				  struct chan_centers *centers);
bool jaldi_hw_setpower(struct jaldi_hw *hw, enum jaldi_power_mode mode);
void jaldi_hw_set11n_txdesc(struct jaldi_hw *hw, void *ds,
				    u32 pktLen, enum jaldi_pkt_type type,
				    u32 txPower, u32 flags);
void jaldi_hw_fill_txdesc(struct jaldi_hw *hw, struct jaldi_desc *ds, u32 seglen,
				  bool is_firstseg, bool is_lastseg,
				  const struct jaldi_desc *ds0, dma_addr_t buf_addr,
				  unsigned int qcu);
u32 jaldi_hw_getrxfilter(struct jaldi_hw *hw);
void jaldi_hw_setrxfilter(struct jaldi_hw *hw, u32 bits);
bool jaldi_hw_stoptxdma(struct jaldi_hw *hw, u32 q);

void jaldi_hw_set_txpowerlimit(struct jaldi_hw *hw, u32 limit);
void jaldi_hw_setmac(struct jaldi_hw *hw, const u8 *mac);
void jaldi_hw_setmcastfilter(struct jaldi_hw *hw, u32 filter0, u32 filter1);
void jaldi_hw_set_opmode(struct jaldi_hw *hw);

#endif
