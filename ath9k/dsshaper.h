/*
 * Copyright (c) 2008-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

//#ifndef ATH9K_H
//#define ATH9K_H
//#include <linux/dma-mapping.h>
//#include <linux/etherdevice.h>
//#include <linux/device.h>
//#include <linux/interrupt.h>
//#include <linux/leds.h>
//#include <linux/completion.h>
//#include <linux/time.h>
#include <linux/timer.h> //for timer mengy
#include <linux/time.h>
#include <linux/types.h>
#include <linux/math64.h>
//#include <linux/hw_random.h>
#include <net/mac80211.h>
#include <linux/types.h>
#include "ath9k.h"
//#include <include/linux/list.h>

//#include "common.h"
//#include <net/mac80211.h>

//#include "../ath.h"

//#include "hw.h"
//#include "hw-ops.h"

//#include "common-init.h"
//#include "common-beacon.h"
//#include "common-debug.h"
//#include "common-spectral.h"
//#include "debug.h"
//#include "mci.h"
//#include "dfs.h"

//extern struct ieee80211_ops ath9k_ops;
//extern int ath9k_modparam_nohwcrypt;
//extern int ath9k_led_blink;
//extern bool is_ath9k_unloaded;
//extern int ath9k_use_chanctx;
/*for timestamp te th tw by mengy*/
extern struct timespec last_ack; // record the last ack timestamp by mengy 
extern int update_te_flag;
extern int update_tw_flag;
extern int has_beacon_flag;
extern int packet_number;
extern int packet_size_all;
extern struct timespec this_ack;
extern struct timespec this_tw;
extern int last_ack_update_flag;


/*for update_deqrate*/
extern int flow_peak; //for the control peak 
extern int ntrans_; // record the flow_peak change times
extern struct timespec delay_sum_;
extern int pktsize_sum_; //bit
extern struct timespec checkInterval_;
extern struct timespec checktime_; //last time update peak
extern int alpha_; //%
extern int rate_avg_; // bits/s
extern int delay_avg_; //us
extern int switchOn_ ;
extern int delay_optimal_;//us
extern int fix_peak ; //bits/s
extern int flow_peak ; // bits/s
///int beta_ ; //bits/s
//int burst_size_; //bits
//int deltaIncrease_ ; //bits/s
//struct timespec checkThtime_;
//struct timespec checkThInterval_;
//int throughput_sum_;
//struct timespec checkThtime_;

#ifndef ATH9K_DSSHAPPER_H
#define ATH9K_DSSHAPPER_H





//class DSShaper;

//class DSShaperHandler : public Handler {
//public:
//	DSShaperHandler(DSShaper *s) : shaper_(s) {}
//	void handle(Event *e);
//private:
//	DSShaper *shaper_;
//};

/*
class DSShaper: public Connector {
private:
	int		received_packets ;
	int		sent_packets ;
	int		shaped_packets ;
	int		dropped_packets ;
	
	PacketQueue shape_queue;
	
	int		curr_bucket_contents ;
	int		flow_id_;
	double      last_time ;
        bool        shape_packet(Packet *p) ;	
        void        schedule_packet(Packet *p) ;
        bool        in_profile(Packet *p) ;
	void        reset_counters();
	DSShaperHandler sh_;
public:
			DSShaper() ;
	void 		resume();
	void		recv(Packet *p, Handler *h) ;
	int		command(int argc, const char*const* argv) ;
	double		peak_ ;
	void		update_bucket_contents() ;
	int		burst_size_ ;
	int         max_queue_length;
} ;
*/
extern void update_deqrate(struct timespec p_delay,struct timespec all_delay, int pktsize_, int pnumber_);
extern void update_bucket_contents(void);
//bool shape_packet(struct list_head *packet,struct ath_softc *sc, struct ath_txq *txq,bool internal,int len);
//int list_length(struct list_head *head);
//int timer_module(double time_delay,struct timer_list *my_timer);
extern void recv(int len, struct ath_softc* sc, struct ath_txq* txq, struct list_head* p, bool internal);
extern void ath_tx_txqaddbuf(struct ath_softc *sc, struct ath_txq *txq,struct list_head *head, bool internal); // changed by my
//bool shape_packet(struct list_head *packet,struct ath_softc *sc, struct ath_txq *txq,bool internal,int len);
//void schedule_packet(struct list_head *p,int len);
//void resume(void);
//int in_profile(int size);
//void update_bucket_contents(void);
/*
void schedule_packet(Packet *p);
bool in_profile(Packet *p);
void reset_counters();
void resume();
void recv(Packet *p, Handler *h);
int	 command(int argc, const char*const* argv);
void update_bucket_contents();
int list_length(struct list_head *head);
*/
struct DSShaper {
	long		received_packets ;
	long		sent_packets ;
	long		shaped_packets ;
	long		dropped_packets ;
	long		curr_bucket_contents ;
	int		flow_id_;
	struct timespec      last_time ; // last time update bucket contents
	//int		peak_ ;
	int			burst_size_ ;
	int         max_queue_length;
};

//extern int flow_peak;
struct packet_msg
{
	/* data */
	struct list_head list;
	struct ath_softc *sc;
	struct ath_txq *txq;
	bool internal;
	int len;


};
struct packet_dsshaper
{
	/* data */
	struct list_head list;
	struct list_head* packet;
	struct hrtimer hr_timer;


};
	//struct list_head shape_queue;

//DSShaper()
  //{
    //received_packets = 0;
	//sent_packets = 0;
	//shaped_packets = 0;
	//dropped_packets = 0;
	//curr_bucket_contents = 0;
	//flow_id_ = 0;
	//max_queue_length= 0;
	//last_time = 0;
    //shape_queue = NULL;

  //

//struct timer_list a_timer;
#endif







