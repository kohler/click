#ifndef INFINITESOURCE_HH
#define INFINITESOURCE_HH
#include "element.hh"

/*
 * =c
 * InfiniteSource(BURSTSIZE)
 * InfiniteSource(BURSTSIZE, DATA)
 * InfiniteSource(BURSTSIZE, DATA, TOTALCOUNT)
 * =d
 * Creates packets consisting of DATA. Pushes BURSTSIZE such packets
 * out its single output every time it is scheduled (which will be often).
 * Default DATA is at least 64 bytes long. If TOTALCOUNT is given,
 * sends that many packets, then stops the driver. Otherwise, sends packets
 * forever.
 * =e
 * = InfiniteSource(5) -> Queue -> ...
 * =n
 * Useful for profiling and experiments.
 */

class InfiniteSource : public Element { protected:
  
  String _data;
  int _count;
  int _limit;
  int _total;
  
 public:
  
  InfiniteSource();
  
  const char *class_name() const		{ return "InfiniteSource"; }
  Processing default_processing() const		{ return PUSH; }
  
  InfiniteSource *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();
  
};

#endif
