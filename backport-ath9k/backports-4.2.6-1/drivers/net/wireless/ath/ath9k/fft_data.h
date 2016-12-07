/**** Added for spectrum analysis ****/

#ifndef FFT_DATA_H
#define FFT_DATA_H

#include<linux/workqueue.h>

#include "hw.h"

//#define USE_NETLINK 1

//XXX TODO replace this with user configured dimension to be written in via debugfis
//       Otherwise there may be some race issues. e.g, the data variable.
// Keep the data structure and constants in sync with client side fft data structure code.
#define FFT_ARRAY_LENGTH_S20 60 // Adding two more bytes as sometimes the data seems to be shifted by two bytes.
#define FFT_ARRAY_LENGTH_HT40 135

// TODO: Currently disabled support for 40 Mhz to pack more data per packet.
#define MAXLEN   67 // 140 // Should be at equal or more than FFT_ARRAY_LENGTH.
#define MAXCOUNT 14 // 8 // The number of fft samples per FFT report sent to client,
#define MIN_DATALEN 56 // Minimum offset in packet header for getting the FFTs.
#define NUM_REPORTS 30

#define DUMP_BUFFER_SIZE 200;

#define UPPER_MASK 0xf0
#define LOWER_MASK 0x0f

#define PRI_CH_RADAR_FOUND 0x01
#define EXT_CH_RADAR_FOUND 0x02

// Changing code for different cards.
#define SCAN_COUNT_AR9002 1

// Debug
#define DBG_CHANNELS 14

// Debug structure for event counters.
typedef struct event_counter {
	u64 rxstatus_neq_zero;
  
	u64 phy_errors;
	u64 phy_error_false_radar_ext;
	u64 phy_error_radar;
  
	u64 phy_error_radar_invalid_datalen;
	u64 phy_error_radar_bogus_bwinfo;
	u64 phy_error_radar_bogus_rssi;
	u64 phy_error_radar_bogus_duration;
	u64 phy_error_radar_lt_min_datalen;
  
	u64 ffts_used;
	u64 ffts_skipped;

} event_counter;

typedef struct channel_util_survey {
	u32 cycles;
	u32 rx_busy;
	u32 rx_frame;
	u32 tx_frame;

  u32 div;

  u32 rx_busy_per;
  u32 rx_frame_per;
} channel_util_survey;

typedef struct spectrum_fft {
	u16 freq;			// Channel of operation
	u64 ts;				// Time of measurement
	//u64 ts1;			// Kernel timestamp in jiffies
	u32 width;			// Width
	int8_t rssi;			// Rssi of the samples
	u16 sample_bin_cnt;		// Number of FFT bins

	// Debug variables
	//u8 rssi_type;			// The type of rssi: 1,2,3 (pri, ext, both)
	u8 start_index;			// The type of rssi: 1,2,3 (pri, ext, both)
  
	u16 datalen;			// The datalen offset in the original packet header.
	//__be16 datalen_orig;		// The datalen offset in the original packet header without conversion.

	unsigned char samples[MAXLEN];	// Stores the energy valyes of the FFT sample.
} spectrum_fft;

// IMP: The total length of the data structure should be less than 
// the size of a UDP packet (i.e., 1500 bytes).
typedef struct fft_data {
	spectrum_fft samples[MAXCOUNT];
	int count;
	u64 seq_num;
} fft_data;

#define FFT_PACKET_SIZE (sizeof(fft_data) + 0)

typedef struct fft_report {
	fft_data fft_stats;

	// Store the UDP packet related information.
	struct sockaddr_in addr;
	struct msghdr msg;
	struct iovec iov;

	// Store the contents of the packets to be sent to the user.
	unsigned char data[FFT_PACKET_SIZE];

	struct work_struct work;
} fft_report;

typedef struct {
  fft_report fft_reports[NUM_REPORTS];	

  // Current report index.
  int report_count;

  // No. of tranmissions complete.
  int transmit_count;
  int trans_remaining_count;
} fft_report_data;

// These functions handle the FFTs obtained from the chipset.
void update_print_fft_direct(spectrum_fft* fft_input, unsigned char* ptr, u16 len, s16 nf_corr);
void update_print_fft_data(spectrum_fft* fft_input, unsigned char* ptr, u16 len, s16 nf_corr, event_counter* spectrum_events);

// Initialize the state related to the spectal scan.
void init_spectral_scan_state(struct ath_hw *ah);
void exit_spectral_scan_state(void);

// These functions are meant for the data reporting mechanism to the user space
// using UDP based packets.
void init_data_report_mechanism(void);
void report_workqueue_handler(struct work_struct *work);
void tx_report_contents(void);
void exit_data_report_mechanism(void);

// UDP Server Socket mechanism to received commands from the userspace client to manage driver state.
int init_server_socket_mechanism(void);
#ifdef USE_NETLINK
void recv_process_client_command(struct sk_buff *skb);
#endif
void exit_server_socket_mechanism(void);

// Manipulating spectrum scan bitmap functions.
void set_spectral_scan_options_htc(struct ath_hw *ah); // Set the predetermined spectral scan options.
void start_spectral_scan_htc(struct ath_hw *ah); // Start the spectral scan.
void end_spectral_scan_htc(struct ath_hw *ah); // End the spectral scan.

// Channel manipulation functions.
void caliberate_channel(struct ath_hw *ah);
void set_current_noisefloor(s16 curr_nf);
void spectral_channel_change(int pos);

// Channel utilization related functions.
//int get_survey_fft(struct ath_hw *ah);
int get_survey_fft(struct ath_hw *ah, channel_util_survey *survey_stats);

// Debug functions.
void print_spectrum_events(event_counter* spectrum_events);
void print_buffer(unsigned char* start_ptr, int buf_len);

#endif /* FFT_DATA_H */

/**** Done ****/
