#ifndef RATEDSOURCE_HH
#define RATEDSOURCE_HH
#include "element.hh"
#include "timer.hh"

/*
 * =c
 * RatedSource([DATA, RATE, LIMIT, ACTIVE])
 * =s
 * generates packets at specified rate
 * V<sources>
 * =d
 *
 * Creates packets consisting of DATA. Pushes RATE such packets out its single
 * output per second for a total of TIME seconds. It will send a maximum of
 * one packet per scheduling, so very high RATEs may not be achievable. Stops
 * sending after LIMIT packets are generated; but if LIMIT is negative,
 * sends packets forever. Will send packets only if ACTIVE is true. (ACTIVE is
 * true by default.) Default DATA is at least 64 bytes long. Default RATE is
 * 10. Default LIMIT is -1 (send packets forever).
 *
 * To generate a particular traffic pattern, use this element and RatedSource
 * in conjunction with PokeHandlers.
 *
 * =e
 *   RatedSource(\<0800>, 10, 10) -> Queue -> ...
 * =n
 * Useful for profiling and experiments.
 *
 * =h count read-only
 * Returns the total number of packets that have been generated.
 * =h reset write-only
 * Resets the number of generated packets to 0. The RatedSource will then
 * generate another LIMIT packets (if it is active).
 * =h rate read/write
 * Returns or sets the RATE parameter.
 * =h limit read/write
 * Returns or sets the LIMIT parameter. Negative numbers mean no limit.
 * =h active read/write
 * Makes the element active or inactive.
 *
 * =a InfiniteSource, PokeHandlers
 */

class RatedSource : public Element { protected:
  
  static const unsigned UGAP_SHIFT = 12;
  static const unsigned NO_LIMIT = 0xFFFFFFFFU;
  
  unsigned _count;
  int _sec_count;
  unsigned _limit;
  int _tv_sec;
  unsigned _ugap;
  unsigned _rate;
  bool _active;
  Packet *_packet;
  String _data;

  static String read_param(Element *, void *);
  static int change_param(const String &, Element *, void *, ErrorHandler *);
  
 public:
  
  RatedSource();
  
  const char *class_name() const		{ return "RatedSource"; }
  const char *processing() const		{ return PUSH; }
  void add_handlers();
  
  RatedSource *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  
};

#endif
