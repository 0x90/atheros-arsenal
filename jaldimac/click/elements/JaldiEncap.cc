/*
 * JaldiEncap.{cc,hh} -- encapsulates packet in Jaldi header
 */

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <click/config.h>
#include "JaldiEncap.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "Frame.hh"

using namespace jaldimac;

CLICK_DECLS

JaldiEncap::JaldiEncap()
{
}

JaldiEncap::~JaldiEncap()
{
}

int JaldiEncap::configure(Vector<String>& conf, ErrorHandler* errh)
{
    String name_of_type;

    // Parse configuration parameters
    if (cp_va_kparse(conf, this, errh,
             "TYPE", cpkP+cpkM, cpString, &name_of_type,
             "SRC", cpkP+cpkM, cpByte, &src_id,
             "DEST", cpkP+cpkM, cpByte, &dest_id,
             cpEnd) < 0)
        return -1;

    // Convert TYPE field from a string to the appropriate code
    if (name_of_type.equals("BULK_FRAME", -1))
        type = BULK_FRAME;
    else if (name_of_type.equals("VOIP_FRAME", -1))
        type = VOIP_FRAME;
    else if (name_of_type.equals("REQUEST_FRAME", -1))
        type = REQUEST_FRAME;
    else if (name_of_type.equals("CONTENTION_SLOT", -1))
        type = CONTENTION_SLOT;
    else if (name_of_type.equals("VOIP_SLOT", -1))
        type = VOIP_SLOT;
    else if (name_of_type.equals("TRANSMIT_SLOT", -1))
        type = TRANSMIT_SLOT;
    else if (name_of_type.equals("BITRATE_MESSAGE", -1))
        type = BITRATE_MESSAGE;
    else if (name_of_type.equals("ROUND_COMPLETE_MESSAGE", -1))
        type = ROUND_COMPLETE_MESSAGE;
    else if (name_of_type.equals("DELAY_MESSAGE", -1))
        type = DELAY_MESSAGE;
    else
    {
        errh->error("invalid Jaldi frame type: %s", name_of_type.c_str());
        return -1;
    }

    // Initialize the sequence number to 0
    seq = 0;

    return 0;
}

void JaldiEncap::take_state(Element* old, ErrorHandler*)
{
    JaldiEncap* oldJE = (JaldiEncap*) old->cast("JaldiEncap");

    if (oldJE)
        seq = oldJE->seq;
}

Packet* JaldiEncap::action(Packet* p)
{
    // Remember the "real" length of this packet
    uint32_t length = p->length();

    // If the packet's too long, kill it
    if (length > UINT32_MAX)
    {
        checked_output_push(out_port_bad, p);
        return NULL;
    }

    // Add space for Jaldi frame header and footer to packet
    WritablePacket* p0 = p->push(Frame::header_size);
    WritablePacket* p1 = p0->put(Frame::footer_size);

    // Create header
    Frame* f = (Frame*) p1->data();
    f->initialize();
    f->dest_id = dest_id;
    f->src_id = src_id;
    f->type = type;
    f->length = Frame::empty_frame_size + length;
    f->seq = seq++;

    // Return the final encapsulated packet
    return p1;
}

void JaldiEncap::push(int, Packet* p)
{
    if (Packet* q = action(p))
        output(out_port).push(q);
}

Packet* JaldiEncap::pull(int)
{
    if (Packet *p = input(in_port).pull())
        return action(p);
    else
        return NULL;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Frame)
EXPORT_ELEMENT(JaldiEncap)
