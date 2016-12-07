/* 
 * jaldi 
 */
 
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "jaldi.h"

MODULE_AUTHOR("Shaddi Hasan");
MODULE_LICENSE("Dual BSD/GPL");

bool jaldi_setpower(struct jaldi_softc *sc, enum jaldi_power_mode mode)
{
	unsigned long flags;
	bool result;

	spin_lock_irqsave(&sc->sc_pm_lock, flags);
	result = jaldi_hw_setpower(sc->hw, mode);
	spin_unlock_irqrestore(&sc->sc_pm_lock, flags);
	return result;
}

void jaldi_ps_wakeup(struct jaldi_softc *sc)
{
	unsigned long flags;
	spin_lock_irqsave(&sc->sc_pm_lock, flags);
	if (++sc->ps_usecount != 1)
		goto unlock;

	jaldi_hw_setpower(sc->hw, JALDI_PM_AWAKE);

 unlock:
	spin_unlock_irqrestore(&sc->sc_pm_lock, flags);
}

void jaldi_ps_restore(struct jaldi_softc *sc)
{
	unsigned long flags;

	spin_lock_irqsave(&sc->sc_pm_lock, flags);
	if (--sc->ps_usecount != 0)
		goto unlock;

	if (sc->ps_idle)
		jaldi_hw_setpower(sc->hw, JALDI_PM_FULL_SLEEP);

unlock:
	spin_unlock_irqrestore(&sc->sc_pm_lock, flags);
}


/* Maintains a priority queue of jaldi_packets to be sent */
void jaldi_tx_enqueue(struct jaldi_softc *sc, struct jaldi_packet *pkt) {
	unsigned long flags;
	
//	spin_lock_irqsave(&sc->sc_netdevlock, flags);
	if(sc->tx_queue == NULL || pkt->tx_time < sc->tx_queue->tx_time){
		jaldi_print(JALDI_DEBUG, "adding to front\n");
		/* New packet goes in front */
		pkt->next = sc->tx_queue;
		sc->tx_queue = pkt;
	} else {
		jaldi_print(JALDI_DEBUG, "adding to back\n");
		struct jaldi_packet *curr = sc->tx_queue;
		while(curr->next != NULL && pkt->tx_time < curr->next->tx_time){	
			curr = curr->next;
		}
		pkt->next = curr->next;
		curr->next = pkt;
	}
//	spin_unlock_irqrestore(&sc->sc_netdevlock, flags);
}

/* Returns the next jaldi_packet to be transmitted */
struct jaldi_packet *jaldi_tx_dequeue(struct jaldi_softc *sc) {
	struct jaldi_packet *pkt;
	unsigned long flags;
	
//	spin_lock_irqsave(&sc->sc_netdevlock, flags);
	pkt = sc->tx_queue;
	if (pkt != NULL) sc->tx_queue = pkt->next;
//	spin_unlock_irqrestore(&sc->sc_netdevlock, flags);
	return pkt;
}


irqreturn_t jaldi_isr(int irq, void *dev)
{
#define SCHED_INTR (				\
		JALDI_INT_FATAL |		\
		JALDI_INT_RXORN |		\
		JALDI_INT_RXEOL |		\
		JALDI_INT_RX |			\
		JALDI_INT_RXLP |		\
		JALDI_INT_RXHP |		\
		JALDI_INT_TX |			\
		JALDI_INT_BMISS |		\
		JALDI_INT_CST |			\
		JALDI_INT_TSFOOR |		\
		JALDI_INT_GENTIMER)

	enum jaldi_intr_type status;
	struct jaldi_softc *sc = dev;
	struct jaldi_hw *hw = sc->hw;
	bool sched;

	DBG_START_MSG;

	if (hw->dev_state != JALDI_HW_INITIALIZED) { return IRQ_NONE; }
	
	/* shared irq, not for us */
	if(!jaldi_hw_intrpend(hw)) { return IRQ_NONE; }

	OHAI;

	/*
	 * Figure out the reason(s) for the interrupt.  Note
	 * that the hal returns a pseudo-ISR that may include
	 * bits we haven't explicitly enabled so we mask the
	 * value to insure we only process bits we requested.
	 */
	hw->ops.get_isr(hw, &status);	/* NB: clears ISR too */
	status &= hw->imask;	/* discard unasked-for bits */

	jaldi_print(JALDI_DEBUG, "intr status 0x%.8x, imask 0x%x\n", status, hw->imask);

	/*
	 * If there are no status bits set, then this interrupt was not
	 * for me (should have been caught above).
	 */
	if (!status)
		return IRQ_NONE;

	/* Cache the status */
	sc->intrstatus = status;

	if (status & SCHED_INTR)
		sched = true;
	
	/*
	 * If a FATAL or RXORN interrupt is received, we have to reset the
	 * chip immediately.
	 */
	if ((status & JALDI_INT_FATAL) || ((status & JALDI_INT_RXORN) &&
	    !(hw->caps.hw_caps & JALDI_HW_CAP_EDMA)))
		goto chip_reset;

//	if (status & JALDI_INT_SWBA)
//		tasklet_schedule(&sc->bcon_tasklet);

	OHAI;

	if (status & JALDI_INT_TXURN)
		jaldi_hw_updatetxtriglevel(hw, true);

	if (hw->caps.hw_caps & JALDI_HW_CAP_EDMA) {
		OHAI;
		if (status & JALDI_INT_RXEOL) {
			OHAI;
			hw->imask &= ~(JALDI_INT_RXEOL | JALDI_INT_RXORN);
			jaldi_hw_set_interrupts(hw, hw->imask);
		}
	}

	if (status & JALDI_INT_MIB) {
		/* we should be ignoring this for now */
		jaldi_print(JALDI_WARN, "MIB intr, ignoring.\n");
	}

	if (!(hw->caps.hw_caps & JALDI_HW_CAP_AUTOSLEEP))
		if (status & JALDI_INT_TIM_TIMER) {
			/* Clear RxAbort bit so that we can
			 * receive frames */
			jaldi_setpower(sc, JALDI_PM_AWAKE);
			jaldi_hw_setrxabort(hw, 0);
		}

chip_reset:

//	ath_debug_stat_interrupt(sc, status);

	if (sched) {
		/* turn off every interrupt except SWBA */
		jaldi_hw_set_interrupts(hw, (hw->imask & JALDI_INT_SWBA));
	
		/* Here, ath9k schedules the ath tasklet which in turn will call
		 * the proper rx tasklet to handle rx. We need to determine if we can 
		 * tolerate the latency that this incurs: ideally we can postpone handling
		 * of rx, but it may be necessary to handle at least latency-sensitive 
		 * packets during the interrupt handler.
		 * 
		 * EDIT on the above: still need to look into this, but the rx 
		 * processing needs locks, which can't be done in interrupt 
		 * context. So we need to find some way around this.
		 * 
		 */

		 tasklet_schedule(&sc->intr_tq);
	}

	DBG_END_MSG;

	return IRQ_HANDLED;

#undef SCHED_INTR
}

	

int jaldi_release(struct net_device *dev)
{
	netif_stop_queue(dev);
	return 0;
}

/******/
/* RX */
/******/
/*
 * Calculate the receive filter according to the
 * operating mode and state:
 * o Just grab everything: we want it all. Handle processing at higher level.
 * o Always maintain current state of phy error reception (the hal
 *   may enable phy error frames for (future) noise immunity work)
 */

u32 jaldi_calcrxfilter(struct jaldi_softc *sc)
{
#define	RX_FILTER_PRESERVE (JALDI_RX_FILTER_PHYERR | JALDI_RX_FILTER_PHYRADAR)

	u32 rfilt;

	rfilt = (jaldi_hw_getrxfilter(sc->hw) & RX_FILTER_PRESERVE)
		| JALDI_RX_FILTER_UCAST | JALDI_RX_FILTER_BCAST
		| JALDI_RX_FILTER_MCAST | JALDI_RX_FILTER_CONTROL
		| JALDI_RX_FILTER_BEACON | JALDI_RX_FILTER_PROM
		| JALDI_RX_FILTER_PROBEREQ | JALDI_RX_FILTER_MYBEACON 
		| JALDI_RX_FILTER_COMP_BAR | JALDI_RX_FILTER_PSPOLL
		| JALDI_RX_FILTER_MCAST_BCAST_ALL;

	return rfilt;

#undef RX_FILTER_PRESERVE
}

void jaldi_opmode_init(struct jaldi_softc *sc)
{
	struct jaldi_hw *hw = sc->hw;
	u32 rfilt, mfilt[2];

	printk(KERN_DEBUG "jaldi: jaldi_opmode_init\n");

	/* configure rx filter */
	rfilt = jaldi_calcrxfilter(sc);
	jaldi_hw_setrxfilter(hw, rfilt);

	/* configure operational mode */
	jaldi_hw_set_opmode(hw);

	/* Handle any link-level address change. */
	jaldi_hw_setmac(hw, sc->macaddr);

	/* calculate and install multicast filter */
	mfilt[0] = mfilt[1] = ~0;
	jaldi_hw_setmcastfilter(hw, mfilt[0], mfilt[1]);
}

static void jaldi_rx_buf_link(struct jaldi_softc *sc, struct jaldi_buf *bf)
{
	struct jaldi_hw *hw = sc->hw;
	struct jaldi_desc *ds;
	struct sk_buff *skb;

	JALDI_RXBUF_RESET(bf);

	ds = bf->bf_desc;
	ds->ds_link = 0; /* link to null */
	ds->ds_data = bf->bf_buf_addr;

	/* virtual addr of the beginning of the buffer. */
	skb = bf->bf_mpdu;
	BUG_ON(skb == NULL);
	ds->ds_vdata = skb->data;

	/*
	 * setup rx descriptors. The rx_bufsize here tells the hardware
	 * how much data it can DMA to us and that we are prepared
	 * to process
	 */
	jaldi_hw_setuprxdesc(hw, ds,
			     sc->rx_bufsize,
			     0);

	if (sc->rx.rxlink == NULL)
		jaldi_hw_putrxbuf(hw, bf->bf_daddr);
	else
		*sc->rx.rxlink = bf->bf_daddr;

	sc->rx.rxlink = &ds->ds_link;
	hw->ops.rx_enable(hw);
}

int jaldi_startrecv(struct jaldi_softc *sc)
{
	struct jaldi_hw *hw = sc->hw;
	struct jaldi_buf *bf, *tbf;

	DBG_START_MSG;

	spin_lock_bh(&sc->rx.rxbuflock);
	if (list_empty(&sc->rx.rxbuf))
		goto start_recv;

	sc->rx.rxlink = NULL;
	list_for_each_entry_safe(bf, tbf, &sc->rx.rxbuf, list) {
		jaldi_rx_buf_link(sc, bf);
	}

	/* We could have deleted elements so the list may be empty now */
	if (list_empty(&sc->rx.rxbuf))
		goto start_recv;

	bf = list_first_entry(&sc->rx.rxbuf, struct jaldi_buf, list);
	jaldi_hw_putrxbuf(hw, bf->bf_daddr);
	hw->ops.rx_enable(hw);

start_recv:
	spin_unlock_bh(&sc->rx.rxbuflock);
	jaldi_opmode_init(sc);
	jaldi_hw_startpcureceive(hw);

	DBG_END_MSG;

	return 0;
}

bool jaldi_stoprecv(struct jaldi_softc *sc)
{
	struct jaldi_hw *hw = sc->hw;
	bool stopped;

	jaldi_hw_stoppcurecv(hw);
	jaldi_hw_setrxfilter(hw, 0);
	stopped = jaldi_hw_stopdmarecv(hw);

	sc->rx.rxlink = NULL;

	return stopped;
}

void jaldi_flushrecv(struct jaldi_softc *sc)
{
	spin_lock_bh(&sc->rx.rxflushlock);
	sc->sc_flags |= SC_OP_RXFLUSH;
	jaldi_rx_tasklet(sc, 1); 
	sc->sc_flags &= ~SC_OP_RXFLUSH;
	spin_unlock_bh(&sc->rx.rxflushlock);
}

static struct jaldi_buf *jaldi_get_next_rx_buf(struct jaldi_softc *sc,
						struct jaldi_rx_status *rs)
{
	struct jaldi_hw *hw = sc->hw;
	struct jaldi_desc *ds;
	struct jaldi_buf *bf;
	int ret;

	if (list_empty(&sc->rx.rxbuf)) {
		sc->rx.rxlink = NULL;
		return NULL;
	}

	bf = list_first_entry(&sc->rx.rxbuf, struct jaldi_buf, list);
	ds = bf->bf_desc;

	/*
	 * Must provide the virtual address of the current
	 * descriptor, the physical address, and the virtual
	 * address of the next descriptor in the h/w chain.
	 * This allows the HAL to look ahead to see if the
	 * hardware is done with a descriptor by checking the
	 * done bit in the following descriptor and the address
	 * of the current descriptor the DMA engine is working
	 * on.  All this is necessary because of our use of
	 * a self-linked list to avoid rx overruns.
	 */
	ret = jaldi_hw_rxprocdesc(hw, ds, rs, 0);
	if (ret == -EINPROGRESS) {
		struct jaldi_rx_status trs;
		struct jaldi_buf *tbf;
		struct jaldi_desc *tds;

		memset(&trs, 0, sizeof(trs));
		if (list_is_last(&bf->list, &sc->rx.rxbuf)) {
			sc->rx.rxlink = NULL;
		}

		tbf = list_entry(bf->list.next, struct jaldi_buf, list);

		/*
		 * On some hardware the descriptor status words could
		 * get corrupted, including the done bit. Because of
		 * this, check if the next descriptor's done bit is
		 * set or not.
		 *
		 * If the next descriptor's done bit is set, the current
		 * descriptor has been corrupted. Force s/w to discard
		 * this descriptor and continue...
		 */

		tds = tbf->bf_desc;
		ret = jaldi_hw_rxprocdesc(hw, tds, &trs, 0);
		if (ret == -EINPROGRESS)
			return NULL;
	}

	if (!bf->bf_mpdu)
		return bf;

	/*
	 * Synchronize the DMA transfer with CPU before
	 * 1. accessing the frame
	 * 2. requeueing the same buffer to h/w
	 */
	dma_sync_single_for_cpu(sc->dev, bf->bf_buf_addr,
			sc->rx_bufsize,
			DMA_FROM_DEVICE);

	return bf;
}


int jaldi_rx_tasklet(struct jaldi_softc *sc, int flush)
{
	struct sk_buff *skb, *requeue_skb;
	struct jaldi_buf *bf;
	struct jaldi_hw *hw = sc->hw;
	struct jaldi_rx_status rs;
	DBG_START_MSG;
	spin_lock_bh(&sc->rx.rxbuflock);

	do {
		OHAI;
		/* If handling rx interrupt and flush is in progress => exit */
		if ((sc->sc_flags & SC_OP_RXFLUSH) && (flush == 0))
			break;

		memset(&rs, 0, sizeof(rs));
		bf = jaldi_get_next_rx_buf(sc, &rs);

		if (!bf)
			break;

		skb = bf->bf_mpdu;
		if (!skb)
			continue;
		OHAI;
		//ath_debug_stat_rx(sc, &rs); // TODO

		/*
		 * If we're asked to flush receive queue, directly
		 * chain it back at the queue without processing it.
		 */
		if (flush)
			goto requeue;

		/* TODO This is where we should check to see if we want to 
		 * accept the packet (since we don't do checking, we accept 
		 * everything: CRC failures, etc.). We should also put the 
		 * relevant rx status information into the packet payload. 
		 * See ath9k_rx_skb_preprocess and ath9k_rx_accept. */


		/* Ensure we always have an skb to requeue once we are done
		 * processing the current buffer's skb */
		requeue_skb = jaldi_rxbuf_alloc(sc, sc->rx_bufsize);

		/* If there is no memory we ignore the current RX'd frame,
		 * tell hardware it can give us a new frame using the old
		 * skb and put it at the tail of the sc->rx.rxbuf list for
		 * processing. */
		if (!requeue_skb)
			goto requeue;

		/* Unmap the frame */
		dma_unmap_single(sc->dev, bf->bf_buf_addr,
				 sc->rx_bufsize,
				 DMA_FROM_DEVICE);

		/* We will now give hardware our shiny new allocated skb */
		bf->bf_mpdu = requeue_skb;
		bf->bf_buf_addr = dma_map_single(sc->dev, requeue_skb->data,
						 sc->rx_bufsize,
						 DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(sc->dev,
			  bf->bf_buf_addr))) {
			dev_kfree_skb_any(requeue_skb);
			bf->bf_mpdu = NULL;
			jaldi_print(JALDI_FATAL,
				  "dma_mapping_error() on RX\n");
			netif_rx(skb);
			break;
		}
		bf->bf_dmacontext = bf->bf_buf_addr;

		/*
		 * change the default rx antenna if rx diversity chooses the
		 * other antenna 3 times in a row.
		 */
		if (sc->rx.defant != rs.rs_antenna) {
			if (++sc->rx.rxotherant >= 3)
				jaldi_print(JALDI_INFO, "antenna %d -> %d\n",
					sc->rx.defant, rs.rs_antenna);
				//ath_setdefantenna(sc, rs.rs_antenna); // TODO
		} else {
			sc->rx.rxotherant = 0;
		}

		skb->dev = sc->net_dev;
		skb->protocol = eth_type_trans(skb, sc->net_dev);
		skb->ip_summed = CHECKSUM_NONE;
		sc->stats.rx_packets++;
		sc->stats.rx_bytes += skb->data_len;
		netif_rx(skb);

requeue:
		list_move_tail(&bf->list, &sc->rx.rxbuf);
		jaldi_rx_buf_link(sc, bf);
	} while (1);

	spin_unlock_bh(&sc->rx.rxbuflock);

	return 0;
}

/******/
/* TX */
/******/
struct jaldi_txq *jaldi_txq_setup(struct jaldi_softc *sc, int qtype, int subtype)
{
	struct jaldi_hw *hw = sc->hw;
	struct jaldi_tx_queue_info qi;
	int qnum, i;

	memset(&qi, 0, sizeof(qi));
	qi.tqi_subtype = subtype;
	qi.tqi_aifs = JALDI_TXQ_USEDEFAULT;
	qi.tqi_cwmin = JALDI_TXQ_USEDEFAULT;
	qi.tqi_cwmax = JALDI_TXQ_USEDEFAULT;
	qi.tqi_physCompBuf = 0;

	/* From ath9k:
	 * "Enable interrupts only for EOL and DESC conditions.
	 * We mark tx descriptors to receive a DESC interrupt
	 * when a tx queue gets deep; otherwise waiting for the
	 * EOL to reap descriptors.  Note that this is done to
	 * reduce interrupt load and this only defers reaping
	 * descriptors, never transmitting frames.  Aside from
	 * reducing interrupts this also permits more concurrency.
	 * The only potential downside is if the tx queue backs
	 * up in which case the top half of the kernel may backup
	 * due to a lack of tx descriptors.
	 *
	 * The UAPSD queue is an exception, since we take a desc-
	 * based intr on the EOSP frames." (note, we don't have uapsd)
	 */
	if (hw->caps.hw_caps & JALDI_HW_CAP_EDMA) {
		qi.tqi_qflags = TXQ_FLAG_TXOKINT_ENABLE |
				TXQ_FLAG_TXERRINT_ENABLE;
	} else {
		qi.tqi_qflags = TXQ_FLAG_TXEOLINT_ENABLE |
				TXQ_FLAG_TXDESCINT_ENABLE;
	}
	qnum = jaldi_hw_setuptxqueue(hw, qtype, &qi);
	if (qnum == -1) {
		/*
		 * NB: don't print a message, this happens
		 * normally on parts with too few tx queues
		 */
		jaldi_print(JALDI_DEBUG, "txq setup failed.\n"); /* just for debugging... */
		return NULL;
	}
	if (qnum >= ARRAY_SIZE(sc->tx.txq)) {
		jaldi_print(JALDI_FATAL,
			  "qnum %u out of range, max %u!\n",
			  qnum, (unsigned int)ARRAY_SIZE(sc->tx.txq));
		jaldi_hw_releasetxqueue(hw, qnum);
		return NULL;
	}
	if (!JALDI_TXQ_SETUP(sc, qnum)) {
		struct jaldi_txq *txq = &sc->tx.txq[qnum];

		txq->axq_qnum = qnum;
		txq->axq_link = NULL;
		INIT_LIST_HEAD(&txq->axq_q);
		INIT_LIST_HEAD(&txq->axq_acq);
		spin_lock_init(&txq->axq_lock);
		txq->axq_depth = 0;
		txq->axq_tx_inprogress = false;
		sc->tx.txqsetup |= 1<<qnum;

		txq->txq_headidx = txq->txq_tailidx = 0;
		for (i = 0; i < JALDI_TXFIFO_DEPTH; i++)
			INIT_LIST_HEAD(&txq->txq_fifo[i]);
		INIT_LIST_HEAD(&txq->txq_fifo_pending);
	}
	return &sc->tx.txq[qnum];
}

int jaldi_tx_setup(struct jaldi_softc *sc, int haltype)
{
	struct jaldi_txq *txq;

	DBG_START_MSG;

	if (haltype >= ARRAY_SIZE(sc->tx.hwq_map)) {
		jaldi_print(JALDI_FATAL,
			  "HAL AC %u out of range, max %zu!\n",
			 haltype, ARRAY_SIZE(sc->tx.hwq_map));
		return 0;
	}
	txq = jaldi_txq_setup(sc, JALDI_TX_QUEUE_DATA, haltype);
	if (txq != NULL) {
		sc->tx.hwq_map[haltype] = txq->axq_qnum;
		return 1;
	} else
		return 0;
}

void jaldi_tx_cleanupq(struct jaldi_softc *sc, struct jaldi_txq *txq)
{
	jaldi_hw_releasetxqueue(sc->hw, txq->axq_qnum);
	sc->tx.txqsetup &= ~(1<<txq->axq_qnum);
}

static void jaldi_tx_complete_buf(struct jaldi_softc *sc, struct jaldi_buf *bf,
				struct jaldi_txq *txq, struct list_head *bf_q,
				int txok)
{
	struct sk_buff *skb = bf->bf_mpdu;
	unsigned long flags;
	int tx_flags = 0;

	if (!txok) {
		tx_flags |= JALDI_TX_ERROR;
	}

	dma_unmap_single(sc->dev, bf->bf_dmacontext, skb->len, DMA_TO_DEVICE);

	/* We'll need to implement this if we start tracking how many pending frames
	 * we have in our txq's. Note that compat-wireless patches this. */
//	jaldi_tx_complete(sc, skb, tx_flags);
//	ath_debug_stat_tx(sc, txq, bf, ts); // TODO

	/*
	 * Return the list of ath_buf of this mpdu to free queue
	 */
	spin_lock_irqsave(&sc->tx.txbuflock, flags);
	list_splice_tail_init(bf_q, &sc->tx.txbuf);
	spin_unlock_irqrestore(&sc->tx.txbuflock, flags);
}

static int jaldi_tx_setup_buffer(struct jaldi_softc *sc, struct jaldi_buf *bf,
					struct jaldi_packet *pkt)
{
	DBG_START_MSG;

	JALDI_TXBUF_RESET(bf);

	jaldi_print(JALDI_DEBUG, "len: %d\n", pkt->skb->len);
	bf->bf_frmlen = pkt->skb->len; /* TODO: should we add space for fcs? */

	bf->bf_mpdu = pkt->skb;

	jaldi_print(JALDI_DEBUG, "dev: %p data: %p len: %d\n", sc->dev, pkt->skb->data, pkt->skb->len);
	bf->bf_dmacontext = dma_map_single(sc->dev, pkt->skb->data, 
				pkt->skb->len, DMA_TO_DEVICE);

	if(unlikely(dma_mapping_error(sc->dev, bf->bf_dmacontext))) {
		bf->bf_mpdu = NULL;
		jaldi_print(JALDI_FATAL, "dma_mapping_error() on TX\n");
		return -ENOMEM;
	}

	bf->bf_buf_addr = bf->bf_dmacontext;

	bf->bf_tx_aborted = false;
	DBG_END_MSG;
	return 0;
}

static struct jaldi_buf *jaldi_tx_get_buffer(struct jaldi_softc *sc)
{
	struct jaldi_buf *bf = NULL;

	DBG_START_MSG;

	spin_lock_bh(&sc->tx.txbuflock);
	if (unlikely(list_empty(&sc->tx.txbuf))) {
		spin_unlock_bh(&sc->tx.txbuflock);
		jaldi_print(JALDI_DEBUG, "txbuf list empty\n");
		return NULL;
	}
	bf = list_first_entry(&sc->tx.txbuf, struct jaldi_buf, list);
	list_del(&bf->list);

	spin_unlock_bh(&sc->tx.txbuflock);

	DBG_END_MSG;

	return bf;
}

static void jaldi_tx_return_buffer(struct jaldi_softc *sc, struct jaldi_buf *bf)
{
	spin_lock_bh(&sc->tx.txbuflock);
	list_add_tail(&bf->list, &sc->tx.txbuf);
	spin_unlock_bh(&sc->tx.txbuflock);
}

/*
 * Drain a given TX queue
 *
 * This assumes output has been stopped and
 * we do not need to block ath_tx_tasklet.
 */
void jaldi_draintxq(struct jaldi_softc *sc, struct jaldi_txq *txq, bool retry_tx)
{
	struct jaldi_buf *bf, *lastbf;
	struct list_head bf_head;
	struct jaldi_tx_status ts;

	memset(&ts, 0, sizeof(ts));
	INIT_LIST_HEAD(&bf_head);

	for (;;) {
		spin_lock_bh(&txq->axq_lock);

		if (list_empty(&txq->axq_q)) {
			txq->axq_link = NULL;
			spin_unlock_bh(&txq->axq_lock);
			break;
		}
		bf = list_first_entry(&txq->axq_q, struct jaldi_buf,
				      list);

		if (bf->bf_stale) {
			list_del(&bf->list);
			spin_unlock_bh(&txq->axq_lock);

			jaldi_tx_return_buffer(sc, bf);
			continue;
		}

		lastbf = bf->bf_lastbf;
		if (!retry_tx)
			lastbf->bf_tx_aborted = true;

		/* remove ath_buf's of the same mpdu from txq */
		list_cut_position(&bf_head, &txq->axq_q, &lastbf->list);

		txq->axq_depth--;

		spin_unlock_bh(&txq->axq_lock);

		jaldi_tx_complete_buf(sc, bf, txq, &bf_head, 0);
	}

	spin_lock_bh(&txq->axq_lock);
	txq->axq_tx_inprogress = false;
	spin_unlock_bh(&txq->axq_lock);
}

void jaldi_drain_all_txq(struct jaldi_softc *sc, bool retry_tx)
{
	struct jaldi_hw *hw = sc->hw;
	struct jaldi_txq *txq;
	int i, npend = 0;

	if (sc->sc_flags & SC_OP_INVALID)
		return;

	/* Stop data queues */
	for (i = 0; i < JALDI_NUM_TX_QUEUES; i++) {
		if (JALDI_TXQ_SETUP(sc, i)) {
			txq = &sc->tx.txq[i];
			jaldi_hw_stoptxdma(hw, txq->axq_qnum);
			npend += jaldi_hw_numtxpending(hw, txq->axq_qnum);
		}
	}

	if (npend) {
		int r;

		jaldi_print(JALDI_FATAL,
			  "Failed to stop TX DMA. Resetting hardware!\n");

		spin_lock_bh(&sc->sc_resetlock);
		r = jaldi_hw_reset(hw, sc->hw->curchan, false);
		if (r)
			jaldi_print(JALDI_FATAL,
				  "Unable to reset hardware; reset status %d\n",
				  r);
		spin_unlock_bh(&sc->sc_resetlock);
	}

	for (i = 0; i < JALDI_NUM_TX_QUEUES; i++) {
		if (JALDI_TXQ_SETUP(sc, i))
			jaldi_draintxq(sc, &sc->tx.txq[i], retry_tx);
	}
}


long get_jaldi_tx_from_skb(struct sk_buff *skb) {
//	return 0; // TODO: decide on a packet format and implement this by reading protocol header field
	return NSEC_PER_SEC/10;

}

int jaldi_pkt_type(struct sk_buff *skb)
{
	// TODO : define format and check for potential control pkt
	return JALDI_PKT_TYPE_NORMAL;
}

// TODO: should return appropriate type
int jaldi_get_qos_type_from_skb(struct sk_buff *skb) {
	return JALDI_QOS_BULK; // TODO: make this random-ish for testing
}

/*****************
 * config
 ****************/

/* This is slightly different than the way ath9k sets the txpower.
 * Txpower is set by mac80211, so they keep track of the current txpower 
 * setting in the softc's config struct. During init, this is set to maximum 
 * power (50 dBm, or 100 in .5 dBm units as specified in the header). As part 
 * of handling mac80211 config changes, ath9k checks for a new value and 
 * updates it here. After performing this update, the softc's power limit is 
 * set to whatever mac80211 said: thus, the softc's curtxpow is actually the 
 * last power limit that was set through mac80211. Here, we set tx power though
 * the ctl packet interface, and the softc's value is the canonical power level
 * used throughotu the driver. Thus, we actually update the power setting in 
 * the hardware based on the new value we're passed by the caller, and then
 * update the softc's value accordingly. */ 
static void jaldi_update_txpow(struct jaldi_softc *sc, u32 newtxpow)
{
	struct jaldi_hw *hw = sc->hw;

	if(newtxpow != sc->curtxpow) {
		jaldi_hw_set_txpowerlimit(hw, newtxpow);
		sc->curtxpow = newtxpow;
	}
}

/*
 * chan - the channel we're changing to
 */
int jaldi_set_channel(struct jaldi_softc *sc, struct jaldi_channel *chan)
{
	struct jaldi_hw *hw = sc->hw;
	bool fastcc = true, stopped;
	int r;
	
	if (sc->sc_flags & SC_OP_INVALID)
		return -EIO;

	jaldi_ps_wakeup(sc);
	jaldi_drain_all_txq(sc, false); // TODO
	stopped = jaldi_stoprecv(sc);

	/* XXX: do not flush receive queue here. We don't want
	 * to flush data frames already in queue because of
	 * changing channel. */

	if (!stopped || (sc->sc_flags & SC_OP_FULL_RESET))
		fastcc = false;

	jaldi_print(JALDI_DEBUG,
		  "(%u MHz) -> (%u MHz)\n",
		  hw->curchan->channel,
		  chan->center_freq);

	spin_lock_bh(&sc->sc_resetlock);

	r = jaldi_hw_reset(hw, chan, fastcc);
	if (r) {
		jaldi_print(JALDI_FATAL,
			  "Unable to reset channel (%u MHz), "
			  "reset status %d\n",
			  chan->center_freq, r);
		spin_unlock_bh(&sc->sc_resetlock);
		goto ps_restore;
	}
	spin_unlock_bh(&sc->sc_resetlock);

	sc->sc_flags &= ~SC_OP_FULL_RESET;

	if (jaldi_startrecv(sc) != 0) {
		jaldi_print(JALDI_FATAL,
			  "Unable to restart recv logic\n");
		r = -EIO;
		goto ps_restore;
	}

	//ath_cache_conf_rate(sc, &hw->conf);
	jaldi_update_txpow(sc, sc->curtxpow);
	jaldi_hw_set_interrupts(hw, hw->imask);

 ps_restore:
	jaldi_ps_restore(sc);
	return r;
}

int jaldi_hw_ctl(struct jaldi_softc *sc, struct jaldi_packet *pkt) {
	// TODO: set the right control parameters

	return 0;
}

static void jaldi_tx_txqaddbuf(struct jaldi_softc *sc, struct jaldi_txq *txq,
				struct list_head *head) 
{
	struct jaldi_hw *hw;
	struct jaldi_buf *bf;

	DBG_START_MSG;

	if (list_empty(head)) 
		return;

	hw = sc->hw;

	bf = list_first_entry(head, struct jaldi_buf, list);

	list_splice_tail_init(head, &txq->axq_q);
	
	if (txq->axq_link == NULL) {
		jaldi_hw_puttxbuf(hw, txq->axq_qnum, bf->bf_daddr);
		jaldi_print(JALDI_DEBUG, "TXDP[%u] = %llx (%p)\n",
				txq->axq_qnum, ito64(bf->bf_daddr),
				bf->bf_desc);
	} else {
		*txq->axq_link = bf->bf_daddr;
		jaldi_print(JALDI_DEBUG, "link[%u] (%p)=%llx (%p)\n",
				txq->axq_qnum, txq->axq_link,
				ito64(bf->bf_daddr), bf->bf_desc);
	}

	txq->axq_link = &((struct jaldi_desc *)bf->bf_lastbf->bf_desc)->ds_link; // get_desc_link

	jaldi_hw_txstart(hw, txq->axq_qnum);

	txq->axq_depth++;
}

static void jaldi_tx_start_dma(struct jaldi_softc *sc, struct jaldi_packet *pkt, 
				struct jaldi_buf *bf)
{
	struct jaldi_desc *ds;
	struct list_head bf_head;
	struct jaldi_hw *hw;

	DBG_START_MSG;

	hw = sc->hw;

	INIT_LIST_HEAD(&bf_head);
	OHAI;
	list_add_tail(&bf->list, &bf_head);
	OHAI;
	ds = bf->bf_desc;
	OHAI;
	ds->ds_link = 0; /* hw_set_desc_link */
	OHAI;
	jaldi_hw_set11n_txdesc(hw, ds, bf->bf_frmlen, pkt->type, MAX_RATE_POWER, bf->bf_flags);
	jaldi_hw_fill_txdesc(hw, ds, pkt->skb->len, true, true, ds, bf->bf_buf_addr, pkt->txq->axq_qnum);
	OHAI;
	spin_lock_bh(&pkt->txq->axq_lock);

	/* TODO: aggregates are handled seperately in ath9k */

	bf->bf_lastbf = bf;
	bf->bf_nframes = 1;
	jaldi_tx_txqaddbuf(sc, pkt->txq, &bf_head);
	
	spin_unlock_bh(&pkt->txq->axq_lock);
}

/* This method basically does the following:
 * - assigns skb to hw buffer
 * - sets up tx flags based on current config
 * - allocates DMA buffer
 * - 
 */
int jaldi_hw_tx(struct jaldi_softc *sc, struct jaldi_packet *pkt)
{
	struct jaldi_buf *bf;
	struct jaldi_hw *hw;
	struct list_head bf_head;
	int r;

	DBG_START_MSG;

	hw = sc->hw;
	
	bf = jaldi_tx_get_buffer(sc);	
	if (!bf) {
		jaldi_print(JALDI_ALERT, "TX buffers are full\n");
		return -1;
	}

	r = jaldi_tx_setup_buffer(sc, bf, pkt);

	if (unlikely(r)) {
		jaldi_print(JALDI_FATAL, "TX mem alloc failure\n");
		jaldi_tx_return_buffer(sc, bf);
	}

	// do a check to make sure the qeueu we're on doesn't exceed our max length... TODO
	
	jaldi_tx_start_dma(sc, pkt, bf);
		
	return 0;
}


enum hrtimer_restart jaldi_timer_tx(struct hrtimer *handle)
{
	struct jaldi_packet *pkt;
	struct jaldi_softc *sc = container_of(handle, struct jaldi_softc,
						tx_timer); 
	DBG_START_MSG;
	pkt = jaldi_tx_dequeue(sc);
	jaldi_hw_tx(sc, pkt);

	return HRTIMER_NORESTART;
}

/* 
 * This function receives an sk_buff from the kernel, and packages it up into a
 * jaldi_packet. Other high-level tx behaviors should be handled here. This 
 * should be completely device agnostic, as device-specific stuff should be
 * handled in jaldi_hw_tx.
 */
int jaldi_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len, qnum;
	char *data;
	struct jaldi_softc *sc = netdev_priv(dev);
	struct jaldi_hw *hw;
	struct jaldi_packet *pkt;
	
	DBG_START_MSG;

	hw = sc->hw;

	data = skb->data;
	len = skb->len;
	
	dev->trans_start = jiffies;

	if(unlikely(hw->power_mode != JALDI_PM_AWAKE)) {
		jaldi_ps_wakeup(sc);
		jaldi_print(JALDI_DEBUG, "Waking up to do TX\n");
	}
	
	/* create jaldi packet */
	pkt = kmalloc (sizeof (struct jaldi_packet), GFP_KERNEL);
	if (!pkt) {
		jaldi_print(JALDI_WARN, "Out of memory while allocating packet\n");
		return 0;
	}
	
	pkt->next = NULL; /* XXX */
	pkt->sc = sc;
	pkt->data = skb->data;
	pkt->skb = skb;
	pkt->tx_time = get_jaldi_tx_from_skb(skb);
	pkt->qos_type = jaldi_get_qos_type_from_skb(skb);
	pkt->txq = &sc->tx.txq[qnum]; /* replaces need for txctl from ath9k */
	pkt->type = jaldi_pkt_type(skb);

	
//	printk(KERN_DEBUG "jaldi hai: pkt len is %d\n", pkt->skb->len);

	/* check for type: control packet or standard */
	if( pkt->type == JALDI_PKT_TYPE_CONTROL ) { // should check header
		jaldi_hw_ctl(sc, pkt);
	} else {
	//	jaldi_hw_tx(sc, pkt);
		if(pkt->skb->len < 150) {
			getnstimeofday(&sc->debug.ts);
			/* add jaldi packet to tx_queue */
			jaldi_tx_enqueue(sc,pkt);
			/* create kernel timer for packet transmission */
			sc->tx_timer.function = jaldi_timer_tx;
			hrtimer_start(&sc->tx_timer, ktime_set(0,pkt->tx_time), HRTIMER_MODE_REL);
			sc->debug.intended_tx_times[(sc->debug.intended_tx_idx)%2048] = timespec_to_ns(&sc->debug.ts)+pkt->tx_time;
			sc->debug.intended_tx_idx++;
//			printk(KERN_DEBUG "jaldi hai: tx intend: %lld\n", pkt->tx_time);
		} else {
			printk(KERN_DEBUG "jaldi hai: regular send\n");
			jaldi_hw_tx(sc,pkt);
		}
	}
	
	return 0;

}

void jaldi_radio_enable(struct jaldi_softc *sc) {
	struct jaldi_hw *hw = sc->hw;
	int r;

	DBG_START_MSG;

	jaldi_ps_wakeup(sc);

	if (!hw->curchan)
		hw->curchan = &sc->chans[JALDI_5GHZ][0]; /* default is chan 36 (5180Mhz, 14) */
	
	spin_lock_bh(&sc->sc_resetlock);
	r = jaldi_hw_reset(hw, hw->curchan, false);
	if (r) {
		jaldi_print(JALDI_FATAL,
			  "Unable to reset channel (%u MHz), "
			  "reset status %d\n",
			  hw->curchan->center_freq, r);
	}
	spin_unlock_bh(&sc->sc_resetlock);
	
	OHAI;

	jaldi_update_txpow(sc, sc->curtxpow);
	if (jaldi_startrecv(sc) != 0) {
		jaldi_print(JALDI_FATAL,
			  "Unable to restart recv logic\n");
		return;
	}

	OHAI;

	/* Re-Enable  interrupts */
	jaldi_hw_set_interrupts(hw, hw->imask);

	//ieee80211_wake_queues(hw);
	jaldi_ps_restore(sc);
}

void jaldi_radio_disable(struct jaldi_softc *sc)
{
	struct jaldi_hw *hw = sc->hw;
	int r;

	jaldi_ps_wakeup(sc);
	//ieee80211_stop_queues(hw);

	/* Disable interrupts */
	jaldi_hw_set_interrupts(hw, 0);

	jaldi_drain_all_txq(sc, false);	/* clear pending tx frames */
	jaldi_stoprecv(sc);		/* turn off frame recv */
	jaldi_flushrecv(sc);		/* flush recv queue */

	if (!hw->curchan)
		hw->curchan = &sc->chans[JALDI_5GHZ][0]; /* default is chan 36 (5180Mhz, 14) */

	spin_lock_bh(&sc->sc_resetlock);
	r = jaldi_hw_reset(hw, hw->curchan, false);
	if (r) {
		jaldi_print(JALDI_FATAL,
			  "Unable to reset channel (%u MHz), "
			  "reset status %d\n",
			  hw->curchan->center_freq, r);
	}
	spin_unlock_bh(&sc->sc_resetlock);

	jaldi_ps_restore(sc);
	jaldi_setpower(sc, JALDI_PM_FULL_SLEEP);
}

/*
 * TODO: this is where ifconfig starts our driver. we need to integrate this 
 * with the rest of the driver. Should establish DMA, do the hw reset, all that
 * jazz. This should also be where we call whatever actually disables the acks
 * and carrier sense: that should not be done during HW initialization.
 */
int jaldi_open(struct net_device *dev)
{
	struct jaldi_softc *sc = netdev_priv(dev);
	struct jaldi_hw *hw = sc->hw;
	struct jaldi_channel *init_chan;
	int r;

	DBG_START_MSG;

	mutex_lock(&sc->mutex);

	/* set default channel if none specified */
	if (!hw->curchan) {
		init_chan = &sc->chans[JALDI_5GHZ][0]; /* default is chan 36 (5180Mhz, 14) */
		hw->curchan = init_chan;
	}

	memcpy(dev->dev_addr, sc->macaddr, ETH_ALEN);

	spin_lock_bh(&sc->sc_resetlock);
	r = jaldi_hw_reset(hw, init_chan, true); /* we're settnig channel for first time so always true */
	if (r) {
		jaldi_print(JALDI_FATAL, "Unable to reset hw; reset status %d (freq %u MHz)\n",
				r, init_chan->center_freq);
		spin_unlock_bh(&sc->sc_resetlock);
		goto mutex_unlock;
	}
	spin_unlock_bh(&sc->sc_resetlock);

	if (jaldi_startrecv(sc) != 0) {
		jaldi_print(JALDI_FATAL,
			  "Unable to restart recv logic\n");
		r = -EIO;
		goto mutex_unlock;
	}

	OHAI;

	hw->imask = JALDI_INT_TX | JALDI_INT_RXEOL | JALDI_INT_RXORN 
			| JALDI_INT_RX | JALDI_INT_FATAL | JALDI_INT_GLOBAL;

	jaldi_radio_enable(sc);

	netif_start_queue(dev);

mutex_unlock:
	mutex_unlock(&sc->mutex);

	return r;
}

void jaldi_tasklet(unsigned long data)
{
	struct jaldi_softc *sc = (struct jaldi_softc *)data;
	struct jaldi_hw *hw = sc->hw;

	u32 status = sc->intrstatus;

	if (status & JALDI_INT_FATAL) { 
		jaldi_print(JALDI_FATAL, "Fatal interrupt status!\n");
	}

	
	if (status & (JALDI_INT_RX | JALDI_INT_RXEOL
			| JALDI_INT_RXORN)) {
		spin_lock_bh(&sc->rx.rxflushlock);
		jaldi_rx_tasklet(sc, 0);
		spin_unlock_bh(&sc->rx.rxflushlock);
	}

	jaldi_hw_set_interrupts(hw, hw->imask);
	jaldi_ps_restore(sc);

}

static void __exit jaldi_cleanup(void)
{
	jaldi_ahb_exit();
	jaldi_pci_exit();
	jaldi_debug_remove_root();
	jaldi_print(JALDI_INFO, "JaldiMAC driver removed.\n");
	return;
}

static const struct net_device_ops jaldi_netdev_ops =
{
	.ndo_open			= jaldi_open,
	.ndo_stop			= jaldi_release,
	.ndo_start_xmit			= jaldi_tx,
//	.ndo_get_stats			= jaldi_stats,
};

void jaldi_attach_netdev_ops(struct net_device *dev)
{
	dev->netdev_ops = &jaldi_netdev_ops;
}

static int __init jaldi_init_module(void)
{
	int result, ret = -ENODEV;
	DBG_START_MSG;
	printk(KERN_INFO "jaldi: Loading JaldiMAC kernel driver. Debug level is %d.\n", JALDI_DEBUG_LEVEL);

	result = jaldi_debug_create_root();
	if (result) {
		printk(KERN_ERR "jaldi: unable to create debugfs root: %d\n", 
			result);
		goto err_out;
	}

	result = jaldi_pci_init();
	if (result < 0) {
		jaldi_print(JALDI_FATAL, "No PCI device found, driver load cancelled.\n");
		goto err_pci;
	}

	jaldi_print(JALDI_DEBUG, "pci_init returns %d\n", result);

	result = jaldi_ahb_init();
	if (result < 0) {
		jaldi_print(JALDI_WARN, "AHB init failed (jaldimac devices should pass this!)\n");
		goto err_ahb;
	}
	jaldi_print(JALDI_DEBUG, "ahb_init returns %d\n", result);

	ret = 0;

	return ret;

err_ahb:
	jaldi_pci_exit();
err_pci:
	jaldi_debug_remove_root();
err_out:
	return ret;
}

module_init(jaldi_init_module);
module_exit(jaldi_cleanup);
