/*
 * JaldiDecap.{cc,hh} -- decapsulates Jaldi frame
 */

#include <click/config.h>
#include "JaldiDecap.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "Frame.hh"

using namespace jaldimac;

CLICK_DECLS

JaldiDecap::JaldiDecap()
{
}

JaldiDecap::~JaldiDecap()
{
}

int JaldiDecap::configure(Vector<String>& conf, ErrorHandler* errh)
{
    // Parse configuration parameters
    if (cp_va_kparse(conf, this, errh,
             "DEST", cpkP+cpkC, &should_filter_by_dest, cpByte, &dest_id,
             cpEnd) < 0)
        return -1;
    else
        return 0;
}

void JaldiDecap::push(int, Packet* p)
{
    // Treat the packet as a Frame
    const Frame* f = (const Frame*) p->data();

    // Filter by dest_id if requested
    if (should_filter_by_dest && !(f->dest_id == BROADCAST_ID || f->dest_id == dest_id))
        checked_output_push(out_port_bad, p);

    // Classify the packet (Control, Data, or Bad)?
    switch (f->type)
    {
        case BULK_FRAME:
        case VOIP_FRAME:
            // Strip Jaldi header and footer
            p->pull(Frame::header_size);
            p->take(Frame::footer_size);
            output(out_port_data).push(p);
            break;

        case REQUEST_FRAME:
        case CONTENTION_SLOT:
        case VOIP_SLOT:
        case TRANSMIT_SLOT:
        case BITRATE_MESSAGE:
        case ROUND_COMPLETE_MESSAGE:
        case DELAY_MESSAGE:
            output(out_port_control).push(p);
            break;

        default:
            checked_output_push(out_port_bad, p);
            break;
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Frame)
EXPORT_ELEMENT(JaldiDecap)
