#ifndef CLICK_JALDIVOIPDEMUX_HH
#define CLICK_JALDIVOIPDEMUX_HH
#include <click/element.hh>
#include "Frame.hh"
CLICK_DECLS

/*
=c

JaldiVoIPDemux(TIMEOUT)

=s jaldi

demultiplexes VoIP flows

=d

JaldiVoIPDemux demultiplexes a stream of incoming VoIP packets into individual
flows. It identifies flows using a combination of destination IP address and
port. It only tracks a limited number of flows at a time; additional flows are
all directed to an "overflow" output. Since flows may start and stop at any
time, JaldiVoIPDemux will forget about flows it has not received a packet for
in TIMEOUT seconds, freeing the corresponding output to be used by another
flow.

The first and only input, which is push, receives IP packets which are presumed
to have been classified as VoIP by an upstream classifier.

The outputs are all push. The number of outputs is equal to the number of flows
that can fit into a VoIP slot, plus an extra overflow output and an optional
additoinal port for bad packets. Though it is not required, it makes sense that
each output would be connected to a short, drop-front queue. (Since stale VoIP
packets are essentially useless.)

=a

JaldiEncap, JaldiDecap, JaldiGate */

struct in_addr;

class JaldiVoIPDemux : public Element { public:

    JaldiVoIPDemux();
    ~JaldiVoIPDemux();

    const char* class_name() const  { return "JaldiVoIPDemux"; }
    const char* port_count() const  { return "1/1-"; }
    const char* processing() const  { return PUSH; }
    const char* flow_code() const   { return COMPLETE_FLOW; }

    int configure(Vector<String>&, ErrorHandler*);
    bool can_live_reconfigure() const   { return true; }

    void push(int, Packet*);

    private:
      static const int in_port = 0;
      static const int out_port_voip_overflow = jaldimac::FLOWS_PER_VOIP_SLOT;
      static const int out_port_bad = jaldimac::FLOWS_PER_VOIP_SLOT + 1;

      uint32_t timeout_s;
      in_addr output_ip[jaldimac::FLOWS_PER_VOIP_SLOT];
      uint16_t output_port[jaldimac::FLOWS_PER_VOIP_SLOT];
      time_t output_last_seen[jaldimac::FLOWS_PER_VOIP_SLOT];
};

CLICK_ENDDECLS
#endif
