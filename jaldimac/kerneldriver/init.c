/*
 * jaldimac
 */
 
#include "jaldi.h"

/* We use the hw_value as an index into our private channel structure */

#define CHAN2G(_freq, _idx)  { \
	.channel = (_freq), \
	.center_freq = (_freq), \
	.hw_value = (_idx), \
	.max_power = 20, \
}

#define CHAN5G(_freq, _idx) { \
	.channel = (_freq), \
	.center_freq = (_freq), \
	.hw_value = (_idx), \
	.max_power = 20, \
}

/* From a9k:
 * "Some 5 GHz radios are actually tunable on XXXX-YYYY
 * on 5 MHz steps, we support the channels which we know
 * we have calibration data for all cards though to make
 * this static"
 *
 * Because we (JaldiMAC) are not doing anything with calibration yet,
 * perhaps we can leverage this capability. TODO: does 9280 support this?
 */
static struct jaldi_channel jaldi_2ghz_chantable[] = {};
static struct jaldi_channel jaldi_5ghz_chantable[] = {
	/* _We_ call this UNII 1 */
	CHAN5G(5180, 14), /* Channel 36 */
	CHAN5G(5200, 15), /* Channel 40 */
	CHAN5G(5220, 16), /* Channel 44 */
	CHAN5G(5240, 17), /* Channel 48 */
	/* _We_ call this UNII 2 */
	CHAN5G(5260, 18), /* Channel 52 */
	CHAN5G(5280, 19), /* Channel 56 */
	CHAN5G(5300, 20), /* Channel 60 */
	CHAN5G(5320, 21), /* Channel 64 */
	/* _We_ call this "Middle band" */
	CHAN5G(5500, 22), /* Channel 100 */
	CHAN5G(5520, 23), /* Channel 104 */
	CHAN5G(5540, 24), /* Channel 108 */
	CHAN5G(5560, 25), /* Channel 112 */
	CHAN5G(5580, 26), /* Channel 116 */
	CHAN5G(5600, 27), /* Channel 120 */
	CHAN5G(5620, 28), /* Channel 124 */
	CHAN5G(5640, 29), /* Channel 128 */
	CHAN5G(5660, 30), /* Channel 132 */
	CHAN5G(5680, 31), /* Channel 136 */
	CHAN5G(5700, 32), /* Channel 140 */
	/* _We_ call this UNII 3 */
	CHAN5G(5745, 33), /* Channel 149 */
	CHAN5G(5765, 34), /* Channel 153 */
	CHAN5G(5785, 35), /* Channel 157 */
	CHAN5G(5805, 36), /* Channel 161 */
	CHAN5G(5825, 37), /* Channel 165 */
};

/* List of rates we can select from. 
 * TODO: What is the hardware limit for these rates? 
 */
#define RATE(_bitrate, _hw_rate) {              \
	.bitrate        = (_bitrate),                   \
	.hw_value       = (_hw_rate),                   \
}

static struct jaldi_bitrate jaldi_rates[] = {
	RATE(10, 0x1b),
	RATE(20, 0x1a),
	RATE(55, 0x19),
	RATE(110, 0x18),
	RATE(60, 0x0b),
	RATE(90, 0x0f),
	RATE(120, 0x0a),
	RATE(180, 0x0e),
	RATE(240, 0x09),
	RATE(360, 0x0d),
	RATE(480, 0x08),
	RATE(540, 0x0c),
};

static void jaldi_deinit_softc(struct jaldi_softc *sc);

static void jaldi_iowrite32(struct jaldi_hw *hw, u32 val, u32 reg_offset) {
	struct jaldi_softc *sc = hw->sc;

	if (hw->serialize_regmode == SER_REG_MODE_ON) {
		unsigned long flags;
		spin_lock_irqsave(&sc->sc_serial_rw, flags);
		iowrite32(val, sc->mem + reg_offset);
		spin_unlock_irqrestore(&sc->sc_serial_rw, flags);
	} else {
		iowrite32(val, sc->mem + reg_offset);
	}
}

static unsigned int jaldi_ioread32(struct jaldi_hw *hw, u32 reg_offset) {
	struct jaldi_softc *sc = hw->sc;
	u32 val; 

	if (hw->serialize_regmode == SER_REG_MODE_ON) {
		unsigned long flags;
		spin_lock_irqsave(&sc->sc_serial_rw, flags);
		val = ioread32(sc->mem + reg_offset);
		spin_unlock_irqrestore(&sc->sc_serial_rw, flags);
	} else {
		val = ioread32(sc->mem + reg_offset);
	}

//	jaldi_print(JALDI_DEBUG, "jaldi_ioread32: %8X\n", val);

	return val;
}

static const struct jaldi_register_ops jaldi_reg_ops = {
	.read = jaldi_ioread32,
	.write = jaldi_iowrite32,
};

/* Should set up DMA as well as worker thread to handle setting up queues, etc. */
int jaldi_tx_init(struct jaldi_softc *sc, int nbufs)
{
	int error = 0;

	DBG_START_MSG;	
	spin_lock_init(&sc->tx.txbuflock);

	error = jaldi_descdma_setup(sc, &sc->tx.txdma, &sc->tx.txbuf, 
					"tx", nbufs, 1, 1);

	if (error != 0) {
		jaldi_print(JALDI_ALERT, "Failed to allocate tx descriptors\n");
		jaldi_tx_cleanup(sc);	
		return error;
	}

	/* TODO ath9k has a bugfix here for tx lockups after ~1hr or so, see 
	 * ath_tx_complete_poll_work */

	return 0;
	
}

struct sk_buff *jaldi_rxbuf_alloc(struct jaldi_softc *sc, u32 len)
{
	struct sk_buff *skb;
	u32 off;

	/*
	 * Cache-line-align.  This is important (for the
	 * 5210 at least) as not doing so causes bogus data
	 * in rx'd frames.
	 */

	/* Note: the kernel can allocate a value greater than
	 * what we ask it to give us. We really only need 4 KB as that
	 * is this hardware supports and in fact we need at least 3849
	 * as that is the MAX AMSDU size this hardware supports.
	 * Unfortunately this means we may get 8 KB here from the
	 * kernel... and that is actually what is observed on some
	 * systems :( */
	skb = dev_alloc_skb(len + sc->cachelsz - 1);
	if (skb != NULL) {
		off = ((unsigned long) skb->data) % sc->cachelsz;
		if (off != 0)
			skb_reserve(skb, sc->cachelsz - off);
	} else {
		printk(KERN_ERR "skbuff alloc of size %u failed\n", len);
		return NULL;
	}

	return skb;
}

int jaldi_rx_init(struct jaldi_softc *sc, int nbufs) 
{
	struct sk_buff *skb;
	struct jaldi_buf *bf;
	int error = 0;

	spin_lock_init(&sc->rx.rxflushlock);
	sc->sc_flags &= ~SC_OP_RXFLUSH;
	spin_lock_init(&sc->rx.rxbuflock);
	
	/* we're not doing edma right now; if we were that would go here, like ath9k */

	sc->rx_bufsize = roundup(JALDI_MAX_MPDU_LEN,
			min(sc->cachelsz, (u16)64));

	jaldi_print(JALDI_DEBUG, "cachelsz %u rxbufsize %u\n",
			sc->cachelsz, sc->rx_bufsize);

	/* Initialize rx descriptors */

	error = jaldi_descdma_setup(sc, &sc->rx.rxdma, &sc->rx.rxbuf,
			"rx", nbufs, 1, 0);
	if (error != 0) {
		jaldi_print(JALDI_FATAL,
			  "failed to allocate rx descriptors: %d\n",
			  error);
		goto err;
	}

	list_for_each_entry(bf, &sc->rx.rxbuf, list) {
		skb = jaldi_rxbuf_alloc(sc, sc->rx_bufsize);
				      
		if (skb == NULL) {
			error = -ENOMEM;
			goto err;
		}

		bf->bf_mpdu = skb;
		bf->bf_buf_addr = dma_map_single(sc->dev, skb->data,
				sc->rx_bufsize,
				DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(sc->dev,
						bf->bf_buf_addr))) {
			dev_kfree_skb_any(skb);
			bf->bf_mpdu = NULL;
			jaldi_print(JALDI_FATAL,
				  "dma_mapping_error() on RX init\n");
			error = -ENOMEM;
			goto err;
		}
		bf->bf_dmacontext = bf->bf_buf_addr;
	}
	sc->rx.rxlink = NULL;

err:
	if (error)
		jaldi_rx_cleanup(sc);

	return error;
}




void jaldi_tx_cleanup(struct jaldi_softc *sc)
{
	DBG_START_MSG;	
	if (sc->tx.txdma.dd_desc_len != 0)
		jaldi_descdma_cleanup(sc, &sc->tx.txdma, &sc->tx.txbuf); 

}

void jaldi_rx_cleanup(struct jaldi_softc *sc)
{
	struct sk_buff *skb;
	struct jaldi_buf *bf;

	/* if we were doing edma it'd go here */

	list_for_each_entry(bf, &sc->rx.rxbuf, list) {
		skb = bf->bf_mpdu;
		if (skb) {
			dma_unmap_single(sc->dev, bf->bf_buf_addr,
					sc->rx_bufsize,
					DMA_FROM_DEVICE);
			dev_kfree_skb(skb);
		}
	}

	if (sc->rx.rxdma.dd_desc_len != 0)
		jaldi_descdma_cleanup(sc, &sc->rx.rxdma, &sc->rx.rxbuf);
}
void jaldi_descdma_cleanup(struct jaldi_softc *sc, struct jaldi_descdma *dd,
				struct list_head *head)
{
	DBG_START_MSG;	
	dma_free_coherent(sc->dev, dd->dd_desc_len, dd->dd_desc,
				dd->dd_desc_paddr);

	INIT_LIST_HEAD(head);
	kfree(dd->dd_bufptr);
	memset(dd, 0, sizeof(*dd));
}

/*  From ath9k:
 *  "This function will allocate both the DMA descriptor structure, and the
 *  buffers it contains.  These are used to contain the descriptors used
 *  by the system."
*/
int jaldi_descdma_setup(struct jaldi_softc *sc, struct jaldi_descdma *dd,
		      struct list_head *head, const char *name,
		      int nbuf, int ndesc, bool is_tx)
{
#define	DS2PHYS(_dd, _ds)						\
	((_dd)->dd_desc_paddr + ((caddr_t)(_ds) - (caddr_t)(_dd)->dd_desc))
#define ATH_DESC_4KB_BOUND_CHECK(_daddr) ((((_daddr) & 0xFFF) > 0xF7F) ? 1 : 0)
#define ATH_DESC_4KB_BOUND_NUM_SKIPPED(_len) ((_len) / 4096)
	u8 *ds;
	struct jaldi_buf *bf;
	int i, bsize, error, desc_len;

	DBG_START_MSG;	
	jaldi_print(JALDI_DEBUG, "%s DMA: %u buffers %u desc/buf\n",
		  name, nbuf, ndesc);

	INIT_LIST_HEAD(head);

	if (is_tx)
		desc_len = sc->hw->caps.tx_desc_len;
	else
		desc_len = sizeof(struct jaldi_desc);

	/* ath_desc must be a multiple of DWORDs */
	if ((desc_len % 4) != 0) {
		jaldi_print(JALDI_FATAL,
			  "jaldi_desc not DWORD aligned\n");
		BUG_ON((desc_len % 4) != 0);
		error = -ENOMEM;
		goto fail;
	}

	dd->dd_desc_len = desc_len * nbuf * ndesc;

	/*
	 * Need additional DMA memory because we can't use
	 * descriptors that cross the 4K page boundary. Assume
	 * one skipped descriptor per 4K page.
	 */
	if (!(sc->hw->caps.hw_caps & JALDI_HW_CAP_4KB_SPLITTRANS)) {
		u32 ndesc_skipped =
			ATH_DESC_4KB_BOUND_NUM_SKIPPED(dd->dd_desc_len);
		u32 dma_len;

		while (ndesc_skipped) {
			dma_len = ndesc_skipped * desc_len;
			dd->dd_desc_len += dma_len;

			ndesc_skipped = ATH_DESC_4KB_BOUND_NUM_SKIPPED(dma_len);
		}
	}

	/* allocate descriptors */
	dd->dd_desc = dma_alloc_coherent(sc->dev, dd->dd_desc_len,
					 &dd->dd_desc_paddr, GFP_KERNEL);
	if (dd->dd_desc == NULL) {
		error = -ENOMEM;
		goto fail;
	}
	ds = (u8 *) dd->dd_desc;
	jaldi_print(JALDI_INFO, "%s DMA map: %p (%u) -> %llx (%u)\n",
		  name, ds, (u32) dd->dd_desc_len,
		  ito64(dd->dd_desc_paddr), /*XXX*/(u32) dd->dd_desc_len);

	/* allocate buffers */
	bsize = sizeof(struct jaldi_buf) * nbuf;
	bf = kzalloc(bsize, GFP_KERNEL);
	if (bf == NULL) {
		error = -ENOMEM;
		goto fail2;
	}
	dd->dd_bufptr = bf;

	for (i = 0; i < nbuf; i++, bf++, ds += (desc_len * ndesc)) {
		bf->bf_desc = ds;
		bf->bf_daddr = DS2PHYS(dd, ds);

		if (!(sc->hw->caps.hw_caps &
		      JALDI_HW_CAP_4KB_SPLITTRANS)) {
			/*
			 * Skip descriptor addresses which can cause 4KB
			 * boundary crossing (addr + length) with a 32 dword
			 * descriptor fetch.
			 */
			while (ATH_DESC_4KB_BOUND_CHECK(bf->bf_daddr)) {
				BUG_ON((caddr_t) bf->bf_desc >=
				       ((caddr_t) dd->dd_desc +
					dd->dd_desc_len));

				ds += (desc_len * ndesc);
				bf->bf_desc = ds;
				bf->bf_daddr = DS2PHYS(dd, ds);
			}
		}
		list_add_tail(&bf->list, head);
	}
	return 0;
fail2:
	dma_free_coherent(sc->dev, dd->dd_desc_len, dd->dd_desc,
			  dd->dd_desc_paddr);
fail:
	memset(dd, 0, sizeof(*dd));
	return error;
#undef ATH_DESC_4KB_BOUND_CHECK
#undef ATH_DESC_4KB_BOUND_NUM_SKIPPED
#undef DS2PHYS
}

static int jaldi_init_queues(struct jaldi_softc *sc)
{
	int i = 0;

	DBG_START_MSG;	
	for (i = 0; i < ARRAY_SIZE(sc->tx.hwq_map); i++)
		sc->tx.hwq_map[i] = -1;

	
	if (!jaldi_tx_setup(sc, JALDI_WME_AC_BK)) { 
		jaldi_print(JALDI_FATAL,
			  "Unable to setup xmit queue for BK traffic\n");
		goto err;
	}

	if (!jaldi_tx_setup(sc, JALDI_WME_AC_BE)) {
		jaldi_print(JALDI_FATAL,
			  "Unable to setup xmit queue for BE traffic\n");
		goto err;
	}
	if (!jaldi_tx_setup(sc, JALDI_WME_AC_VI)) {
		jaldi_print(JALDI_FATAL,
			  "Unable to setup xmit queue for VI traffic\n");
		goto err;
	}
	if (!jaldi_tx_setup(sc, JALDI_WME_AC_VO)) {
		jaldi_print(JALDI_FATAL,
			  "Unable to setup xmit queue for VO traffic\n");
		goto err;
	}

	return 0;
err:
	for (i = 0; i < JALDI_NUM_TX_QUEUES; i++)
		if (JALDI_TXQ_SETUP(sc, i))
			jaldi_tx_cleanupq(sc, &sc->tx.txq[i]);

	return -EIO;
}

int jaldi_init_softc(u16 devid, struct jaldi_softc *sc, u16 subsysid, const struct jaldi_bus_ops *bus_ops)
{
	struct jaldi_hw *hw = NULL;
	struct ath9k_platform_data *pdata;
	int ret = 0;
	int csz = 0;

	DBG_START_MSG;	
	hw = kzalloc(sizeof(struct jaldi_hw), GFP_KERNEL);
	if (!hw) return -ENOMEM;

	hw->hw_version.devid = devid;
	hw->hw_version.subsysid = subsysid;

	pdata = (struct ath9k_platform_data *) sc->dev->platform_data;
	if (!pdata) {
		jaldi_print(JALDI_DEBUG, "no pdev\n");
		hw->hw_flags |= AH_USE_EEPROM;
	}

	hw->sc = sc;
	sc->hw = hw;
	
	hw->reg_ops = &jaldi_reg_ops;
	hw->bus_ops = bus_ops;

	spin_lock_init(&sc->sc_resetlock);
	spin_lock_init(&sc->sc_netdevlock);
	spin_lock_init(&sc->sc_pm_lock);
	spin_lock_init(&sc->sc_serial_rw);
	mutex_init(&sc->mutex);
	tasklet_init(&sc->intr_tq, jaldi_tasklet, (unsigned long)sc);
	/* init tasklets and other locks here */

	hrtimer_init(&sc->tx_timer,CLOCK_REALTIME, HRTIMER_MODE_ABS);

	hw->bus_ops->read_cachesize(sc, &csz); 
	sc->cachelsz = csz;

	/* ath9k reads cache line size here... may be relevant */
	ret = jaldi_hw_init(hw);
	if (ret) goto err_hw;

	ret = jaldi_init_debug(hw);
	if (ret) {
		jaldi_print(JALDI_WARN, "Couldn't create debubfs\n");
		goto err_debug;
	}

	sc->chans[JALDI_2GHZ] = jaldi_2ghz_chantable;
	sc->chans[JALDI_5GHZ] = jaldi_5ghz_chantable;

	ret = jaldi_init_queues(sc);
	if (ret) goto err_queues;

	return 0;
	
err_queues:
	jaldi_hw_deinit(hw);
err_debug:
	jaldi_exit_debug(hw);
err_hw:
	tasklet_kill(&sc->intr_tq);
	kfree(hw);
	sc->hw = NULL;
	jaldi_print(JALDI_FATAL,"init_device failed, ret=%d\n",ret);
	return ret;
}

int jaldi_init_device(u16 devid, struct jaldi_softc *sc, u16 subsysid, const struct jaldi_bus_ops *bus_ops)
{
	int error;

	DBG_START_MSG;	
	error = jaldi_init_softc(devid, sc, subsysid, bus_ops);
	if (error != 0)
		goto error_init;


	/* Setup TX DMA */
	error = jaldi_tx_init(sc, JALDI_NUM_TXBUF);
	if (error) { goto error_tx; }

	/* Setup RX DMA */
	error = jaldi_rx_init(sc, JALDI_NUM_RXBUF); // TODO
	if (error) { goto error_rx; }

	/* initialize workers here if needed */

	return 0;

error_rx:
	jaldi_tx_cleanup(sc);
error_tx:
	jaldi_deinit_softc(sc);
error_init:
	jaldi_print(JALDI_FATAL, "init_device failed, error %d.\n",error);
	return error;
}

/* TODO
 * this is probably similar to ath9k_init_interrupt_masks in ath9k's hw.c
 */
int jaldi_init_interrupts(struct jaldi_softc *sc)
{
	DBG_START_MSG;	
	return 0;
}

/*****************************/
/*     De-Initialization     */
/*****************************/

static void jaldi_deinit_softc(struct jaldi_softc *sc)
{
	DBG_START_MSG;	
	int i = 0;

	for (i = 0; i < JALDI_NUM_TX_QUEUES; i++)
		if (JALDI_TXQ_SETUP(sc, i))
			jaldi_tx_cleanupq(sc, &sc->tx.txq[i]);

	jaldi_hw_deinit(sc->hw);

	tasklet_kill(&sc->intr_tq);

	kfree(sc->hw);
	sc->hw = NULL;
}

void jaldi_deinit_device(struct jaldi_softc *sc)
{
	DBG_START_MSG;	
	jaldi_ps_wakeup(sc); 

	jaldi_rx_cleanup(sc);  
	jaldi_tx_cleanup(sc); 
	jaldi_deinit_softc(sc);
}

/* deinit of descdma could go here */


/*******************/
/*     net_dev     */
/*******************/
/* This is our alloc_netdev callback. It just sets up the softc memory 
 * region in the netdev. We actually perform the rest of our device 
 * initialization through our bus probe calls, which start in 
 * jaldi_init_device. We call alloc_netdev during bus probe as well, but 
 * we call init_device. */
void jaldi_init(struct net_device *dev)
{
	struct jaldi_softc *sc;

	DBG_START_MSG;

	ether_setup(dev);

	jaldi_attach_netdev_ops(dev);
	dev->flags = IFF_NOARP;
	dev->features = NETIF_F_NO_CSUM;

	sc = netdev_priv(dev);

	memset(sc,0,sizeof(struct jaldi_softc));

	sc->net_dev = dev;

	jaldi_print(JALDI_DEBUG, "init sc: %p\n", sc);
	
	if(jaldi_init_interrupts(sc)) {
		jaldi_print(JALDI_FATAL, "error initializing interrupt handlers\n");
		return; 
	}

	jaldi_print(JALDI_INFO, "jaldi_init end\n");
}

/* Allocates and registers network device
 * Also allocates memory for jaldi_softc 
 */
struct net_device *jaldi_init_netdev(void)
{
	struct net_device *jaldi_dev;

	DBG_START_MSG;
	jaldi_dev = alloc_netdev(sizeof(struct jaldi_softc), "jaldi%d", jaldi_init);
	if (jaldi_dev == NULL) {
		jaldi_print(JALDI_FATAL, "net_dev is null\n");
		return -ENOMEM;
	}

	jaldi_print(JALDI_INFO, "netdev allocated.\n");

	return jaldi_dev;
}

int jaldi_start_netdev(struct jaldi_softc *sc)
{
	int result;
	struct net_device *ndev;

	DBG_START_MSG;
	ndev = sc->net_dev;

	result = register_netdev(ndev);
	if (result) {
		jaldi_print(JALDI_FATAL, "error %i registering device \"%s\"\n", result, ndev->name);
		return -ENOMEM;
	}

	jaldi_print(JALDI_INFO, "netdev registered.\n");	
	
	return 0;
}
