#ifndef RATEDSOURCE_HH
#define RATEDSOURCE_HH
#include "element.hh"
#include "timer.hh"

/*
 * =c
 * RatedSource([DATA, RATE, TIME, ACTIVE])
 * =d
 * Creates packets consisting of DATA. The desired behavior is to push
 * RATE such packets out its single output per second, do so for 
 * number of seconds. After all packets are sent, stops the driver. Default
 * DATA is at least 64 bytes long. Default RATE is 10. Default TIME is
 * -1 (send 10*10=100 number of packets).
 * =e
 * = RatedSource(\<0800>, 10, 10) -> Queue -> ...
 * =n
 * Useful for profiling and experiments.
 * =h count read-only
 * Returns the number of packets that have been generated.
 */

class RatedSource : public Element { protected:
  
  String _data;
  unsigned _rate;
  int _count;
  int _limit;
  unsigned _ngap;
  bool _active;
  Packet *_packet;
  struct timeval _start_time;

  static String read_param(Element *, void *);
  static int change_param(const String &, Element *, void *, ErrorHandler *);
  
 public:
  
  RatedSource();
  
  const char *class_name() const		{ return "RatedSource"; }
  const char *processing() const		{ return PUSH; }
  void add_handlers();
  
  RatedSource *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  
};

#endif
