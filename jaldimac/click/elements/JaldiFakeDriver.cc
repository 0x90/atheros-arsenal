/*
 * JaldiFakeDriver.{cc,hh} -- simulates the behavior of the JaldiMAC kernel driver for use with other drivers
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include <click/routervisitor.hh>

#include "JaldiClick.hh"
#include "JaldiQueue.hh"
#include "JaldiFakeDriver.hh"

using namespace jaldimac;

CLICK_DECLS

JaldiFakeDriver::JaldiFakeDriver() : timer(this), max_frames_per_trigger(1),
                                     voip_queue_connected(false),
                                     scheduled_queue(NULL), voip_queue(NULL)
{
}

JaldiFakeDriver::~JaldiFakeDriver()
{
}

int JaldiFakeDriver::configure(Vector<String>& conf, ErrorHandler* errh)
{
    // Parse configuration parameters
    if (cp_va_kparse(conf, this, errh,
             "FRAMES", cpkP+cpkM, cpUnsigned, &max_frames_per_trigger,
             cpEnd) < 0)
        return -1;

    // Record input port configuration
    if (ninputs() == 2)
        voip_queue_connected = false;

    return 0;
}

int JaldiFakeDriver::initialize(ErrorHandler* errh)
{
    // Find the nearest upstream scheduled frame queue
    ElementCastTracker filter(router(), "JaldiQueue");

    if (router()->visit_upstream(this, in_port_scheduled, &filter) < 0 || filter.size() == 0)
        return errh->error("couldn't find an upstream scheduled frame JaldiQueue on input port %<%d%> using flow-based router context", in_port_scheduled);

    if (! (scheduled_queue = (JaldiQueue*) filter[0]->cast("JaldiQueue")))
        return errh->error("scheduled frame queue %<%s%> on input port %<%d%> is not a valid JaldiQueue (cast failed)", filter[0]->name().c_str(), in_port_scheduled);

    if (voip_queue_connected)
    {
        // Find the nearest upstream VoIP queue
        filter.clear();

        if (router()->visit_upstream(this, in_port_upstream_voip, &filter) < 0 || filter.size() == 0)
            return errh->error("couldn't find an upstream VoIP JaldiQueue on input port %<%d%> using flow-based router context", in_port_upstream_voip);

        if (! (voip_queue = (JaldiQueue*) filter[0]->cast("JaldiQueue")))
            return errh->error("VoIP queue %<%s%> on input port %<%d%> is not a valid JaldiQueue (cast failed)", filter[0]->name().c_str(), in_port_upstream_voip);
    }

    // Initialize timer
    timer.initialize(this);
    timer.schedule_now();

    // Success!
    return 0;
}

void JaldiFakeDriver::push(int, Packet* p)
{
    // Got a message from downstream (the stations); push it to the master.
    output(out_port_to_master).push(p);
}

void JaldiFakeDriver::run_timer(Timer*)
{
    // Pull scheduled frames
    unsigned pulled_frames = 0;
    while (pulled_frames < max_frames_per_trigger)
    {
        Packet* p = input(in_port_scheduled).pull();
        ++pulled_frames;

        if (p == NULL)
            break;
            
        // Got a Jaldi frame from the scheduler; decode it to decide what to do.
        const Frame* f = (const Frame*) p->data();

        switch (f->type)
        {
            case BULK_FRAME:
            case VOIP_FRAME:
            case REQUEST_FRAME:
            {
                // Transmit a VoIP frame from upstream if one is waiting.
                if (voip_queue_connected)
                {
                    Packet* vp = input(in_port_upstream_voip).pull();
                
                    if (vp)
                    {
                        ++pulled_frames;
                        output(out_port_to_stations).push(vp);
                    }
                }

                // Transmit the scheduled frame.
                output(out_port_to_stations).push(p);

                break;
            }

            case CONTENTION_SLOT:
            {
                // Convert duration to milliseconds.
                const ContentionSlotPayload* csp = (const ContentionSlotPayload*) f->payload;
                uint32_t duration_ms = csp->duration_us / 1000;

                if (duration_ms < 1)
                    duration_ms = 1;

                // Let the master know that the round is complete.
                RoundCompleteMessagePayload* rcmp;
                WritablePacket* rcp = make_jaldi_frame<ROUND_COMPLETE_MESSAGE, MASTER_ID>(DRIVER_ID, rcmp);
                output(out_port_to_master).push(rcp);

                // Announce the contention slot.
                output(out_port_to_stations).push(p);

                // Wait until it's over. (as best we can with this timer resolution)
                timer.reschedule_after_msec(duration_ms);

                return;
            }

            case VOIP_SLOT:
            {
                // Convert duration to milliseconds.
                const VoIPSlotPayload* vsp = (const VoIPSlotPayload*) f->payload;
                uint32_t duration_ms = vsp->duration_us / 1000;

                if (duration_ms < 1)
                    duration_ms = 1;

                // Announce the VoIP slot.
                output(out_port_to_stations).push(p);

                // Wait until it's over. (as best we can with this timer resolution)
                timer.reschedule_after_msec(duration_ms);

                return;
            }

            case TRANSMIT_SLOT:
            {
                // Convert duration to milliseconds.
                const TransmitSlotPayload* tsp = (const TransmitSlotPayload*) f->payload;
                uint32_t duration_ms = tsp->duration_us / 1000;

                if (duration_ms < 1)
                    duration_ms = 1;

                // Announce the transmit slot.
                output(out_port_to_stations).push(p);

                // Wait until it's over. (as best we can with this timer resolution)
                timer.reschedule_after_msec(duration_ms);

                return;
            }

            case BITRATE_MESSAGE:
            {
                // This isn't implemented.
                click_chatter("%s: BITRATE_MESSAGE is unsupported\n", declaration().c_str());
                p->kill();
                break;
            }

            case ROUND_COMPLETE_MESSAGE:
            {
                // This isn't meant to be broadcast.
                p->kill();
                break;
            }

            case DELAY_MESSAGE:
            {
                // Convert duration to milliseconds.
                const DelayMessagePayload* tsp = (const DelayMessagePayload*) f->payload;
                uint32_t duration_ms = tsp->duration_us / 1000;

                if (duration_ms < 1)
                    duration_ms = 1;

                // Delays aren't meant to be broadcast, so kill this frame.
                p->kill();

                // Wait until it's over. (as best we can with this timer resolution)
                timer.reschedule_after_msec(duration_ms);

                return;
            }

            default:
            {
                // Bad stuff; dump it out the optional output
                checked_output_push(out_port_bad, p);
                break;
            }
        }
    }

    // We've pulled all of the frames we're allowed to until the timer is
    // triggered again, so reschedule and return.
    timer.reschedule_after_msec(timer_period_ms);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Frame)
EXPORT_ELEMENT(JaldiFakeDriver)
