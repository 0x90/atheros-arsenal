#ifndef CLICK_JALDIGATE_HH
#define CLICK_JALDIGATE_HH
#include <click/element.hh>
#include "Frame.hh"
CLICK_DECLS

/*
=c

JaldiGate(ID)

=s jaldi

sends Jaldi frames in response to control messages

=d

JaldiGate observes incoming control messages and sends data queued on its
inputs to its output when requested by the master. It's also responsible for
sending requests to the master when more data is queued than can be sent in the
current round. JaldiGate is, in some sense, the equivalent of JaldiScheduler
for stations.

ID is the station ID of this station.

JaldiGate has at least 3 inputs. Input 0 (push) is for control traffic and
input 1 (pull) is for bulk traffic. Inputs 2 and above (pull) are for VoIP
traffic; there should be as many VoIP inputs as there are flows that may fit in
a VoIP slot, plus one for any excess VoIP flows that will have to be sent with
bulk data. Everything arriving on the inputs should be encapsulated in Jaldi
frames, and all pull inputs should be connected to JaldiQueues.

There is one push output (though a second push output may be connected to
receive erroneous packets). 

=a

JaldiGate */

class JaldiQueue;

class JaldiGate : public Element { public:

    JaldiGate();
    ~JaldiGate();

    const char* class_name() const  { return "JaldiGate"; }
    const char* port_count() const  { return "3-/1-2"; }
    const char* processing() const  { return "hl/h"; }
    const char* flow_code() const   { return COMPLETE_FLOW; }

    int configure(Vector<String>&, ErrorHandler*);
    int initialize(ErrorHandler*);
    bool can_live_reconfigure() const   { return true; }
    void take_state(Element*, ErrorHandler*);

    WritablePacket* make_request_frame();

    void push(int, Packet*);

  private:
    static const int in_port_control = 0;
    static const int in_port_bulk = 1;
    static const int in_port_voip_first = 2;
    static const int in_port_voip_overflow = in_port_voip_first + jaldimac::FLOWS_PER_VOIP_SLOT;
    static const int out_port = 0;
    static const int out_port_bad = 1;

    JaldiQueue* bulk_queue;
    JaldiQueue* voip_queues[jaldimac::FLOWS_PER_VOIP_SLOT];
    JaldiQueue* voip_overflow_queue;
    bool outstanding_requests;
    uint32_t bulk_requested_bytes;
    uint8_t voip_requested_flows;
    uint8_t station_id;
};

CLICK_ENDDECLS
#endif
