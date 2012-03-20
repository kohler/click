#ifndef CLICK_AVERAGECOUNTER_HH
#define CLICK_AVERAGECOUNTER_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/atomic.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * AverageCounter([IGNORE])
 * =s counters
 * measures historical packet count and rate
 * =d
 *
 * Passes packets unchanged from its input to its
 * output, maintaining statistics information about
 * packet count and packet rate using a strict average.
 *
 * The rate covers only the time between the first and
 * most recent packets.
 *
 * IGNORE, by default, is 0. If it is greater than 0,
 * the first IGNORE number of seconds are ignored in
 * the count.
 *
 * =h count read-only
 * Returns the number of packets that have passed through since the last reset.
 *
 * =h byte_count read-only
 * Returns the number of packets that have passed through since the last reset.
 *
 * =h rate read-only
 * Returns packet arrival rate.
 *
 * =h byte_rate read-only
 * Returns packet arrival rate in bytes per second.  (Beware overflow!)
 *
 * =h reset write-only
 * Resets the count and rate to zero.
 */

class AverageCounter : public Element { public:

    AverageCounter();

    const char *class_name() const		{ return "AverageCounter"; }
    const char *port_count() const		{ return PORTS_1_1; }
    int configure(Vector<String> &, ErrorHandler *);

    uint32_t count() const			{ return _count; }
    uint32_t byte_count() const			{ return _byte_count; }
    uint32_t first() const			{ return _first; }
    uint32_t last() const			{ return _last; }
    uint32_t ignore() const			{ return _ignore; }
    void reset();

    int initialize(ErrorHandler *);
    void add_handlers();

    Packet *simple_action(Packet *);

  private:

    atomic_uint32_t _count;
    atomic_uint32_t _byte_count;
    atomic_uint32_t _first;
    atomic_uint32_t _last;
    atomic_uint32_t _first_count;
    uint32_t _ignore;

};

CLICK_ENDDECLS
#endif
