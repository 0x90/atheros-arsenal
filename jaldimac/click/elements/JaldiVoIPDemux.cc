/*
 * JaldiVoIPDemux.{cc,hh} -- demultiplexes VoIP flows
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>

#include "Frame.hh"
#include "JaldiVoIPDemux.hh"

using namespace jaldimac;

CLICK_DECLS

JaldiVoIPDemux::JaldiVoIPDemux() : timeout_s(3)
{
}

JaldiVoIPDemux::~JaldiVoIPDemux()
{
}

int JaldiVoIPDemux::configure(Vector<String>& conf, ErrorHandler* errh)
{
    // Parse configuration parameters
    if (cp_va_kparse(conf, this, errh,
             "TIMEOUT", cpkP+cpkM, cpUnsigned, &timeout_s,
             cpEnd) < 0)
        return -1;

    // Check that we have the right number of output ports
    if (noutputs() < int(FLOWS_PER_VOIP_SLOT + 1) || noutputs() > int(FLOWS_PER_VOIP_SLOT + 2))
        return errh->error("wrong number of output ports; need either %<%d%> or %<%d%>", FLOWS_PER_VOIP_SLOT + 1, FLOWS_PER_VOIP_SLOT + 2);

    // Looks good!
    return 0;
}

void JaldiVoIPDemux::push(int, Packet* p)
{
    time_t current_time = time(NULL);

    // Get destination IP and port of this packet
    if (! p->has_network_header())
    {
        checked_output_push(out_port_bad, p);
        return;
    }

    const in_addr dest_ip = p->ip_header()->ip_dst;

    if (! p->ip_header()->ip_p == IP_PROTO_UDP)
    {
        checked_output_push(out_port_bad, p);
        return;
    }

    uint16_t dest_port = p->udp_header()->uh_dport;

    // Check for an output port already assigned to this 2-tuple
    for (unsigned i = 0 ; i < FLOWS_PER_VOIP_SLOT ; ++i)
    {
        if (output_ip[i] == dest_ip && output_port[i] == dest_port)
        {
            output_last_seen[i] = current_time;
            output(i).push(p);
            return;
        }
    }

    // Try to find an expired output port to reclaim
    for (unsigned i = 0 ; i < FLOWS_PER_VOIP_SLOT ; ++i)
    {
        if (difftime(current_time, output_last_seen[i]) > timeout_s)
        {
            output_ip[i] = dest_ip;
            output_port[i] = dest_port;
            output_last_seen[i] = current_time;
            output(i).push(p);
            return;
        }
    }

    // All output ports are claimed! We have to send it out the overflow port
    output(out_port_voip_overflow);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Frame)
EXPORT_ELEMENT(JaldiVoIPDemux)
