#ifndef CLICK_JALDIFAKEDRIVERPRECISE_HH
#define CLICK_JALDIFAKEDRIVERPRECISE_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

JaldiFakeDriverPrecise

=s jaldi

simulates the behavior of the JaldiMAC kernel driver for use with other drivers

=d

JaldiFakeDriverPrecise sits between the JaldiScheduler and a non-JaldiMAC kernel
driver that does not support the notifications and timers that the JaldiMAC
kernel driver makes available. It simulates these behaviors in Click.

JaldiFakeDriverPrecise is different from JaldiFakeDriver in that it uses a higher
resolution timing method. The downside of this method is that it relies on busy
waiting, so JaldiFakeDriverPrecise will take all available CPU time for itself.
However, JaldiFakeDriverPrecise should do a much better job of getting correct
timing and sending packets at high speed than JaldiFakeDriver.

Because of a recent design change, JaldiFakeDriverPrecise also has an additional
responsibility - it dynamically inserts VoIP frames destined for the stations
from upstream into its output. This may have the effect of making the resulting
round longer than the nominal maximum round size, or slightly changing the
distance between VoIP slots, but under normal traffic conditions these effects
should be minimal, and this is the best way we have to simulate real dynamic
scheduling of VoIP from upstream under the deadline constraints we have.

JaldiFakeDriverPrecise's first input (push) receives traffic from downstream (the
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

JaldiScheduler, JaldiFakeDriver */

class JaldiQueue;

class JaldiFakeDriverPrecise : public Element { public:

    JaldiFakeDriverPrecise();
    ~JaldiFakeDriverPrecise();

    const char* class_name() const  { return "JaldiFakeDriverPrecise"; }
    const char* port_count() const  { return "2-3/2-3"; }
    const char* processing() const  { return "hl/h"; }
    const char* flow_code() const   { return COMPLETE_FLOW; }

    int configure(Vector<String>&, ErrorHandler*);
    int initialize(ErrorHandler*);
    bool can_live_reconfigure() const   { return true; }
    void take_state(Element*, ErrorHandler*);

    void push(int, Packet*);
    bool run_task(Task*);

  private:
    void sleep_for_us(uint32_t us);

    static const int in_port_from_stations = 0;
    static const int in_port_scheduled = 1;
    static const int in_port_upstream_voip = 2;
    static const int out_port_to_master = 0;
    static const int out_port_to_stations = 1;
    static const int out_port_bad = 1;

    static const unsigned max_frames_per_trigger = 1;

    Task task;
    bool voip_queue_connected;
    JaldiQueue* scheduled_queue;
    JaldiQueue* voip_queue;
    bool sleeping;
    timeval sleep_until;
};

CLICK_ENDDECLS
#endif
