/**** Added for spectrum analysis ****/

#include <linux/net.h>
#include <net/sock.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/socket.h>

#include <linux/netlink.h>
#include <linux/skbuff.h>

#include "fft_data.h"

#define USE_AR9002 1
//#define USE_PRINT 1
// TODO: Currently using configs for only ar9002.h

#if USE_AR9002
#include "ar9002_phy.h"
#else
#include "ar9003_phy.h"
#endif

#define SEND_NETWORK true
#define PRINT_FFTS true

#define PORT_NUM 10000
//#define SERV_IP_ADDRESS 329738624l
//#define SERV_IP_ADDRESS 1912711360
#define SERV_IP_ADDRESS 16777343
#define NETLINK_USER 31 // TODO: Finalize how to set the NETLINK_USER value.

// Global variables.

// State related condtitional variables.
static bool is_spectral_scan_intialized = false;
static bool is_udp_socket_initalized = false; // Stores whether the state of the data reporting mechanism has been initialized or not.
#ifdef USE_NETLINK
static bool is_nl_server_initialized = false;

// Store a pointer to the ath_hw structure for the atheros driver.
static struct ath_hw* ah_ref = NULL;
#endif

// Used to store the FFT reports obtained from the chipset.
static fft_report_data reporting_data;
//static fft_data *fft_stats = &reporting_data.fft_stats;
static u64 pkt_seq_num = 0;

// The kernel based client socket for the data reporting mechanism.
static struct socket *reporting_sock = NULL;

int data_len = FFT_PACKET_SIZE; // + 5;

#ifdef USE_NETLINK
// Netlink based communication variables.
static struct sock *nl_server_sock = NULL;
#endif

// Local method declarations.
void reset_ffts(void);
void reset_fft_data(void);
void print_spectrum_fft(spectrum_fft* spec_data, int i);
void update_spectrum_fft(spectrum_fft* fft_input, spectrum_fft* fft_dest, unsigned char* ptr, u16 len, s16 nf_corr);
void socket_alloc(void);

// Reset the fft_stats object.
void reset_fft_data(void) {
	//reporting_data = kmalloc (sizeof(fft_report), GFP_ATOMIC);
  int i = 0;

	reporting_data.report_count = 0;
  for (; i < NUM_REPORTS; i++) {
		reporting_data.fft_reports[i].fft_stats.count = 0;
	}

  pkt_seq_num ++;
  reporting_data.fft_reports[0].fft_stats.seq_num = pkt_seq_num;
}

// Reset the fft_stats object.
void reset_ffts(void) {
  reset_fft_data();

  // Reset variables corresponding to state.
  reporting_data.transmit_count = 0;
  reporting_data.trans_remaining_count = 0;
}

// Print the contents of a single fft sample.
void print_spectrum_fft(spectrum_fft* spec_data, int i) {
	u32 j = 0;

	printk("******%d %u %llu %u %d %u %u %u : ",
			i + 1,
			spec_data->freq,
			spec_data->ts, 
			//spec_data->ts1, 
			spec_data->width,
			spec_data->rssi,
			spec_data->start_index,
			spec_data->datalen,
			//spec_data->datalen_orig,
			spec_data->sample_bin_cnt);

	for (j = 0; j < spec_data->sample_bin_cnt; j++) {
		printk("%u ",  spec_data->samples[j]);
	}

	printk("\n");
}

// Fill the fft_details object using the contents from fft_input object and the 
// buffer pointer ptr.
void update_spectrum_fft(spectrum_fft* fft_input, spectrum_fft* fft_dest, unsigned char* ptr, u16 len, s16 nf_corr)
{
	fft_dest->freq = fft_input->freq;
	fft_dest->ts = fft_input->ts;
	//fft_dest->ts1 = (jiffies*1000000000L)/HZ;
	fft_dest->width = fft_input->width;
	//fft_dest->rssi = fft_input->rssi;

	// Apply correction to rssi.
	//printk("Original Rssi: %d\n", fft_input->rssi); 
	fft_dest->rssi = fft_input->rssi + nf_corr; 
  //printk("Correction applied: %d\n", get_current_fft_nf_correction()); 
	//printk("Updated Rssi: %d\n", fft_dest->rssi); 
	
	//fft_dest->rssi_type = fft_input->rssi_type;
	fft_dest->datalen = fft_input->datalen;

	fft_dest->sample_bin_cnt = len;

	//if (!is_backwards) {
		memcpy(fft_dest->samples, ptr, len);
	//} else {
	//	memcpy(fft_dest->samples, ptr - len + 1, len);
	//}
}

// Updates and prints a single fft sample directly without adding it to the fft data.
void update_print_fft_direct(spectrum_fft* fft_input, unsigned char* ptr, u16 len, s16 nf_corr) {
	update_spectrum_fft(fft_input, fft_input, ptr, len, nf_corr);
	print_spectrum_fft(fft_input, -1);
}

// Add the input fft sample to a fft_data data structure. If the structure is full,
// it prints the contents of fft_data structure and resets it.
void update_print_fft_data(spectrum_fft* fft_input, unsigned char* ptr, u16 len, s16 nf_corr, event_counter* spectrum_events) {
	// Take lock.
	//spin_lock_bh(&fft_stats_lock);
  fft_data* curr_fft_data;

	if (!ptr) {
		printk("NULL ptr...\n");
    return;
	}

  if (len > MAXLEN) {
	  printk("length should be less than %d ... got %d\n", MAXLEN, len);
    return;
	}

  if (reporting_data.fft_reports[reporting_data.report_count].fft_stats.count >= MAXCOUNT) {
			reporting_data.report_count++;
      reporting_data.fft_reports[reporting_data.report_count].fft_stats.seq_num = pkt_seq_num;
			pkt_seq_num ++;
  }

  if (reporting_data.report_count >= NUM_REPORTS) {
    if (SEND_NETWORK) {
      tx_report_contents();
    } else if (PRINT_FFTS) {
      u32 i = 0, j = 0;

      for (i = 0; i < NUM_REPORTS; i++) {
        for (j = 0; j < MAXCOUNT; j++) {
          print_spectrum_fft(&reporting_data.fft_reports[i].fft_stats.samples[j], i * NUM_REPORTS + j);
        }
      }
    } else {
      print_spectrum_events(spectrum_events);
    }

    // Reset the fft data.
    reset_fft_data();
  }

  curr_fft_data = &reporting_data.fft_reports[reporting_data.report_count].fft_stats;
  update_spectrum_fft(fft_input, &curr_fft_data->samples[curr_fft_data->count], ptr, len, nf_corr);
  curr_fft_data->count++;

  // Release lock.
	//spin_unlock_bh(&fft_stats_lock);
}

// Initialize the kernel based client socket that will be used to send packets to the server.
void socket_alloc(void) {
	if (sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &reporting_sock) < 0) {
		printk(KERN_INFO "airshark: could not create a socket for airshark reporting, error = %d\n", -ENXIO);
	}

	//reporting_sock->sk->sk_allocation = GFP_ATOMIC;

	printk(KERN_INFO "socket_alloc called, allocation type: %d\n", reporting_sock->sk->sk_allocation);
}

// Exit the spectral scan state.
void exit_spectral_scan_state(void) {
	//end_spectral_scan_htc();
	exit_data_report_mechanism();
	exit_server_socket_mechanism();
}

void exit_data_report_mechanism(void) {
	printk("Entering: %s\n",__FUNCTION__);
	if (reporting_sock) { 
		sock_release(reporting_sock);
	}

	reporting_sock = NULL;
}

// Initialize the spectral scan state.
void init_spectral_scan_state(struct ath_hw *ah) {
  int i = 0;

  // Initialize card specific noise floor related state.
  ah->current_fft_channel = 0;
  
  ah->min_nf_val_2g = 0;
  ah->min_nf_val_5g = 0;
  ah->curr_nf_val = DUMMY_NF_VAL; 
  ah->curr_nfcorr_val = 0;

  for (i = 0; i < NUM_CHANNELS_NF; i++) {
    ah->nf_correction[i] = 0;
  }

  // Initialize global mechanism for spectral data reporting. Only needs to be done once.
	if (is_spectral_scan_intialized) {
		printk("Spectral scan initalization already done! Doing nothing...\n");
		return;
	}

	is_spectral_scan_intialized = true;

  #ifdef USE_NETLINK
	ah_ref = ah;
  #endif

	init_server_socket_mechanism();
	init_data_report_mechanism();

  #ifdef USE_NETLINK
	if (ah == NULL) {
		printk("********Warning******* ah is null: will causes errors\n");
	}
  #endif

	printk("Spectral scan initalization complete...\n");
}

// Initialize the variables for the data reporting mechanism.
void init_data_report_mechanism(void) {	
        u32 i = 0;

	if (is_udp_socket_initalized) {
		return;
	}
	is_udp_socket_initalized = true;

	// Initialize the socket.
	socket_alloc();

	// Initialize the fft report and stats objects.
	reset_ffts();

	// Initialize the work queue.
	//INIT_WORK(&reporting_data.work, report_workqueue_handler);

	// Initialize the packets.
	for (; i < NUM_REPORTS; i++) {
		reporting_data.fft_reports[i].addr.sin_family = AF_INET;
		reporting_data.fft_reports[i].addr.sin_port = htons(PORT_NUM);
		reporting_data.fft_reports[i].addr.sin_addr.s_addr = SERV_IP_ADDRESS;
		//reporting_data.fft_reports[i].addr.sin_addr.s_addr = INADDR_LOOPBACK;

		reporting_data.fft_reports[i].iov.iov_base = reporting_data.fft_reports[i].data;
		reporting_data.fft_reports[i].iov.iov_len = data_len;

		reporting_data.fft_reports[i].msg.msg_name = (void *) &reporting_data.fft_reports[i].addr;
		reporting_data.fft_reports[i].msg.msg_namelen = sizeof(reporting_data.fft_reports[i].addr);
		reporting_data.fft_reports[i].msg.msg_iov = &reporting_data.fft_reports[i].iov;
		reporting_data.fft_reports[i].msg.msg_iovlen = 1;
		reporting_data.fft_reports[i].msg.msg_control = NULL;
		reporting_data.fft_reports[i].msg.msg_controllen = 0;
		reporting_data.fft_reports[i].msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
	}

	printk("init_data_report_mechanism: Initialization completed.\n");
}

// The handler function work queue. This function sends the packet the server.
void report_workqueue_handler(struct work_struct *work) {
	mm_segment_t oldfs;
	int result, current_index;
  //fft_report *report = container_of(work, fft_report, work);

  //if (report) {
    //printk("Sending report: reporting_data.trans_remaining_count: %d....\n", reporting_data.trans_remaining_count); 
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    current_index = reporting_data.transmit_count - reporting_data.trans_remaining_count;
    result = sock_sendmsg(reporting_sock, &reporting_data.fft_reports[current_index].msg, data_len);
    // report->msg.msg_namelen);
    //printk("result: %d FFT_PACKET_SIZE: %d sizeof(fft_data): %d data_len: %d: \n",
    //	 result, FFT_PACKET_SIZE, sizeof(fft_data), data_len);
    set_fs(oldfs);
  //} else {
  //  printk("Warning: report == NULL\n");
  //}
	// Finished the data packet transmission.
  reporting_data.trans_remaining_count--;
  //printk("Sent report: reporting_data.trans_remaining_count: %d....\n", reporting_data.trans_remaining_count); 
}

// Put the reporting_data onto the work queue to be sent to the server.
void tx_report_contents(void) {
  u32 i = 0;
  struct fft_report *curr_report;

  if (reporting_data.trans_remaining_count > 0) {
      printk("Missing opportunity to transmit: %d..\n", reporting_data.trans_remaining_count);
      return;
  }

  reporting_data.transmit_count = reporting_data.report_count ;
  reporting_data.trans_remaining_count = reporting_data.report_count;

  for (; i < reporting_data.report_count; i++) {
    curr_report = &reporting_data.fft_reports[i];

    memcpy(curr_report->data, &curr_report->fft_stats, sizeof(fft_data));
    INIT_WORK(&curr_report->work, report_workqueue_handler);
    schedule_work(&curr_report->work);
  }
}


int init_server_socket_mechanism(void) {
	printk("Entering: %s\n", __FUNCTION__);

  #ifdef USE_NETLINK
	if (is_nl_server_initialized) {
		printk("Netlink socket initialization already called..\n");
		return 0;
	}

	nl_server_sock = netlink_kernel_create(&init_net, NETLINK_USER, 0, recv_process_client_command,
	NULL, THIS_MODULE);

	if (!nl_server_sock) {
		printk(KERN_ALERT "Error creating netlinks server socket.\n");
		return -1;
	}

	printk("Netlink server socket initialized...\n");
  #endif

	return 0;
}

// Used from: http://stackoverflow.com/questions/3299386/how-to-use-netlink-socket-to-communicate-with-a-kernel-module
#ifdef USE_NETLINK
void recv_process_client_command(struct sk_buff *skb) {
	struct nlmsghdr *nlh;
	char message_type[20];
	int length = 0;
	char *msg;
	int channel = 9;

  #ifdef USE_PRINT
	printk(KERN_INFO "Entering: %s\n", __FUNCTION__);
  #endif

	nlh = (struct nlmsghdr*) skb->data;
	msg = (char*) nlmsg_data(nlh);

  #ifdef USE_PRINT
	printk(KERN_INFO "Netlink received msg from pid %d payload: %s\n", nlh->nlmsg_pid, msg);
  #endif

	if (!strchr(msg, ' ')) {

    #ifdef USE_PRINT
		printk("No arguments sent...\n");
    #endif

		strcpy((char * ) &message_type, msg);

	} else {
		length = strchr(msg, ' ') - msg;
		strncpy((char *) &message_type, msg, length);
		message_type[length] = '\0'; 
	}

  #ifdef USE_PRINT
	printk(KERN_INFO "Message type:%s... length: %d\n", message_type, length);
  #endif

	if (ah_ref == NULL) {
    #ifdef USE_PRINT
		printk("**Warning** Not executing command as ah is NULL...\n");
    #endif

		return;
	}

	if (!strcmp(message_type, "get_survey_info")) {
		if (ah_ref == NULL) {

      #ifdef USE_PRINT
			printk("Not doing anything as ah_ref == NULL...\n"); 
      #endif

		} else {
			get_survey_fft();
		}
	} else if(!strcmp(message_type, "change_channel")) {
		if (ah_ref->curchan == NULL) {

      #ifdef USE_PRINT
			printk("Not doing anything as ah_ref->curchan == NULL...\n"); 
      #endif

		} else {
			kstrtoint(msg + length + 1, 10, &channel);

      #ifdef USE_PRINT
			printk("Changing to channel: %u...\n", channel);
      #endif

			spectral_channel_change(channel);

		}
	} else if(!strcmp(message_type, "start_scan")) {

    #ifdef USE_PRINT
		printk("Received start_scan command!!!\n");
    #endif

		start_spectral_scan_htc(ah_ref);

	} else if(!strcmp(message_type, "set_scan_options")) {

    #ifdef USE_PRINT
		printk("Received set_scan_options command!!!\n");
    #endif

		set_spectral_scan_options_htc(ah_ref);

	} else if(!strcmp(message_type, "end_scan")) {

    #ifdef USE_PRINT
		printk("Received end_scan command!!!\n");
    #endif

		end_spectral_scan_htc(ah_ref);
	} 
	else {

    #ifdef USE_PRINT
		printk(KERN_INFO "Netlink command \"%s\" not executed as no command available...\n", message_type);
    #endif
	}
}
#endif

void exit_server_socket_mechanism(void) {
	printk("Entering: %s\n",__FUNCTION__);

  #ifdef USE_NETLINK
	if (nl_server_sock) {
		netlink_kernel_release(nl_server_sock);
	} 

	nl_server_sock = NULL;
  #endif
}

// Start the spectral scan mode.
void start_spectral_scan_htc(struct ath_hw *ah) {
  #ifdef SCAN_COUNT_AR9002
  int scanCount = 128;
  #else
  int scanCount = 0;
  #endif

	int scanFFTPeriod = 6;
	int scanPeriod = 18;

  #ifdef USE_PRINT
	u32 val;
  #endif

  #ifdef USE_PRINT
	printk("Entering: %s\n", __FUNCTION__);
  #endif

	if (!ah) {
		printk("ath_hw *ah is null... Doing nothing..\n");
    return;
	}

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k Before: %x\n", val);
  #endif

  // TODO: Using the modified from 16-27 instead of 16-23.
	#ifdef USE_AR9002
	REG_RMW_FIELD(ah, AR_PHY_SPECTRAL_SCAN,
	              //AR_PHY_SPECTRAL_SCAN_COUNT_MOD,
	              AR_PHY_SPECTRAL_SCAN_COUNT,
	              scanCount);

    #ifdef USE_PRINT_DEBUG
	  val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
  	printk("ath9k ar_phy_spectral_count: %x\n", val);
    #endif

	#else
	REG_RMW_FIELD(ah, AR_PHY_SPECTRAL_SCAN,
	              AR_PHY_SPECTRAL_SCAN_COUNT,
	              scanCount);

    #ifdef USE_PRINT_DEBUG
  	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	  printk("ath9k ar_phy_spectral_count: %x\n", val);
    #endif

	#endif

	REG_RMW_FIELD(ah, AR_PHY_SPECTRAL_SCAN,
	 	AR_PHY_SPECTRAL_SCAN_FFT_PERIOD, scanFFTPeriod);

  #ifdef USE_PRINT_DEBUG
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_FFT_PERIOD: %x\n", val);
  #endif

	REG_RMW_FIELD(ah, AR_PHY_SPECTRAL_SCAN,
		AR_PHY_SPECTRAL_SCAN_PERIOD, scanPeriod);

  #ifdef USE_PRINT_DEBUG
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_PERIOD: %x\n", val);
  #endif

  REG_SET_BIT(ah, AR_PHY_RADAR_0,
		AR_PHY_RADAR_0_FFT_ENA);

  #ifdef USE_PRINT_DEBUG
	val = REG_READ(ah, AR_PHY_RADAR_0);
	printk("AR_PHY_RADAR_0 AR_PHY_RADAR_0_FFT_ENA: %x\n", val);
  #endif

	/* Enable spectral scan */
	REG_SET_BIT(ah, AR_PHY_SPECTRAL_SCAN,
	            AR_PHY_SPECTRAL_SCAN_ENABLE);
  
  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_ENABLE: %x\n", val);	
  #endif

	/* Activate spectral scan */
	REG_SET_BIT(ah, AR_PHY_SPECTRAL_SCAN,
	            AR_PHY_SPECTRAL_SCAN_ACTIVE);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_ACTIVE: %x\n", val);
  #endif

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k After: %x\n", val);
  #endif
}

// End the spectal scan mode.
void end_spectral_scan_htc(struct ath_hw *ah) {
  #ifdef USE_PRINT
	u32 val;
  #endif

  #ifdef USE_PRINT
	printk("Entering: %s\n", __FUNCTION__);
  #endif

	if (!ah) {
		printk("ath_hw *ah is null... Doing nothing..\n");
    return;
	}

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k Before: %x\n", val);
  #endif

	/* Disable spectral scan */
	REG_CLR_BIT(ah, AR_PHY_SPECTRAL_SCAN,
			AR_PHY_SPECTRAL_SCAN_ENABLE);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_ENABLE: disabled spectral scan: %x\n", val);
  #endif

	/* Clear active scan */
	REG_CLR_BIT(ah, AR_PHY_SPECTRAL_SCAN,
			AR_PHY_SPECTRAL_SCAN_ACTIVE);
  
  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_ACTIVE: disabled spectral scan: %x\n", val);
  #endif

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k After: %x\n", val);
  #endif
}

// Set the predetermined spectral scan options.
void set_spectral_scan_options_htc(struct ath_hw *ah) {
  #ifdef SCAN_COUNT_AR9002
	int scanCount = 128;
  #else
  int scanCount = 0;
  #endif

	int scanFFTPeriod = 6;
	int scanPeriod = 18;

  #ifdef USE_PRINT
	u32 val = 0;
  #endif

  #ifdef USE_PRINT
	printk("Entering: %s\n", __FUNCTION__);
  #endif

	if (!ah) {
		printk("ath_hw *ah is null... Doing nothing..\n");
    return;
	}

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k Before: %x\n", val);
  #endif

	// TODO: Using the modified from 16-27 instead of 16-23.
	#ifdef USE_AR9002
	REG_RMW_FIELD(ah, AR_PHY_SPECTRAL_SCAN,
	              //AR_PHY_SPECTRAL_SCAN_COUNT_MOD,
	              AR_PHY_SPECTRAL_SCAN_COUNT,
	              scanCount);

    #ifdef USE_PRINT
	  val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
  	printk("ath9k ar_phy_spectral_count: %x\n", val);
    #endif

	#else
	REG_RMW_FIELD(ah, AR_PHY_SPECTRAL_SCAN,
	              AR_PHY_SPECTRAL_SCAN_COUNT,
	              scanCount);

    #ifdef USE_PRINT
	  val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	  printk("ath9k ar_phy_spectral_count: %x\n", val);
    #endif

	#endif

	REG_RMW_FIELD(ah, AR_PHY_SPECTRAL_SCAN,
	 	AR_PHY_SPECTRAL_SCAN_FFT_PERIOD, scanFFTPeriod);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_FFT_PERIOD: %x\n", val);
  #endif

	REG_RMW_FIELD(ah, AR_PHY_SPECTRAL_SCAN,
		AR_PHY_SPECTRAL_SCAN_PERIOD, scanPeriod);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_PERIOD: %x\n", val);
  #endif

	// TODO: Current removed as the bit is set by default.
	/*
	REG_SET_BIT(ah, AR_PHY_SPECTRAL_SCAN,
	            AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT: %x\n", val);
  #endif
	*/

	REG_CLR_BIT(ah, AR_PHY_SPECTRAL_SCAN,
	            AR_PHY_SPECTRAL_SCAN_SHORT_PRIORITY);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k[%x] AR_PHY_SPECTRAL_SCAN_SHORT_PRIORITY: %x\n", AR_PHY_SPECTRAL_SCAN, val);
  #endif

	// Trying the disable_radar_tctl_rst bit:
	//REG_SET_BIT(ah, AR_PHY_SPECTRAL_SCAN,
	//	    0x00000004);
	//val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	//printk("ath9k disable_radar_tctl_rst: %x\n", val);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_RADAR_0);
	printk("Initial ath9k AR_PHY_RADAR_0: %x\n", val);
	val = REG_READ(ah, AR_PHY_RADAR_1);
	printk("Initial ath9k AR_PHY_RADAR_1: %x\n", val);
  	val = REG_READ(ah, AR_PHY_RADAR_EXT);
	printk("Initial ath9k AR_PHY_RADAR_EXT: %x\n", val);
  #endif

	#if USE_AR9002
    #ifdef USE_PRINT
  	val = REG_READ(ah, AR_PHY_AGC_CONTROL_TMP);
	  printk("Initial ath9k AR_PHY_AGC_CONTROL_TMP: %x\n", val);
    #endif
	#endif

	/** Manipulate the Radar registers **/
	REG_SET_BIT(ah, AR_PHY_RADAR_0,
		 AR_PHY_RADAR_0_ENA);
  
  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_RADAR_0);
	printk("AR_PHY_RADAR_0 AR_PHY_RADAR_0_ENA: %x\n", val);
  #endif

	REG_SET_BIT(ah, AR_PHY_RADAR_0,
		AR_PHY_RADAR_0_FFT_ENA);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_RADAR_0);
	printk("AR_PHY_RADAR_0 AR_PHY_RADAR_0_FFT_ENA: %x\n", val);
  #endif

	REG_SET_BIT(ah, AR_PHY_RADAR_EXT,
		AR_PHY_RADAR_EXT_ENA);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_RADAR_EXT);
	printk("AR_PHY_RADAR_EXT AR_PHY_RADAR_EXT_ENA: %x\n", val);
  #endif

	REG_SET_BIT(ah, AR_PHY_RADAR_1,
		AR_PHY_RADAR_1_BLOCK_CHECK);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_RADAR_1);
	printk("AR_PHY_RADAR_1 AR_PHY_RADAR_1_BLOCK_CHECK: %x\n", val);
  #endif

	//REG_RMW_FIELD(ah, AR_PHY_RADAR_0,
	//	AR_PHY_RADAR_0_RRSSI, 0x111111);
	//val = REG_READ(ah, AR_PHY_RADAR_0);
	//printk("AR_PHY_RADAR_0 AR_PHY_RADAR_0_RRSSI: %x\n", val);

	//REG_RMW_FIELD(ah, AR_PHY_RADAR_1,
	//	AR_PHY_RADAR_1_BIN_MAX_BW, 0);
	//val = REG_READ(ah, AR_PHY_RADAR_1);
	//printk("AR_PHY_RADAR_1 AR_PHY_RADAR_1_BIN_MAX_BW: %x\n", val);

	//REG_RMW_FIELD(ah, AR_PHY_AGC_CONTROL_TMP,
	//	AR_PHY_AGC_CONTROL_TEST, 0x18);
	//val = REG_READ(ah, AR_PHY_AGC_CONTROL_TMP);
	//printk("AR_PHY_AGC_CONTROL_TMP AR_PHY_AGC_CONTROL_TEST: %x\n", val);

	// Changing radar_lb_dc_cap
	//REG_RMW_FIELD(ah, AR_PHY_RADAR_EXT,
	//	AR_PHY_RADAR_EXT_LB_DC_CAP, 5);
	//val = REG_READ(ah, AR_PHY_RADAR_EXT);
	//printk("AR_PHY_RADAR_EXT AR_PHY_RADAR_EXT_LB_DC_CAP: %x\n", val);

	// Changing radar_dc_pwr_thresh.
	//REG_RMW_FIELD(ah, AR_PHY_RADAR_EXT,
	//	AR_PHY_RADAR_EXT_DC_POWER_THRESH, 127);
	//val = REG_READ(ah, AR_PHY_RADAR_EXT);
	//printk("AR_PHY_RADAR_EXT AR_PHY_RADAR_EXT_DC_POWER_THRESH: %x\n", val);

	// Setting disable_adcsathold.	
	//REG_SET_BIT(ah, AR_PHY_RADAR_EXT,
	//	0x80000000);
	//val = REG_READ(ah, AR_PHY_RADAR_EXT);
	//printk("AR_PHY_RADAR_EXT disable_adcsathold: %x\n", val);

	// Disabling the leaky bucket filter	
	//REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL_TMP,
	//  AR_PHY_AGC_CONTROL_TMP_LEAKY_BKT_ENA);
	//val = REG_READ(ah, AR_PHY_AGC_CONTROL_TMP);
	//printk("AR_PHY_AGC_CONTROL_TMP disable leaky_bucket_enable: %x\n", val);

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_RADAR_0);
	printk("Final ath9k AR_PHY_RADAR_0: %x\n", val);
	val = REG_READ(ah, AR_PHY_RADAR_1);
	printk("Final ath9k AR_PHY_RADAR_1: %x\n", val);
	val = REG_READ(ah, AR_PHY_RADAR_EXT);
	printk("Final ath9k AR_PHY_RADAR_EXT: %x\n", val);
  #endif

	#if USE_AR9002
    #ifdef USE_PRINT
  	val = REG_READ(ah, AR_PHY_AGC_CONTROL_TMP);
	  printk("Final ath9k AR_PHY_AGC_CONTROL_TMP: %x\n", val);
    #endif
	#endif

	/* Is chan info memory capture involved? */
	//REG_SET_BIT(ah, AR_PHY_CHAN_INFO_MEMORY,
	//            AR_PHY_CHAN_INFO_MEMORY_CAPTURE_MASK);

	/* Clear phy chan memory *
	REG_CLR_BIT(ah, AR_PHY_CHAN_INFO_MEMORY,
	            AR_PHY_CHAN_INFO_MEMORY_CAPTURE_MASK);
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k AR_PHY_SPECTRAL_SCAN_ACTIVE: disabled spectral scan: %x\n", val);
	*/

  #ifdef USE_PRINT
	val = REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
	printk("ath9k After: %x\n", val);
  #endif
}

/*
 * Updates the survey statistics and returns the busy time since last
 * update in %, if the measurement duration was long enough for the
 * result to be useful, -1 otherwise.
 */
static int print_survey_stats(struct ath_hw *ah, channel_util_survey *survey_stats)
{
	//struct ath_hw *ah = priv->ah;
	struct ath_common *common = ath9k_hw_common(ah);
	//int pos = ah->curchan - &ah->channels[0];
	//struct survey_info *survey = &sc->survey[pos];
	struct ath_cycle_counters *cc = &common->cc_survey;
	unsigned int div = common->clockrate * 1000;
	int ret = 0;

	printk("In %s, div: %u\n", __FUNCTION__, div);

	if (!ah->curchan)
		return -1;

	if (ah->power_mode == ATH9K_PM_AWAKE)
		ath_hw_cycle_counters_update(common);

	if (cc->cycles > 0) {
		/*
		survey->filled |= SURVEY_INFO_CHANNEL_TIME |
			SURVEY_INFO_CHANNEL_TIME_BUSY |
			SURVEY_INFO_CHANNEL_TIME_RX |
			SURVEY_INFO_CHANNEL_TIME_TX;
		*/
		printk("In %s, channel_time_cycles: %u\n", __FUNCTION__, cc->cycles / div);
		printk("In %s, channel_time_busy: %u\n", __FUNCTION__, cc->rx_busy / div);
		printk("In %s, channel_time_rx: %u\n", __FUNCTION__, cc->rx_frame / div);
		printk("In %s, channel_time_tx: %u\n", __FUNCTION__, cc->tx_frame / div);

    if (cc->cycles > div) {
      survey_stats->cycles = cc->cycles / div;
      survey_stats->rx_busy = cc->rx_busy / div;
      survey_stats->rx_frame = cc->rx_frame / div;
      survey_stats->tx_frame = cc->tx_frame / div;

      survey_stats->div = div;
		  survey_stats->rx_busy_per = (cc->rx_busy / div) * 100 / (cc->cycles / div);
      survey_stats->rx_frame_per = (cc->rx_frame / div) * 100 / (cc->cycles / div); 
    }
	} else {
		printk("In %s, Could not calculate stats...\n", __FUNCTION__);
	}

	if (cc->cycles < div)
		return -1;

	if (cc->cycles > 0) {
		ret = (cc->rx_busy / div) * 100 / (cc->cycles / div);
	}

	printk("In %s, ret: %u\n", __FUNCTION__, ret);
	memset(cc, 0, sizeof(*cc));

	//ath_update_survey_nf(sc, pos);

	return ret;
}

int get_survey_fft(struct ath_hw *ah, channel_util_survey *survey_stats)
{
	struct ath_common *common = ath9k_hw_common(ah);
	unsigned long flags;

	printk("In %s\n", __FUNCTION__);

	spin_lock_irqsave(&common->cc_lock, flags);
	print_survey_stats(ah, survey_stats);
	spin_unlock_irqrestore(&common->cc_lock, flags);

	return 0;
}

// Debug functions.
void print_spectrum_events(event_counter* spectrum_events) {
	printk("$$ rxstatus_neq_zero               : %llu\n", spectrum_events->rxstatus_neq_zero);

	printk("$$ phy_errors                      : %llu\n", spectrum_events->phy_errors);
	printk("$$ phy_error_false_radar_ext       : %llu\n", spectrum_events->phy_error_false_radar_ext);
	printk("$$ phy_error_radar                 : %llu\n", spectrum_events->phy_error_radar);

	printk("$$ phy_error_radar_invalid_datalen : %llu\n", spectrum_events->phy_error_radar_invalid_datalen);
	printk("$$ phy_error_radar_bogus_bwinfo    : %llu\n", spectrum_events->phy_error_radar_bogus_bwinfo);
	printk("$$ phy_error_radar_bogus_rssi      : %llu\n", spectrum_events->phy_error_radar_bogus_rssi);
	printk("$$ phy_error_radar_bogus_duration  : %llu\n", spectrum_events->phy_error_radar_bogus_duration);
	printk("$$ phy_error_radar_invalid_datalen : %llu\n", spectrum_events->phy_error_radar_invalid_datalen);

	printk("$$ ffts_used                       : %llu\n", spectrum_events->ffts_used);
	printk("$$ ffts_skipped                    : %llu\n", spectrum_events->ffts_skipped);

	printk("\n");
}

void print_buffer(unsigned char* start_ptr, int buf_len) {
  int j = 0;

	for (j = 0; j < buf_len; j++) {
		printk("%u ",  start_ptr[j]);
	}
}

/**** Done ****/
