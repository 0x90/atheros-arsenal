#ifndef CLICK_JALDIQUEUE_HH
#define CLICK_JALDIQUEUE_HH
#include <click/element.hh>
#include <click/standard/storage.hh>
CLICK_DECLS

/*
=c

JaldiQueue
JaldiQueue(CAPACITY)

=s jaldi

stores packets in a FIFO queue

=d

Stores incoming packets in a first-in-first-out queue.
Drops incoming packets if the queue already holds CAPACITY packets.
The default for CAPACITY is 1000.

B<Multithreaded Click note:> JaldiQueue is designed to be used in an
environment with at most one concurrent pusher and at most one concurrent
puller.  Thus, at most one thread pushes to the JaldiQueue at a time and at
most one thread pulls from the JaldiQueue at a time.  Different threads can
push to and pull from the JaldiQueue concurrently, however.  See
ThreadSafeQueue for a queue that can support multiple concurrent pushers and
pullers.

JaldiQueue is a variation of SimpleQueue from the Click distribution that
includes internal changes required for other Jaldi elements, such as JaldiGate,
to work. From the level of the Click configuration language, there is no
difference between JaldiQueue and SimpleQueue.

=n

The Queue and NotifierQueue elements act like JaldiQueue, but additionally
notify interested parties when they change state (from nonempty to empty or
vice versa, and/or from nonfull to full or vice versa).

=h length read-only

Returns the current number of packets in the queue.

=h highwater_length read-only

Returns the maximum number of packets that have ever been in the queue at once.

=h capacity read/write

Returns or sets the queue's capacity.

=h drops read-only

Returns the number of packets dropped by the queue so far.  Dropped packets
are emitted on output 1 if output 1 exists.

=h reset_counts write-only

When written, resets the C<drops> and C<highwater_length> counters.

=h reset write-only

When written, drops all packets in the queue.

=a Queue, NotifierQueue, MixedQueue, RED, FrontDropQueue, ThreadSafeQueue */

class JaldiQueue : public Element, public Storage { public:

    JaldiQueue();
    ~JaldiQueue();

    int drops() const               { return _drops; }
    int highwater_length() const        { return _highwater_length; }

    inline bool enq(Packet*);
    inline void lifo_enq(Packet*);
    inline Packet* deq();
    inline unsigned total_length();
    inline unsigned head_length();

    // to be used with care
    Packet* packet(int i) const         { return _q[i]; }
    void reset();               // NB: does not do notification

    template <typename Filter> Packet* yank1(Filter);
    template <typename Filter> Packet* yank1_peek(Filter);
    template <typename Filter> int yank(Filter, Vector<Packet *> &);

    const char *class_name() const      { return "JaldiQueue"; }
    const char *port_count() const      { return PORTS_1_1X2; }
    const char *processing() const      { return "h/lh"; }
    void* cast(const char*);

    int configure(Vector<String>&, ErrorHandler*);
    int initialize(ErrorHandler*);
    void cleanup(CleanupStage);
    bool can_live_reconfigure() const       { return true; }
    int live_reconfigure(Vector<String>&, ErrorHandler*);
    void take_state(Element*, ErrorHandler*);
    void add_handlers();

    void push(int port, Packet*);
    Packet* pull(int port);

  protected:

    Packet* volatile * _q;
    volatile int _drops;
    int _highwater_length;

    friend class MixedQueue;
    friend class TokenQueue;
    friend class InOrderQueue;
    friend class ECNQueue;

    static String read_handler(Element*, void*);
    static int write_handler(const String&, Element*, void*, ErrorHandler*);

};


inline bool
JaldiQueue::enq(Packet *p)
{
    assert(p);
    int h = _head, t = _tail, nt = next_i(t);
    if (nt != h) {
    _q[t] = p;
    packet_memory_barrier(_q[t], _tail);
    _tail = nt;
    int s = size(h, nt);
    if (s > _highwater_length)
        _highwater_length = s;
    return true;
    } else {
    p->kill();
    _drops++;
    return false;
    }
}

inline void
JaldiQueue::lifo_enq(Packet *p)
{
    // XXX NB: significantly more dangerous in a multithreaded environment
    // than plain (FIFO) enq().
    assert(p);
    int h = _head, t = _tail, ph = prev_i(h);
    if (ph == t) {
    t = prev_i(t);
    _q[t]->kill();
    _tail = t;
    }
    _q[ph] = p;
    packet_memory_barrier(_q[ph], _head);
    _head = ph;
}

inline Packet *
JaldiQueue::deq()
{
    int h = _head, t = _tail;
    if (h != t) {
    Packet *p = _q[h];
    packet_memory_barrier(_q[h], _head);
    _head = next_i(h);
    assert(p);
    return p;
    } else
    return 0;
}

// Report the total length, in bytes, of every packet in the queue.
inline unsigned
JaldiQueue::total_length()
{
    unsigned size = 0;

    for (int trav = _head; trav != _tail; trav = next_i(trav))
        size += _q[trav]->length();

    return size;
}

// Report the length, in bytes, of the packet at the head of the queue.
// Unsafe if the queue is empty, so check first!
inline unsigned
JaldiQueue::head_length()
{
    return _q[_head]->length();
}

template <typename Filter>
Packet *
JaldiQueue::yank1(Filter filter)
    /* Remove from the queue and return the first packet that matches
       'filter(Packet *)'. The returned packet must be deallocated by the
       caller. */
{
    for (int trav = _head; trav != _tail; trav = next_i(trav))
    if (filter(_q[trav])) {
        Packet *p = _q[trav];
        int prev = prev_i(trav);
        while (trav != _head) {
        _q[trav] = _q[prev];
        trav = prev;
        prev = prev_i(prev);
        }
        _head = next_i(_head);
        return p;
    }
    return 0;
}

template <typename Filter>
Packet *
JaldiQueue::yank1_peek(Filter filter)
    /* return the first packet that matches
       'filter(Packet *)'. The returned packet must *NOT* be deallocated by the
       caller. */
{
    for (int trav = _head; trav != _tail; trav = next_i(trav))
    if (filter(_q[trav])) {
        Packet *p = _q[trav];
        return p;
    }
    return 0;
}

template <typename Filter>
int
JaldiQueue::yank(Filter filter, Vector<Packet *> &yank_vec)
    /* Removes from the queue and adds to 'yank_vec' all packets in the queue
       that match 'filter(Packet *)'. Packets are added to 'yank_vec' in LIFO
       order, so 'yank_vec.back()' will equal the first packet in the queue
       that matched 'filter()'. Caller should deallocate any packets returned
       in 'yank_vec'. Returns the number of packets yanked. */
{
    int write_ptr = _tail;
    int nyanked = 0;
    for (int trav = _tail; trav != _head; ) {
    trav = prev_i(trav);
    if (filter(_q[trav])) {
        yank_vec.push_back(_q[trav]);
        nyanked++;
    } else {
        write_ptr = prev_i(write_ptr);
        _q[write_ptr] = _q[trav];
    }
    }
    _head = write_ptr;
    return nyanked;
}

CLICK_ENDDECLS
#endif
