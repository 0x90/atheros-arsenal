/*
 * JaldiGate.{cc,hh} -- sends packets to master at appropriate times based upon control packets
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include <click/routervisitor.hh>

#include "JaldiClick.hh"
#include "JaldiQueue.hh"
#include "JaldiGate.hh"

using namespace jaldimac;

CLICK_DECLS

JaldiGate::JaldiGate() : bulk_queue(NULL), voip_overflow_queue(NULL),
                         outstanding_requests(false), bulk_requested_bytes(0),
                         voip_requested_flows(0), station_id(0)
{
}

JaldiGate::~JaldiGate()
{
}

int JaldiGate::configure(Vector<String>& conf, ErrorHandler* errh)
{
    // Parse configuration parameters
    if (cp_va_kparse(conf, this, errh,
             "ID", cpkP+cpkM, cpByte, &station_id,
             cpEnd) < 0)
        return -1;

    // Check that we have the right number of input ports
    if (ninputs() != FLOWS_PER_VOIP_SLOT + 3)
        return errh->error("wrong number of input ports; need %<%d%>", FLOWS_PER_VOIP_SLOT + 3);

    // Looks good!
    return 0;
}

int JaldiGate::initialize(ErrorHandler* errh)
{
    // Seed PRNG
    srand(time(NULL));

    // Find the nearest upstream bulk queue
    ElementCastTracker filter(router(), "JaldiQueue");

    if (router()->visit_upstream(this, in_port_bulk, &filter) < 0 || filter.size() == 0)
        return errh->error("couldn't find an upstream bulk JaldiQueue on input port %<%d%> using flow-based router context", in_port_bulk);

    if (! (bulk_queue = (JaldiQueue*) filter[0]->cast("JaldiQueue")))
        return errh->error("bulk queue %<%s%> on input port %<%d%> is not a valid JaldiQueue (cast failed)", filter[0]->name().c_str(), in_port_bulk);

    // Find the nearest upstream VoIP queues
    for (unsigned voip_port = 0 ; voip_port < FLOWS_PER_VOIP_SLOT ; ++voip_port)
    {
        filter.clear();

        if (router()->visit_upstream(this, in_port_voip_first + voip_port, &filter) < 0 || filter.size() == 0)
            return errh->error("couldn't find an upstream VoIP JaldiQueue on input port %<%d%> using flow-based router context", in_port_voip_first + voip_port);

        if (! (voip_queues[voip_port] = (JaldiQueue*) filter[0]->cast("JaldiQueue")))
            return errh->error("VoIP queue %<%s%> on input port %<%d%> is not a valid JaldiQueue (cast failed)", filter[0]->name().c_str(), in_port_voip_first + voip_port);
    }

    // Find the nearest upstream VoIP overflow queue
    filter.clear();

    if (router()->visit_upstream(this, in_port_voip_overflow, &filter) < 0 || filter.size() == 0)
        return errh->error("couldn't find an upstream VoIP overflow JaldiQueue on input port %<%d%> using flow-based router context", in_port_voip_overflow);

    if (! (voip_overflow_queue = (JaldiQueue*) filter[0]->cast("JaldiQueue")))
        return errh->error("VoIP queue %<%s%> on input port %<%d%> is not a valid JaldiQueue (cast failed)", filter[0]->name().c_str(), in_port_voip_overflow);


    // Success!
    return 0;
}

void JaldiGate::take_state(Element* old, ErrorHandler*)
{
    JaldiGate* oldJG = (JaldiGate*) old->cast("JaldiGate");

    if (oldJG)
    {
        outstanding_requests = oldJG->outstanding_requests;
        bulk_requested_bytes = oldJG->bulk_requested_bytes;
        voip_requested_flows = oldJG->voip_requested_flows;
        station_id = oldJG->station_id;
    }
}

WritablePacket* JaldiGate::make_request_frame()
{
    // Verify that a request is needed
    unsigned bulk_new_bytes = bulk_queue->total_length() - bulk_requested_bytes;
    unsigned voip_new_flows = 0;

    for (int voip_queue = 0 ; voip_queue < int(FLOWS_PER_VOIP_SLOT) ; ++voip_queue)
    {
        if (! voip_queues[voip_queue]->empty())
            voip_new_flows += 1;
    }

    voip_new_flows -= voip_requested_flows;

    if (bulk_new_bytes == 0 && voip_new_flows == 0)
        return NULL;        // Nothing to request!

    // Construct a request frame
    RequestFramePayload* rfp;
    WritablePacket* rp = make_jaldi_frame<REQUEST_FRAME, MASTER_ID>(station_id, rfp);
    rfp->bulk_request_bytes = bulk_new_bytes;
    rfp->voip_request_flows = voip_new_flows;

    // Update state
    outstanding_requests = true;
    bulk_requested_bytes += bulk_new_bytes;
    voip_requested_flows += voip_new_flows;

    // Return constructed packet
    return rp;
}

void JaldiGate::push(int, Packet* p)
{
    // We've received some kind of control traffic; take action based on the
    // specific type and parameters.
    const Frame* f = (const Frame*) p->data();

    if (! (f->dest_id == BROADCAST_ID || f->dest_id == station_id))
    {
        // Not for us! Dump it out the optional output port
        checked_output_push(out_port_bad, p);
        return;
    }

    switch (f->type)
    {
        case CONTENTION_SLOT:
        {
            WritablePacket* rp;

            // Reset requested VoIP flows since they don't carry over between rounds
            voip_requested_flows = 0;

            // Send requests if we need to and we won't get a chance later
            if (outstanding_requests)
                outstanding_requests = false;  // We'll get another chance
            else if ((rp = make_request_frame()) != NULL)
            {
                // We need to send a request!

		// If possible, create a delay message with a random delay
		// within the contention slot
                const ContentionSlotPayload* payload = (const ContentionSlotPayload*) f->payload;
                uint32_t requested_duration_us = rp->length() / BITRATE__BYTES_PER_US + 1;

                if (requested_duration_us < payload->duration_us)
                {
                    // Construct and send a delay message frame
                    DelayMessagePayload* dmp;
                    WritablePacket* dp = make_jaldi_frame<DELAY_MESSAGE, DRIVER_ID>(station_id, dmp);
                    dmp->duration_us = rand() % (payload->duration_us - requested_duration_us + 1);
                    output(out_port).push(dp);
                }

                // Send the request
                output(out_port).push(rp);
            }

            p->kill();

            break;
        }

        case VOIP_SLOT:
        {
            WritablePacket* rp;

            // Send a VoIP packet if the master has given us a chance to do so

            const VoIPSlotPayload* payload = (const VoIPSlotPayload*) f->payload;

            bool already_requested = false;
            int cur_voip_queue = in_port_voip_first;
            for (unsigned i = 0 ; i < FLOWS_PER_VOIP_SLOT ; ++i)
            {
                if (payload->stations[i] == station_id)
                {
                    if (! already_requested && (rp = make_request_frame()) != NULL)
                    {
                        // Send a request frame
                        output(out_port).push(rp);
                        already_requested = true;
                    }

                    // Send one of our VoIP packets
                    while (cur_voip_queue < in_port_voip_overflow)
                    {
                        Packet* vp = input(cur_voip_queue++).pull();

                        if (vp)
                        {
                            output(out_port).push(vp);
                            break;
                        }
                    }
                }
                else
                {
                    // Construct a delay message frame for the driver
                    DelayMessagePayload* dmp;
                    WritablePacket* dp = make_jaldi_frame<DELAY_MESSAGE, DRIVER_ID>(station_id, dmp);
                    dmp->duration_us = VOIP_SLOT_SIZE__BYTES / BITRATE__BYTES_PER_US + 1;

                    // Send it
                    output(out_port).push(dp);
                }
            }

            p->kill();

            break;
        }

        case TRANSMIT_SLOT:
        {
            WritablePacket* rp;
            uint32_t next_frame_duration_us;

            // If we have requests or bulk data, send them
            
            const TransmitSlotPayload* payload = (const TransmitSlotPayload*) f->payload;
            uint32_t duration_us = payload->duration_us;

            if ((rp = make_request_frame()) != NULL)
            {
                // Send a request frame
                output(out_port).push(rp);
                duration_us -= rp->length() / BITRATE__BYTES_PER_US + 1;
            }

            // Send VoIP frames that we will not receive a VoIP slot for
            if (payload->voip_granted_flows < voip_requested_flows)
            {
                uint8_t skip_flows = payload->voip_granted_flows;
                unsigned cur_voip_queue = 0;

                // Send one of our VoIP packets
                while (cur_voip_queue < FLOWS_PER_VOIP_SLOT)
                {
                    // Skip over any empty voip queues
                    if (voip_queues[cur_voip_queue]->empty())
                    {
                        ++cur_voip_queue;
                        continue;
                    }

                    // Skip over queues that will be handled by a VoIP slot
                    if (skip_flows > 0)
                    {
                        ++cur_voip_queue;
                        --skip_flows;
                        continue;
                    }

                    // Skip over queues that we don't have time to send
                    if ((next_frame_duration_us = voip_queues[cur_voip_queue]->head_length() / BITRATE__BYTES_PER_US + 1) < duration_us)
                    {
                        ++cur_voip_queue;
                        continue;
                    }

                    // OK, it's safe to send a packet from this queue!
                    Packet* vp = input(in_port_voip_first + cur_voip_queue++).pull();
                    output(out_port).push(vp);

                    // Update remaining duration
                    duration_us -= next_frame_duration_us;
                }
            }

            // Send overflow VoIP frames
            while (! voip_overflow_queue->empty() && (next_frame_duration_us = voip_overflow_queue->head_length() / BITRATE__BYTES_PER_US + 1) < duration_us)
            {
                // Pull the next frame and send it
                Packet* vp = input(in_port_voip_overflow).pull();
                output(out_port).push(vp);

                // Update remaining duration
                duration_us -= next_frame_duration_us;
            }

            // Send bulk frames
            while (! bulk_queue->empty() && (next_frame_duration_us = bulk_queue->head_length() / BITRATE__BYTES_PER_US + 1) < duration_us)
            {
                // Pull the next frame, update stats, and send it
                Packet* bp = input(in_port_bulk).pull();
                bulk_requested_bytes -= bp->length();
                output(out_port).push(bp);

                // Update remaining duration
                duration_us -= next_frame_duration_us;
            }

            p->kill();

            break;
        }

        default:
        {
            // Bad stuff; dump it out the optional output
            checked_output_push(out_port_bad, p);
            break;
        }
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Frame)
EXPORT_ELEMENT(JaldiGate)
