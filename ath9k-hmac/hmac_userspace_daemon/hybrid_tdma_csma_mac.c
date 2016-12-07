/*
 * Userland part of the hybrid TDMA/CSMA MAC processor which is responsible for sending the configuration at
 * the beginning of each time slot to the ATH9k WiFi driver using Netlink.
 *
 * build with: make all
 *
 * @authors S Zehl, A. Zubow
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <unistd.h>
#include <time.h>
#ifdef _EVENT_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/socket.h>
#include <stdlib.h>
#define CPTCFG_ATH9K_TID_SLEEPING
#include "../backports-3.12.8-1/include/uapi/linux/nl80211.h"
#include "iw.h"
//nclude "../backports-3.12.8-1/include/uapi/linux/nl80211.h"
#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <string>
#include <iostream>
#include <zmq.hpp>
#include <string>
#include <sstream>
#include <vector>


// struct to be passed via netlink to ath9k driver
struct tid_sleep_tuple
{
    char mac[6]; // destination MAC address
    char mask; // TID mask
};

int mDebug = 0;

int isRunning = 1;

// Used by local controller for communication with mac processor
int LOCAL_MAC_PROCESSOR_CTRL_PORT = 1217;

// variables set at start-up
long slotDuration = 10000; // mus
int slotsPerFrame = 10; // e.g. 10
char * interface; // e.g. wifi0
char * configuration; // e.g. 1,mac_addr,tid_map;2,mac_addr,tid_map

// updated at runtime
std::string *schedule_per_slot = NULL;

struct nl80211_state state;

// internal state for keeping slotting time aligned
long oldtime_l = 0;
int event_is_persistent;
const int estimatePrecisionEveryNslots = 1000;
int times[estimatePrecisionEveryNslots];
int cnt = 0;
long gl_clock = 0; // global clock w/ step size of slotDuration
long slotCnt = 0; // total slot counter
long frameCnt = 0; // total frame counter

///////////////////////////////////////////////////////////////////////////////

/** helper functions for string manipulation */
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

/** helper functions for string manipulation */
std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

/**
 * Called at start-up or in case of reconfiguration.
 */
void updateSchedule(std::string &msg) {

	// init
	for (int i=0; i<slotsPerFrame; i++) {
		schedule_per_slot[i] = "";
	}

	// assign configuration to each slot
	std::vector<std::string> msg_parts = split(msg, '#');

	for (int k=0; k<msg_parts.size(); k++) {
		std::vector<std::string> entry = split(msg_parts[k], ',');

		int slot_id = atoi(entry[0].c_str());
		const char * mac_addr = entry[1].c_str();
		int tid_mask = atoi(entry[2].c_str());

		std::stringstream ss;
		if (schedule_per_slot[slot_id].empty()) {
			ss << mac_addr << "," << entry[2].c_str();
		} else {
			ss << schedule_per_slot[slot_id] << "#" << mac_addr << "," << entry[2].c_str();
		}

		schedule_per_slot[slot_id] = ss.str();
	}
}

/** Worker thread responsible for receiving new configuration updates */
void *worker_routine (void *arg)
{
    std::cout << "Worker routine started ... ready to receive new configuration messages via ZMQ socket." << std::endl;

    zmq::context_t *context = (zmq::context_t *) arg;
    zmq::socket_t socket (*context, ZMQ_REP);

	std::stringstream ss;
	ss << "tcp://*:" << LOCAL_MAC_PROCESSOR_CTRL_PORT;
    socket.bind (ss.str().c_str());
    //socket.bind ("ipc:///tmp/localmacprocessor");

    while (true) {
        zmq::message_t request;

        //  Wait for next request from client
        socket.recv (&request);

        std::string msg = std::string(static_cast<char*>(request.data()), request.size());
        std::cout << "Received new configuration update: " << msg << std::endl;

		if (msg.find("TERMINATE") == 0) {
			// shutdown process
			isRunning = 0;
		} else {
			// update slot schedule
			updateSchedule(msg);
		}

        sleep (0.1);

        //  Send ACK to client
        zmq::message_t reply(2);
        memcpy ((void *) reply.data(), "OK", 2);
        socket.send(reply);
	if(isRunning == 0)
	{
		socket.close();
		break;
	}
    }
    return 0;
}

/** netlink error handling */
static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	int *ret = (int*) arg;
	*ret = err->error;

	printf("->CFG80211 returns: error: No:%d, %s\n",err->error, strerror((-1)*err->error));
	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = (int*) arg;
	*ret = 0;
	return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = (int*) arg;
	*ret = 0;
	return NL_STOP;
}

static int send_nl_msg(std::string& schedule)
{
    if (mDebug == 1) {
		std::cout << "Send schedule via netlink to ath9k driver: " << schedule << std::endl;
	}

	std::vector<std::string> tuples = split(schedule, '#');

	struct nl_cb *cb;
	struct nl_cb *s_cb;
	struct nl_msg *msg;
	signed long long devidx = 0;
	int err = 0;
	devidx = if_nametoindex(interface);
	
	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message\n");
		return 2;
	}
	
	cb = nl_cb_alloc(NL_CB_DEFAULT);
	s_cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb || !s_cb) {
		fprintf(stderr, "failed to allocate netlink callbacks\n");
		err = 2;
		nlmsg_free(msg);
		return err;
	}


	// create NetLink message
	genlmsg_put(msg, 0, 0, state.nl80211_id, 0, 0, NL80211_CMD_SET_TID_SLEEP, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);

	// update tids_tuple with current slot schedule
    uint8_t mac_u8[6];
    struct tid_sleep_tuple tids_tuple[tuples.size()];
	for (int k=0; k<tuples.size(); k++) {
		std::vector<std::string> tuple = split(tuples[k], ',');
        sscanf(tuple[0].c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac_u8[0], &mac_u8[1], &mac_u8[2], &mac_u8[3], &mac_u8[4], &mac_u8[5]);
		int tid_mask = atoi(tuple[1].c_str());
        
        for (int zz=0; zz<6; zz++) {
            tids_tuple[k].mac[zz] = mac_u8[zz];
        }
        tids_tuple[k].mask = tid_mask;
	}

    NLA_PUT(msg, NL80211_ATTR_TID_SLEEP, sizeof(struct tid_sleep_tuple)*tuples.size(), (const void *) &tids_tuple[0]);
	nl_socket_set_cb(state.nl_sock, s_cb);

	// send message
	err = nl_send_auto_complete(state.nl_sock, msg);
	if (err < 0)
	{
		nl_cb_put(cb);
		nlmsg_free(msg);
		return err;
	}

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

	while (err > 0) {
		nl_recvmsgs(state.nl_sock, cb);
	}

	out:
		nl_cb_put(cb);
	out_free_msg:
		nlmsg_free(msg);
		return err;
	nla_put_failure:
		fprintf(stderr, "building message failed\n");
		return 2;
}

/** initialize netlink */
static int nl80211_init(struct nl80211_state *state)
{
	int err;

	printf("nl80211 init called v2\n");

	state->nl_sock = nl_socket_alloc();
	if (!state->nl_sock) {
		fprintf(stderr, "Failed to allocate netlink socket.\n");
		return -ENOMEM;
	}

	nl_socket_set_buffer_size(state->nl_sock, 8192, 8192);

	if (genl_connect(state->nl_sock)) {
		fprintf(stderr, "Failed to connect to generic netlink.\n");
		err = -ENOLINK;
		nl_socket_free(state->nl_sock);
		return err;
	}

	state->nl80211_id = genl_ctrl_resolve(state->nl_sock, "nl80211");
	if (state->nl80211_id < 0) {
		fprintf(stderr, "nl80211 not found.\n");
		err = -ENOENT;
		nl_socket_free(state->nl_sock);
		return err;
	}

	return 0;

    out_handle_destroy:
		nl_socket_free(state->nl_sock);

	return err;
}

/** timer callback called at the beginning of each new slot */
static void
timeout_cb(evutil_socket_t fd, short event, void *arg)
{
	struct timeval newtime, difference;
	struct event *timeout = (struct event *)arg;
	long error = 0;

	if (isRunning == 0) {
		printf("Terminating ...\n");
		return;
	}

	evutil_gettimeofday(&newtime, NULL);
	long newtime_l = 1000000 * newtime.tv_sec + newtime.tv_usec;

	if (gl_clock == 0) {
		gl_clock = (newtime_l / slotDuration) * slotDuration;
		slotCnt = 0;
		frameCnt = 0;
	} else {
		gl_clock = gl_clock + (slotDuration % 1000000);
		slotCnt++;
		frameCnt = slotCnt / slotsPerFrame;
	}
	// current frame number
	long relFrameNum = slotCnt % slotsPerFrame;

	// estimate slot scheduling precision
	if(cnt < estimatePrecisionEveryNslots) {
		times[cnt] = newtime_l;
		cnt++;
	} else if(cnt == estimatePrecisionEveryNslots) {
		double sum = 0;
		for(int i = 1; i < estimatePrecisionEveryNslots; i++) {
			sum += times[i] - times[i-1];
		}
		sum = sum / (estimatePrecisionEveryNslots-1);

		printf("Average slot duration: %.2f\n", sum);
		cnt = 0;
	}

	if (!event_is_persistent) {
		struct timeval tv;
		evutil_timerclear(&tv);

		if (oldtime_l != 0) {
			error = 2 * (newtime_l - gl_clock);
		} 

		tv.tv_sec = slotDuration / 1000000;
		tv.tv_usec = (slotDuration % 1000000) - error;

		event_add(timeout, &tv);
		oldtime_l = newtime_l;

		send_nl_msg(schedule_per_slot[relFrameNum]);
	}
}

/** main entry point */
int
main(int argc, char **argv)
{
	struct event timeout;
	struct timeval tv;
	struct event_base *base;
	int flags;
	int err;
	char * inter_ptr;
	char * conf_ptr;
	int opt = 0;
	char *mac = NULL;
	char *guard_frames = NULL;
	char *best_effort = NULL;
	char *voice = NULL;
	char *pt;
	interface = '\0';
	configuration = '\0';

	while ((opt = getopt(argc, argv, "d:i:f:n:c:")) != -1) {
		switch(opt) {
		case 'd':
			mDebug = atoi(optarg);
			printf("\nDebug = %d", mDebug);
			break;
		case 'i':
			inter_ptr = optarg;
		  	interface = (char *) malloc(strlen(inter_ptr));
			strcpy(interface, inter_ptr);
		   	printf("\nInterface = %s", interface);
			break;
		case 'f':
			slotDuration = atoi(optarg);
			printf("\nSlot Duration = %lu", slotDuration);
			break;
		case 'n':
			slotsPerFrame = atoi(optarg);
			printf("\nTotal number of slots in frame = %d", slotsPerFrame);
			break;
		case 'c':
			conf_ptr = optarg;
		  	configuration = (char *) malloc(strlen(conf_ptr));
			strcpy(configuration, conf_ptr);
			printf("\nConfig = %s", configuration);
			break;
		case '?':
			printf("Usage: ./hybrid_tdma_csma_mac -i wifi0 -f 20000 -n 10\n");
			break;
		}
	 }

	// init schedule
	schedule_per_slot = new std::string[slotsPerFrame];

	// update schedule
	std::string msg(configuration);
	updateSchedule(msg);

	if (interface == NULL) {
		printf("Error no interface supplied. Usage: ./hybrid_tdma_csma_mac -i wifi0 -f 20000 -n 10\n");
		return -1;
	}

	printf("Using init schedule w/:\n");
	
	for(int i=0; i<10; i++) {
		printf("#%d: %s, ", i, schedule_per_slot[i].c_str());
	}
	
    //  Prepare our context and sockets
    zmq::context_t context (1);

    try {
		pthread_t worker;
		pthread_create (&worker, NULL, worker_routine, (void *) &context);
    } catch (const std::exception& ex)  {
		std::cerr << "Error setting up ZMQ: " << ex.what() << std::endl;
		return -1;
    } catch (...) {
		std::cerr << "Other strange error!!! " << std::endl;
		return -1;
    }

	err = nl80211_init(&state);
	
	event_is_persistent = 0;
	flags = 0;
	
	/* Initalize the event library */
	base = event_base_new();

	/* Initalize one event */
	event_assign(&timeout, base, -1, flags, timeout_cb, (void*) &timeout);

	evutil_timerclear(&tv);

	/* align schedule to seconds */
	struct timeval curTime;
	evutil_gettimeofday(&curTime, NULL);

	int mus = 1000000 - curTime.tv_usec;
	tv.tv_sec = 1;
	tv.tv_usec = mus;
	event_add(&timeout, &tv);

	event_base_dispatch(base);

	return (0);
}

