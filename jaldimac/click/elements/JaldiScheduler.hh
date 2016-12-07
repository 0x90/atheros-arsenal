#ifndef CLICK_JALDISCHEDULER_HH
#define CLICK_JALDISCHEDULER_HH
#include <click/element.hh>
#include "Frame.hh"
CLICK_DECLS

/*
=c

JaldiScheduler(CSONLYRATELIMIT)

=s jaldi

constructs a Jaldi round layout from incoming packets and requests

=d

JaldiScheduler constructs a Jaldi round layout (expressed in the form of a
sequence of Jaldi frames that direct the driver, which actually implements the
requested behavior) from incoming packets from the Internet and incoming
requests from stations.

In a complete JaldiMAC implementation, JaldiScheduler would have a STATIONS
parameter indicating the number of stations it must serve, and it would
calculate port numbers dynamically from that. For now, STATIONS is hardcoded.

Input 0 (push) is for control Jaldi frames, coming from either stations (e.g.
REQUEST_FRAME) or from the driver. (e.g. ROUND_COMPLETE_MESSAGE) Input 1 (push)
is a second input for control Jaldi frames for use by other elements; in
particular, this input is intended to be used by an InfiniteSource or similar
to jumpstart the scheduling process by sending a single initial
ROUND_COMPLETE_MESSAGE. Inputs 2 thru STATIONS + 1 (pull) are for bulk Jaldi
frames destined for each station.  VoIP traffic destined for the stations is
not handled by the scheduler at all; instead, it is inserted dynamically (by
another element) as soon as the traffic arrives. JaldiScheduler has one push
output (though a second push output may be connected to receive erroneous
packets).  Everything arriving on the inputs should be encapsulated in Jaldi
frames.

The CSONLYRATELIMIT parameter, if specified, indicates the minimum time between
contention-slot-only rounds in microseconds. (i.e., rounds which contain
neither data from upstream nor requests from the stations) If this parameter is
not specified, a reasonable default is chosen.

=a

JaldiGate */

class JaldiQueue;

class JaldiScheduler : public Element { public:

    JaldiScheduler();
    ~JaldiScheduler();

    const char* class_name() const  { return "JaldiScheduler"; }
    const char* port_count() const  { return "2-/1-2"; }
    const char* processing() const  { return "hhl/h"; }
    const char* flow_code() const   { return COMPLETE_FLOW; }

    int configure(Vector<String>&, ErrorHandler*);
    int initialize(ErrorHandler*);
    bool can_live_reconfigure() const   { return true; }
    void take_state(Element*, ErrorHandler*);

    void run_timer(Timer*);
    void push(int, Packet*);

  private:
    void received_round_complete_message();
    bool have_data_or_requests();
    void count_upstream();
    bool try_to_allocate_voip_request(unsigned, unsigned&);
    void allocate_voip_to_no_one(unsigned);
    void compute_fair_allocation();
    void generate_layout();

    static const int in_port_control = 0;
    static const int in_port_control_secondary = 1;
    static const int in_port_bulk_first = 2;
    static const int out_port = 0;
    static const int out_port_bad = 1;

    JaldiQueue* bulk_queues[jaldimac::STATION_COUNT];

    uint32_t bulk_requested_bytes[jaldimac::STATION_COUNT];
    uint8_t voip_requested_flows[jaldimac::STATION_COUNT];
    uint32_t bulk_upstream_bytes[jaldimac::STATION_COUNT];

    bool granted_voip;
    uint8_t voip_granted_by_station[jaldimac::STATION_COUNT];
    jaldimac::VoIPSlotPayload voip_granted;
    uint32_t bulk_granted_bytes[jaldimac::STATION_COUNT];
    uint32_t bulk_granted_upstream_bytes[jaldimac::STATION_COUNT];

    uint32_t rate_limit_distance_us;
    timeval rate_limit_until;
    Timer timer;
};

CLICK_ENDDECLS
#endif
