#ifndef RATEDSOURCE_HH
#define RATEDSOURCE_HH
#include "element.hh"

/*
 * =c
 * RatedSource([DATA, COUNTPERSEC, SEC])
 * =d
 * Creates packets consisting of DATA. The desired behavior is to push
 * COUNTPERSEC such packets out its single output per second, do so for SEC
 * number of seconds. After all packets are sent, stops the driver. Default
 * DATA is at least 64 bytes long. Default COUNTPERSEC is 10. Default SEC is
 * 10 (send 10*10=100 number of packets).
 * =e
 * = RatedSource(\<0800>, 10, 10) -> Queue -> ...
 * =n
 * Useful for profiling and experiments.
 * =h count read-only
 * Returns the number of packets that have been generated.
 */

class RatedSource : public Element { protected:
  
  String _data;
  unsigned _time;
  unsigned _persec;
  unsigned _total_sent;
  unsigned _total;
  unsigned _ngap;
  Packet *_packet;
  struct timeval _tv1, _tv2, _diff;
  
 public:
  
  RatedSource();
  
  const char *class_name() const		{ return "RatedSource"; }
  const char *processing() const		{ return PUSH; }
  void add_handlers();
  
  RatedSource *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  int total_sent() const			{ return _total_sent; }
  void run_scheduled();
};

#endif
