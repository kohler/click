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
 * =s measurement
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
 * the first IGNORE number of seconds are ignored. in
 * the count.
 *
 * =h count read-only
 * Returns the number of packets that have passed through.
 *
 * =h rate read-only
 * Returns packet arrival rate.
 *
 * =h reset write-only
 * Resets the count and rate to zero.
 */

class AverageCounter : public Element { protected:
  
  uatomic32_t _count;
  uatomic32_t _first;
  uatomic32_t _last;
  uint32_t _ignore;
  
 public:

  AverageCounter();
  ~AverageCounter();
  
  const char *class_name() const		{ return "AverageCounter"; }
  const char *processing() const		{ return AGNOSTIC; }
  int configure(Vector<String> &, ErrorHandler *);

  uint32_t count() const			{ return _count; }
  uint32_t first() const			{ return _first; }
  uint32_t last() const			{ return _last; }
  uint32_t ignore() const			{ return _ignore; }
  void reset();
  
  AverageCounter *clone() const			{ return new AverageCounter; }
  int initialize(ErrorHandler *);
  void add_handlers();
  
  Packet *simple_action(Packet *);
  
};

CLICK_ENDDECLS
#endif
