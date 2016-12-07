/*
 * JaldiPrint.{cc,hh} -- prints Jaldi frames
 */

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <algorithm>

#include "JaldiPrint.hh"

using namespace jaldimac;
using namespace std;

CLICK_DECLS

JaldiPrint::JaldiPrint()
{
}

JaldiPrint::~JaldiPrint()
{
}

int JaldiPrint::configure(Vector<String>& conf, ErrorHandler* errh)
{
    bool contents_supplied = false;

    // Parse configuration parameters
    if (cp_va_kparse(conf, this, errh,
             "CONTENTS", cpkP+cpkC, &contents_supplied, cpBool, &show_contents,
             cpEnd) < 0)
        return -1;

    if (! contents_supplied)
        show_contents = false;

    return 0;
}

void JaldiPrint::show_raw_payload(const Frame* f)
{
    if (show_contents)
    {
        char buffer[5000];
        char* buf = buffer;
        unsigned length = min(f->payload_length(), (const uint32_t) 2000);
        const uint8_t* payload = f->payload;

        for (unsigned i = 0 ; i < length ; ++i)
        {
            if (i && (i % 4) == 0)
                *buf++ = ' ';

            sprintf(buf, "%02x", *payload++ & 0xff);
            buf += 2;
        }

        *buf = '\0';

        click_chatter("Payload: %s", buffer);
    }
}

Packet* JaldiPrint::action(Packet* p)
{
    // Treat the packet as a Frame
    const Frame* f = (const Frame*) p->data();

    // Print header
    click_chatter("===========================");

    click_chatter("Preamble: %c%c%c%u    Source: %u    Dest: %u",
                  f->preamble[0], f->preamble[1], f->preamble[2],
                  unsigned(f->preamble[3]), f->src_id, f->dest_id);

    click_chatter("Tag: %u    Length: %u    Payload Length: %u    Sequence #: %u",
                  unsigned(f->tag), unsigned(f->length),
                  unsigned(f->payload_length()), unsigned(f->seq));

    switch (f->type)
    {
        case BULK_FRAME:
        {
            click_chatter("Type: BULK_FRAME");
            show_raw_payload(f);
            break;
        }

        case VOIP_FRAME:
        {
            click_chatter("Type: VOIP_FRAME");
            show_raw_payload(f);
            break;
        }

        case REQUEST_FRAME:
        {
            const RequestFramePayload* rfp = (const RequestFramePayload*) f->payload;
            click_chatter("Type: REQUEST_FRAME    Bulk request (bytes): %u    VoIP request (flows): %u",
                          rfp->bulk_request_bytes, unsigned(rfp->voip_request_flows));

            show_raw_payload(f);
            break;
        }

        case CONTENTION_SLOT:
        {
            const ContentionSlotPayload* csp = (const ContentionSlotPayload*) f->payload;
            click_chatter("Type: CONTENTION_SLOT    Duration (us): %u",
                          csp->duration_us);
            
            show_raw_payload(f);
            break;
        }

        case VOIP_SLOT:
        {
            // FIXME: Don't hardcode the number of flows per VoIP slot
            const VoIPSlotPayload* vsp = (const VoIPSlotPayload*) f->payload;
            click_chatter("Type: VOIP_SLOT    Duration (us): %u    Stations: %u %u %u %u",
                          vsp->duration_us, unsigned(vsp->stations[0]),
                          unsigned(vsp->stations[1]), unsigned(vsp->stations[2]),
                          unsigned(vsp->stations[3]));
            
            show_raw_payload(f);
            break;
        }

        case TRANSMIT_SLOT:
        {
            const TransmitSlotPayload* tsp = (const TransmitSlotPayload*) f->payload;
            click_chatter("Type: TRANSMIT_SLOT    Duration (us): %u    Granted VoIP flows: %u",
                          tsp->duration_us, unsigned(tsp->voip_granted_flows));
            
            show_raw_payload(f);
            break;
        }

        case BITRATE_MESSAGE:
        {
            const BitrateMessagePayload* bmp = (const BitrateMessagePayload*) f->payload;
            click_chatter("Type: BITRATE_MESSAGE    Bitrate: %u", bmp->bitrate);
            
            show_raw_payload(f);
            break;
        }

        case ROUND_COMPLETE_MESSAGE:
        {
            click_chatter("Type: ROUND_COMPLETE_MESSAGE");
            
            show_raw_payload(f);
            break;
        }


        case DELAY_MESSAGE:
        {
            const DelayMessagePayload* dmp = (const DelayMessagePayload*) f->payload;
            click_chatter("Type: DELAY_MESSAGE    Duration (us): %u",
                          dmp->duration_us);
            
            show_raw_payload(f);
            break;
        }

        default:
            click_chatter("Type: <<<UNKNOWN TYPE>>>\n"); break;
    }

    click_chatter("===========================");

    return p;
}

void JaldiPrint::push(int, Packet* p)
{
    if (Packet* q = action(p))
        output(out_port).push(q);
}

Packet* JaldiPrint::pull(int)
{
    if (Packet *p = input(in_port).pull())
        return action(p);
    else
        return NULL;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Frame)
EXPORT_ELEMENT(JaldiPrint)
