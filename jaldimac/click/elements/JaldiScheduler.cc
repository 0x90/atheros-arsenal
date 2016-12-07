/*
 * JaldiScheduler.{cc,hh} -- constructs a Jaldi round layout from incoming packets and requests
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include <click/routervisitor.hh>
#include <algorithm>

#include "JaldiClick.hh"
#include "JaldiQueue.hh"
#include "Frame.hh"
#include "JaldiScheduler.hh"

using namespace jaldimac;
using namespace std;

CLICK_DECLS

JaldiScheduler::JaldiScheduler() : granted_voip(false),
                                   rate_limit_distance_us(DEFAULT_CONTENTION_SLOT_ONLY_DISTANCE__US),
                                   timer(this)
{
}

JaldiScheduler::~JaldiScheduler()
{
}

int JaldiScheduler::configure(Vector<String>& conf, ErrorHandler* errh)
{
    bool rld_supplied = false;
             
    // Parse configuration parameters
    if (cp_va_kparse(conf, this, errh,
             "CSONLYRATELIMIT", cpkP+cpkC, &rld_supplied, cpUnsigned, &rate_limit_distance_us,
             cpEnd) < 0)
        return -1;

    if (! rld_supplied)
        rate_limit_distance_us = DEFAULT_CONTENTION_SLOT_ONLY_DISTANCE__US;

    // We should have 1 input port for every station and 2 control inputs
    if (ninputs() != STATION_COUNT + 2)
        return errh->error("wrong number of input ports connected; need two control ports and a bulk port for each station");
    else
        return 0;
}

int JaldiScheduler::initialize(ErrorHandler* errh)
{
    // Find the nearest upstream queues
    ElementCastTracker filter(router(), "JaldiQueue");

    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
    {
        // Get bulk queue
        filter.clear();

        if (router()->visit_upstream(this, in_port_bulk_first + station, &filter) < 0 || filter.size() == 0)
            return errh->error("couldn't find an upstream bulk JaldiQueue on input port %<%d%> using flow-based router context", in_port_bulk_first + station);

        if (! (bulk_queues[station] = (JaldiQueue*) filter[0]->cast("JaldiQueue")))
            return errh->error("bulk queue %<%s%> found on input port %<%d%> is not a valid JaldiQueue (cast failed)", filter[0]->name().c_str(), in_port_bulk_first + station);
    }

    // Initialize requests and grants
    granted_voip = false;
    voip_granted.duration_us = 0;
    
    for (unsigned flow = 0 ; flow < FLOWS_PER_VOIP_SLOT ; ++flow)
        voip_granted.stations[flow] = BROADCAST_ID;

    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
    {
        bulk_requested_bytes[station] = 0;
        voip_requested_flows[station] = 0;
        bulk_upstream_bytes[station] = 0;
        voip_granted_by_station[station] = 0;
        bulk_granted_bytes[station] = 0;
        bulk_granted_upstream_bytes[station] = 0;
    }

    // Initialize rate limit.
    gettimeofday(&rate_limit_until, NULL);

    // Initialize timer.
    timer.initialize(this);
    timer.schedule_now();

    // Success!
    return 0;
}

void JaldiScheduler::take_state(Element* old, ErrorHandler*)
{
    JaldiScheduler* oldJS = (JaldiScheduler*) old->cast("JaldiScheduler");

    if (oldJS)
    {
        granted_voip = oldJS->granted_voip;
        voip_granted.duration_us = oldJS->voip_granted.duration_us;

        for (unsigned flow = 0 ; flow < FLOWS_PER_VOIP_SLOT ; ++flow)
            voip_granted.stations[flow] = oldJS->voip_granted.stations[flow];

        for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        {
            bulk_requested_bytes[station] = oldJS->bulk_requested_bytes[station];
            voip_requested_flows[station] = oldJS->voip_requested_flows[station];
            bulk_upstream_bytes[station] = oldJS->bulk_upstream_bytes[station];
            voip_granted_by_station[station] = oldJS->voip_granted_by_station[station];
            bulk_granted_bytes[station] = oldJS->bulk_granted_bytes[station];
            bulk_granted_upstream_bytes[station] = oldJS->bulk_granted_upstream_bytes[station];
        }

        rate_limit_until = oldJS->rate_limit_until;
    }
}

void JaldiScheduler::run_timer(Timer*)
{
    // Pretend we received a ROUND_COMPLETE_MESSAGE.
    received_round_complete_message();
}

void JaldiScheduler::push(int, Packet* p)
{
    // We've received some kind of control traffic; take action based on the
    // specific type and parameters.
    const Frame* f = (const Frame*) p->data();

    if (! (f->dest_id == BROADCAST_ID || f->dest_id == MASTER_ID))
    {
        // Not for us! Dump it out the optional output port
        checked_output_push(out_port_bad, p);
        return;
    }

    switch (f->type)
    {
        case REQUEST_FRAME:
        {
            uint8_t station_idx = f->src_id - FIRST_STATION_ID;

            if (f->src_id < FIRST_STATION_ID || station_idx >= STATION_COUNT)
            {
                // Invalid station! dump it out the optional output port
                checked_output_push(out_port_bad, p);
                return;
            }

            // Update requests
            const RequestFramePayload* rfp = (const RequestFramePayload*) f->payload;

            bulk_requested_bytes[station_idx] += rfp->bulk_request_bytes;
            voip_requested_flows[station_idx] += rfp->voip_request_flows;

            p->kill();

            break;
        }

        case ROUND_COMPLETE_MESSAGE:
        {
            // All requests have been received, and all upstream traffic eligible
            // for distribution this round is in the queues. It's time to compute
            // the layout for the next round.

            p->kill();

            received_round_complete_message();

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

void JaldiScheduler::received_round_complete_message()
{
    // Count the frames destined for each station in the queues.
    count_upstream();

    // Rate limit contention-slot-only rounds.
    if (! have_data_or_requests())
    {
        timeval now;
        gettimeofday(&now, NULL);

        if (now.tv_sec < rate_limit_until.tv_sec || (now.tv_sec == rate_limit_until.tv_sec && now.tv_usec < rate_limit_until.tv_usec))
        {
            // Don't create round; just reschedule timer.
            timer.reschedule_after_msec(1);
            return;
        }
        else
        {
            // OK to create round, but first, determine time at which
            // it's ok to send the _next_ contention-slot-only round.
            gettimeofday(&rate_limit_until, NULL);

            // Advance.
            rate_limit_until.tv_usec += rate_limit_distance_us;

            // Normalize.
            while (rate_limit_until.tv_usec > 1000000)
            {
                ++rate_limit_until.tv_sec;
                rate_limit_until.tv_usec -= 1000000;
            }

            // Go ahead and create round...
        }
    }

    /*
    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
    {
        click_chatter("Station: %u BRB: %u VRF: %u BUB: %u",
        station, bulk_requested_bytes[station],
        unsigned(voip_requested_flows[station]),
        bulk_upstream_bytes[station]);
    }
    */

    // Run a fairness algorithm over the upstream frames and requests
    // to determine the allocation each station will receive.
    compute_fair_allocation();

    /*
    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
    {
        click_chatter("Station: %u VG: %u BGB: %u BGUB: %u",
        station, unsigned(voip_granted_by_station[station]),
        bulk_granted_bytes[station],
        bulk_granted_upstream_bytes[station]);
    }

    click_chatter("Granted_voip: %s", granted_voip ? "true" : "false");
    click_chatter("voip_granted.duration_us: %u", voip_granted.duration_us);

    for (unsigned flow = 0 ; flow < FLOWS_PER_VOIP_SLOT ; ++flow)
    {
        click_chatter("Flow %u: station %u", flow,
        unsigned(voip_granted.stations[flow]));
    }
    */

    // Actually compute a layout based on this allocation.
    generate_layout();

    // Reset VoIP requests; they must be re-requested every round.
    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        voip_requested_flows[station] = 0;
}

bool JaldiScheduler::have_data_or_requests()
{
    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
    {
        if (bulk_requested_bytes[station] > 0 || voip_requested_flows[station] > 0 || bulk_upstream_bytes[station] > 0)
            return true;
    }

    return false;
}

void JaldiScheduler::count_upstream()
{
    // Look in each queue and record their total size in bytes.
    // FIXME: This will need to be changed once we add bulk ACKs.
    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        bulk_upstream_bytes[station] = bulk_queues[station]->total_length();
}

bool JaldiScheduler::try_to_allocate_voip_request(unsigned flow, unsigned& next_request_station)
{
    unsigned request_station = next_request_station;
    do
    {
        if (voip_requested_flows[request_station] > 0)
        {
            voip_requested_flows[request_station] -= 1;
            voip_granted.stations[flow] = FIRST_STATION_ID + request_station;
            voip_granted_by_station[request_station] += 1;
            next_request_station = (request_station + 1) % STATION_COUNT;
            return true;
        }
        else
            request_station = (request_station + 1) % STATION_COUNT;
    } while (request_station != next_request_station);

    return false;
}

void JaldiScheduler::allocate_voip_to_no_one(unsigned flow)
{
    for (unsigned flow_ = flow; flow_ < FLOWS_PER_VOIP_SLOT ; ++flow_)
        voip_granted.stations[flow_] = BROADCAST_ID;
}

void JaldiScheduler::compute_fair_allocation()
{
    // We now have all upstream and downstream requests; now we need to
    // decide on an allocation for each station (and each direction) that
    // satisfies the constraints we're operating under (such as the maximum
    // round size) and is in some sense "fair".

    // Initialization.
    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        voip_granted_by_station[station] = 0;

    // First, we take care of VoIP. We only need to schedule upstream VoIP
    // streams here; downstream VoIP streams will be handled dynamically.
    unsigned next_request_station = 0;
    granted_voip = false;
    for (unsigned flow = 0 ; flow < FLOWS_PER_VOIP_SLOT ; ++flow)
    {
        if (try_to_allocate_voip_request(flow, next_request_station))
            granted_voip = true;
        else
        {
            allocate_voip_to_no_one(flow);
            break;
        }
    }

    // Now, handle bulk using max-min fairness.

    // Grant every request / upstream flow the minimum chunk size.
    uint32_t round_size = 0;
    for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
    {
        if (bulk_requested_bytes[station] > 0)
        {
            bulk_granted_bytes[station] = MIN_CHUNK_SIZE__BYTES;
            round_size += MIN_CHUNK_SIZE__BYTES;
            bulk_requested_bytes[station] -= min(MIN_CHUNK_SIZE__BYTES - REQUEST_FRAME_SIZE__BYTES, bulk_requested_bytes[station]);
        }

        if (bulk_granted_bytes[station] == 0 && voip_requested_flows[station] > 0)
        {
            bulk_granted_bytes[station] = MIN_CHUNK_SIZE__BYTES;
            round_size += MIN_CHUNK_SIZE__BYTES;
        }

        if (bulk_upstream_bytes[station] > 0)
        {
            bulk_granted_upstream_bytes[station] = MIN_CHUNK_SIZE__BYTES;
            round_size += MIN_CHUNK_SIZE__BYTES;
            bulk_upstream_bytes[station] -= MIN_CHUNK_SIZE__BYTES;
        }
    }

    // Now keep granting requests until we're out of them or we fill up the round.
    uint32_t next_voip_slot_bytes = MAX_ROUND_SIZE__BYTES;

    // We need to account for the VoIP slots when we're calculating the total
    // round size, so we'll add another VoIP slot's worth of bytes to the
    // round size every time we would reach a VoIP slot in the schedule.
    if (granted_voip)
    {
        round_size += VOIP_SLOT_SIZE__BYTES;
        next_voip_slot_bytes = INTER_VOIP_SLOT_DISTANCE__BYTES;
    }

    unsigned active_stations_and_directions = 2 * STATION_COUNT;
    while (round_size < MAX_ROUND_SIZE__BYTES && active_stations_and_directions > 0)
    {
        // If we need a VoIP slot here, account for it in the round size.
        if (round_size >= next_voip_slot_bytes)
        {
            round_size += VOIP_SLOT_SIZE__BYTES;
            next_voip_slot_bytes += INTER_VOIP_SLOT_DISTANCE__BYTES;

            if (round_size >= MAX_ROUND_SIZE__BYTES)
                break;
        }

        // The max increment is the largest amount we can grant to each station
        // / direction without making it impossible to be fair with the
        // remaining stations / directions. We take the remaining space in the
        // round, and divide it by the number of active stations / directions.
        // We add one to avoid rounding issues.
        uint32_t max_increment = (MAX_ROUND_SIZE__BYTES - round_size)
                                 / active_stations_and_directions + 1;

        // Grant what we can at this point to each station.
        active_stations_and_directions = 0;
        for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        {
            if (bulk_requested_bytes[station] > 0)
            {
                uint32_t to_grant = min(bulk_requested_bytes[station], max_increment);
                bulk_granted_bytes[station] += to_grant;
                round_size += to_grant;
                ++active_stations_and_directions;
            }

            if (bulk_upstream_bytes[station] > 0)
            {
                uint32_t to_grant = min(bulk_upstream_bytes[station], max_increment);
                bulk_granted_upstream_bytes[station] += to_grant;
                round_size += to_grant;
                ++active_stations_and_directions;
            }
        }
    }
}

void JaldiScheduler::generate_layout()
{
    // This is a very simple greedy scheduler. The idea is that we want to
    // minimize (1) RX/TX switches, and (2) fragmentation of the allocations
    // granted to each station. To describe it, we introduce the notion of a
    // 'deadline' - this is the next point in the layout where a particular
    // allocation or frame MUST go. Right now, the deadline corresponds to
    // the next VoIP slot. Given this definition, the scheduler greedily
    // grabs frames from queues and places them in the layout, with the
    // following prioritization (from first choice to last choice):
    //
    // 1. Requests which can be completely fulfilled before the next
    //    deadline.
    // 2. Upstream transfers which can be completely fulfilled before
    //    the next deadline.
    // 3. Requests which can't be completely fulfilled before the next
    //    deadline.
    // 4. Upstream transfers which can't be completely fulfilled before
    //    the next deadline.
    //
    // There's one other factor which affects the choices: feasibility.
    // The scheduler maintains the invariant that transfers from two
    // different stations cannot be scheduled without an intervening
    // transfers from the master. This means that at some times, some
    // of the choices above may be infeasible.

    // Determine first deadline.
    uint32_t next_deadline_bytes = granted_voip ? 0 : 2 * MAX_ROUND_SIZE__BYTES;

    // Generate the layout.
    bool done = false;
    bool last_was_request = false;
    uint32_t round_pos_bytes = 0;
    while (! done)
    {
        top:

        // Are we at a deadline?
        if (round_pos_bytes >= next_deadline_bytes)
        {
            // Emit a VoIP slot.
            VoIPSlotPayload* vsp;
            WritablePacket* vp = make_jaldi_frame<VOIP_SLOT, BROADCAST_ID>(MASTER_ID, vsp);
            memcpy(vsp, &voip_granted, sizeof(VoIPSlotPayload));
            output(out_port).push(vp);

            // Update state.
            next_deadline_bytes += INTER_VOIP_SLOT_DISTANCE__BYTES;
            round_pos_bytes += VOIP_SLOT_SIZE__BYTES;
            last_was_request = true;

            goto top;
        }
        
        uint32_t to_deadline_bytes = next_deadline_bytes - round_pos_bytes;

        // Are there any requests that can be fulfilled before the next deadline?
        for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        {
            if ((! last_was_request) && to_deadline_bytes >= MIN_CHUNK_SIZE__BYTES && bulk_granted_bytes[station] <= to_deadline_bytes && bulk_granted_bytes[station] > 0)
            {
                // Emit a TRANSMIT_SLOT.
                TransmitSlotPayload* tsp;
                WritablePacket* tp = make_jaldi_frame_dyn_dest<TRANSMIT_SLOT>(MASTER_ID, FIRST_STATION_ID + station, tsp);
                tsp->duration_us = max(MIN_CHUNK_SIZE__BYTES, bulk_granted_bytes[station]) / BITRATE__BYTES_PER_US + 1;
                tsp->voip_granted_flows = voip_granted_by_station[station];
                output(out_port).push(tp);

                // Update state.
                round_pos_bytes += bulk_granted_bytes[station];
                bulk_granted_bytes[station] = 0;
                last_was_request = true;

                goto top;
            }
        }

        // Are there any upstream transfers that can be fulfilled before the deadline?
        for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        {
            if (bulk_granted_upstream_bytes[station] <= to_deadline_bytes && bulk_granted_upstream_bytes[station] > 0)
            {
                do
                {
                    if (bulk_queues[station]->empty())
                    {
                        // If the queue's empty, we're done. (Bug?)
                        bulk_granted_upstream_bytes[station] = 0;
                        break;
                    }

                    uint32_t len_bytes = 0;
                    if ((len_bytes = bulk_queues[station]->head_length()) <= bulk_granted_upstream_bytes[station])
                    {
                        // Send.
                        Packet* p = input(in_port_bulk_first + station).pull();
                        output(out_port).push(p);

                        // Update state.
                        round_pos_bytes += len_bytes;
                        bulk_granted_upstream_bytes[station] -= len_bytes;
                    }
                    else
                    {
                        bulk_granted_upstream_bytes[station] = 0;
                        break;
                    }
                } while (true);

                last_was_request = false;

                goto top;
            }
        }

        // Are there any requests that can be partially fulfilled?
        for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        {
            if ((! last_was_request) && to_deadline_bytes >= MIN_CHUNK_SIZE__BYTES && bulk_granted_bytes[station] > to_deadline_bytes)
            {
                // Emit a TRANSMIT_SLOT.
                TransmitSlotPayload* tsp;
                WritablePacket* tp = make_jaldi_frame_dyn_dest<TRANSMIT_SLOT>(MASTER_ID, FIRST_STATION_ID + station, tsp);
                tsp->duration_us = to_deadline_bytes / BITRATE__BYTES_PER_US + 1;
                tsp->voip_granted_flows = voip_granted_by_station[station];
                output(out_port).push(tp);

                // Update state.
                round_pos_bytes += to_deadline_bytes;
                bulk_granted_bytes[station] -= to_deadline_bytes;
                last_was_request = true;

                goto top;
            }
        }

        // Are there any upstream transfers that can be partially fulfilled?
        for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        {
            if (bulk_granted_upstream_bytes[station] > to_deadline_bytes)
            {
                do
                {
                    if (bulk_queues[station]->empty())
                    {
                        // If the queue's empty, we're done. (Bug?)
                        bulk_granted_upstream_bytes[station] = 0;
                        break;
                    }

                    uint32_t len_bytes = 0;
                    if ((len_bytes = bulk_queues[station]->head_length()) <= (next_deadline_bytes - round_pos_bytes))
                    {
                        // Send.
                        Packet* p = input(in_port_bulk_first + station).pull();
                        output(out_port).push(p);

                        // Update state.
                        round_pos_bytes += len_bytes;
                        bulk_granted_upstream_bytes[station] -= len_bytes;
                    }
                    else
                        break;
                } while (true);

                last_was_request = false;

                goto top;
            }
        }

        // If we've reached this point, we couldn't find anything to send.
        // We're either done, or there's nothing that can fit before the
        // next deadline, and we just need to insert a delay.
        done = true;
        for (unsigned station = 0 ; station < STATION_COUNT ; ++station)
        {
            if (bulk_granted_bytes[station] > 0 || bulk_granted_upstream_bytes[station] > 0)
            {
                done = false;
                break;
            }
        }

        if (! done)
        {
            // Insert an appropriate delay.
            DelayMessagePayload* dmp;
            WritablePacket* dp = make_jaldi_frame<DELAY_MESSAGE, DRIVER_ID>(MASTER_ID, dmp);
            dmp->duration_us = to_deadline_bytes / BITRATE__BYTES_PER_US + 1;
            output(out_port).push(dp);

            // Update state.
            round_pos_bytes = next_deadline_bytes;
            last_was_request = false;

            goto top;
        }
    }

    // We've generated the entire layout. Now we complete the round by
    // emitting a contention slot, and we're done!
    ContentionSlotPayload* csp;
    WritablePacket* cp = make_jaldi_frame<CONTENTION_SLOT, BROADCAST_ID>(MASTER_ID, csp);
    csp->duration_us = CONTENTION_SLOT_DURATION__US;
    output(out_port).push(cp);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Frame)
EXPORT_ELEMENT(JaldiScheduler)
