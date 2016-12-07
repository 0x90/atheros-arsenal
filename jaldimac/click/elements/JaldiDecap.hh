#ifndef CLICK_JALDIDECAP_HH
#define CLICK_JALDIDECAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

JaldiDecap(DEST)

=s jaldi

decapsulates packets which are wrapped in a Jaldi header

=d

Decapsulates Jaldi frames into IP packets. Jaldi control messages, for which
decapsulation is not meaningful, are placed on output 0. IP packets are placed
on output 1. Jaldi frames which could not be decapsulated for whatever reason
(for example, they have an invalid type, or they failed the CRC check) are
placed on output 2 if that output is connected.

DEST an optional parameter which specifies the destination station id we are
interested in. If DEST is not supplied, JaldiDecap will decapsulate all incoming
Jaldi frames. If DEST is supplied, JaldiDecap will only decapsulate incoming
Jaldi frames which are either destined for station DEST, or are destined for
station 0 (broadcast). Frames which are not decapsulated because of these rules
are placed on output 2 if that output is connected.

This element is push only.

=a

JaldiEncap */

class JaldiDecap : public Element { public:

    JaldiDecap();
    ~JaldiDecap();

    const char* class_name() const  { return "JaldiDecap"; }
    const char* port_count() const  { return "1/2-3"; }
    const char* processing() const  { return PUSH; }
    const char* flow_code() const   { return COMPLETE_FLOW; }

    int configure(Vector<String>&, ErrorHandler*);
    bool can_live_reconfigure() const   { return true; }

    void push(int, Packet*);

    private:
      static const int in_port = 0;
      static const int out_port_control = 0;
      static const int out_port_data = 1;
      static const int out_port_bad = 2;

      bool should_filter_by_dest;
      uint8_t dest_id;
};

CLICK_ENDDECLS
#endif
