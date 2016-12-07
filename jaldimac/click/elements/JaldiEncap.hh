#ifndef CLICK_JALDIENCAP_HH
#define CLICK_JALDIENCAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

JaldiEncap(TYPE, SRC, DEST)

=s jaldi

encapsulates packets in Jaldi header

=d

Encapsulates each packet in the Jaldi header specified by its arguments.

TYPE may be one of: BULK_FRAME, VOIP_FRAME, REQUEST_FRAME, CONTENTION_SLOT,
VOIP_SLOT, TRANSMIT_SLOT, ROUND_COMPLETE_MESSAGE, DELAY_MESSAGE, or
BITRATE_MESSAGE.

SRC is the station identifier of the sending station.

DEST is the station identifier of the station the Jaldi frame is intended for.

Successfully encapsulated packets are sent to the first output (output 0).
Certain erroneous packets may be dropped by JaldiEncap - in particular, packets
which have a size larger than the limit of the Jaldi frame length field will be
dropped. If the second output (output 1) is connected, such packets will be
sent there instead of being dropped. Although the first input and first
output are agnostic, the second output is always push.

=e

Encapsulate packets in a Jaldi header with type BULK_FRAME,
destination station 2:

  JaldiEncap(BULK_FRAME, 2)

=a

JaldiDecap */

class JaldiEncap : public Element { public:

    JaldiEncap();
    ~JaldiEncap();

    const char* class_name() const  { return "JaldiEncap"; }
    const char* port_count() const  { return "1/1-2"; }
    const char* processing() const  { return PROCESSING_A_AH; }
    const char* flow_code() const   { return COMPLETE_FLOW; }

    int configure(Vector<String>&, ErrorHandler*);
    bool can_live_reconfigure() const   { return true; }
    void take_state(Element*, ErrorHandler*);

    Packet* action(Packet* p);
    void push(int, Packet*);
    Packet* pull(int);

  private:
    static const int in_port = 0;
    static const int out_port = 0;
    static const int out_port_bad = 1;

    uint8_t src_id;
    uint8_t dest_id;
    uint8_t type;
    uint32_t seq;

};

CLICK_ENDDECLS
#endif
