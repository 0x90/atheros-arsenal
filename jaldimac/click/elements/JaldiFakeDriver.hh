#ifndef CLICK_JALDIFAKEDRIVER_HH
#define CLICK_JALDIFAKEDRIVER_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

JaldiFakeDriver(FRAMES)

=s jaldi

simulates the behavior of the JaldiMAC kernel driver for use with other drivers

=d

JaldiFakeDriver sits between the JaldiScheduler and a non-JaldiMAC kernel
driver that does not support the notifications and timers that the JaldiMAC
kernel driver makes available. It simulates these behaviors in Click.

Because of a recent design change, JaldiFakeDriver also has an additional
responsibility - it dynamically inserts VoIP frames destined for the stations
from upstream into its output. This may have the effect of making the resulting
round longer than the nominal maximum round size, or slightly changing the
distance between VoIP slots, but under normal traffic conditions these effects
should be minimal, and this is the best way we have to simulate real dynamic
scheduling of VoIP from upstream under the deadline constraints we have.

FRAMES is the maximum number of frames that JaldiFakeDriver will process each
time it is trigger. This should be set large enough that we get reasonable
performance (since JaldiFakeDriver only runs once per millisecond) but small
enough that other timers and periodic events get a chance to run.

JaldiFakeDriver's first input (push) receives traffic from downstream (the
stations) and passes it along on its first output (push) unchanged. Input 1
(pull) receives the output of a JaldiScheduler element. Input 2 (pull) receives
upstream VoIP traffic if it is connected.  Everything arriving on all inputs
should be encapsulated in Jaldi frames, and both pull inputs should be
connected to a JaldiQueue.

There are two push outputs; the first is for traffic from downstream (the
stations) to the master, and the second is for scheduled traffic being sent to
the stations. A third push output may be connected to receive erroneous
packets. 

=a

JaldiScheduler, JaldiFakeDriverPrecise */

class JaldiQueue;

class JaldiFakeDriver : public Element { public:

    JaldiFakeDriver();
    ~JaldiFakeDriver();

    const char* class_name() const  { return "JaldiFakeDriver"; }
    const char* port_count() const  { return "2-3/2-3"; }
    const char* processing() const  { return "hl/h"; }
    const char* flow_code() const   { return COMPLETE_FLOW; }

    int configure(Vector<String>&, ErrorHandler*);
    int initialize(ErrorHandler*);
    bool can_live_reconfigure() const   { return true; }

    void push(int, Packet*);
    void run_timer(Timer*);

  private:
    static const int in_port_from_stations = 0;
    static const int in_port_scheduled = 1;
    static const int in_port_upstream_voip = 2;
    static const int out_port_to_master = 0;
    static const int out_port_to_stations = 1;
    static const int out_port_bad = 1;

    static const uint32_t timer_period_ms = 1;

    Timer timer;
    unsigned max_frames_per_trigger;
    bool voip_queue_connected;
    JaldiQueue* scheduled_queue;
    JaldiQueue* voip_queue;
};

CLICK_ENDDECLS
#endif
