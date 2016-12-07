/*
 * JaldiFakeDriverPrecise.{cc,hh} -- simulates the behavior of the JaldiMAC kernel driver for use with other drivers
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include <click/routervisitor.hh>
#include <click/standard/scheduleinfo.hh>

#include "JaldiClick.hh"
#include "JaldiQueue.hh"
#include "JaldiFakeDriverPrecise.hh"

using namespace jaldimac;

CLICK_DECLS

JaldiFakeDriverPrecise::JaldiFakeDriverPrecise() : task(this),
                                                   voip_queue_connected(false),
                                                   scheduled_queue(NULL),
                                                   voip_queue(NULL),
                                                   sleeping(false)
{
}

JaldiFakeDriverPrecise::~JaldiFakeDriverPrecise()
{
}

int JaldiFakeDriverPrecise::configure(Vector<String>&, ErrorHandler*)
{
    // Record input port configuration
    if (ninputs() == 2)
        voip_queue_connected = false;

    return 0;
}

int JaldiFakeDriverPrecise::initialize(ErrorHandler* errh)
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

    // Initialize state
    sleeping = false;

    // Initialize task
    ScheduleInfo::initialize_task(this, &task, true, errh);

    // Success!
    return 0;
}

void JaldiFakeDriverPrecise::take_state(Element* old, ErrorHandler*)
{
    JaldiFakeDriverPrecise* oldJFDP = (JaldiFakeDriverPrecise*) old->cast("JaldiFakeDriverPrecise");

    if (oldJFDP)
    {
        sleeping = oldJFDP->sleeping;
        sleep_until.tv_sec = oldJFDP->sleep_until.tv_sec;
        sleep_until.tv_usec = oldJFDP->sleep_until.tv_usec;
    }
}

void JaldiFakeDriverPrecise::push(int, Packet* p)
{
    // Got a message from downstream (the stations); push it to the master.
    output(out_port_to_master).push(p);
}

void JaldiFakeDriverPrecise::sleep_for_us(uint32_t us)
{
    // Get current time.
    gettimeofday(&sleep_until, NULL);

    // Advance.
    sleep_until.tv_usec += us;

    // Normalize.
    while (sleep_until.tv_usec > 1000000)
    {
        ++sleep_until.tv_sec;
        sleep_until.tv_usec -= 1000000;
    }
}

bool JaldiFakeDriverPrecise::run_task(Task*)
{
    // Sleep if needed.
    if (sleeping)
    {
        timeval now;
        gettimeofday(&now, NULL);

        if (now.tv_sec < sleep_until.tv_sec || (now.tv_sec == sleep_until.tv_sec && now.tv_usec < sleep_until.tv_usec))
            return false;   // Keep sleeping.
        else
            sleeping = false;
    }

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
                const ContentionSlotPayload* csp = (const ContentionSlotPayload*) f->payload;
                // Let the master know that the round is complete.
                RoundCompleteMessagePayload* rcmp;
                WritablePacket* rcp = make_jaldi_frame<ROUND_COMPLETE_MESSAGE, MASTER_ID>(DRIVER_ID, rcmp);
                output(out_port_to_master).push(rcp);

                // Announce the contention slot.
                output(out_port_to_stations).push(p);

                // Wait until it's over.
                sleep_for_us(csp->duration_us);

                break;
            }

            case VOIP_SLOT:
            {
                const VoIPSlotPayload* vsp = (const VoIPSlotPayload*) f->payload;

                // Announce the VoIP slot.
                output(out_port_to_stations).push(p);

                // Wait until it's over.
                sleep_for_us(vsp->duration_us);

                break;
            }

            case TRANSMIT_SLOT:
            {
                const TransmitSlotPayload* tsp = (const TransmitSlotPayload*) f->payload;

                // Announce the transmit slot.
                output(out_port_to_stations).push(p);

                // Wait until it's over.
                sleep_for_us(tsp->duration_us);

                break;
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
                const DelayMessagePayload* tsp = (const DelayMessagePayload*) f->payload;

                // Delays aren't meant to be broadcast, so kill this frame.
                p->kill();

                // Wait until it's over.
                sleep_for_us(tsp->duration_us);

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

    // We've pulled all of the frames we're allowed to until the timer is
    // triggered again, so reschedule and return.
    task.fast_reschedule();
    return true;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Frame)
EXPORT_ELEMENT(JaldiFakeDriverPrecise)
