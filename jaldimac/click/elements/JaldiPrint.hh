#ifndef CLICK_JALDIPRINT_HH
#define CLICK_JALDIPRINT_HH
#include <click/element.hh>
#include "Frame.hh"
CLICK_DECLS

/*
=c

JaldiPrint(CONTENTS)

=s jaldi

prints Jaldi frames

=d

Prints Jaldi frames received on its only (agnostic) input and then
sends them out through its only (agnostic) output.

CONTENTS determines if the payload of the frame will be printed.
If not present, defaults to false.

=a

JaldiEncap, JaldiDecap */

class JaldiPrint : public Element { public:

    JaldiPrint();
    ~JaldiPrint();

    const char* class_name() const  { return "JaldiPrint"; }
    const char* port_count() const  { return "1/1"; }
    const char* processing() const  { return AGNOSTIC; }
    const char* flow_code() const   { return COMPLETE_FLOW; }

    int configure(Vector<String>&, ErrorHandler*);
    bool can_live_reconfigure() const   { return true; }

    void show_raw_payload(const jaldimac::Frame*);
    Packet* action(Packet* p);
    void push(int, Packet*);
    Packet* pull(int);

  private:
    static const int in_port = 0;
    static const int out_port = 0;

    bool show_contents;
};

CLICK_ENDDECLS
#endif
