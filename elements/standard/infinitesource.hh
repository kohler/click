#ifndef INFINITESOURCE_HH
#define INFINITESOURCE_HH
#include "element.hh"

/*
 * =c
 * InfiniteSource([DATA, LIMIT, BURSTSIZE, ACTIVE])
 * =s generates packets whenever scheduled
 * =d
 * Creates packets consisting of DATA. Pushes BURSTSIZE such packets
 * out its single output every time it is scheduled (which will be often).
 * Stops sending after LIMIT packets are generated; but if LIMIT is negative,
 * sends packets forever.
 * Will send packets only if ACTIVE is true. (ACTIVE is true by default.)
 * Default DATA is at least 64 bytes long. Default LIMIT is -1 (send packets
 * forever). Default BURSTSIZE is 1.
 *
 * To generate a particular traffic pattern, use this element and RatedSource
 * in conjunction with PokeHandlers.
 * =e
 *   InfiniteSource(\<0800>) -> Queue -> ...
 * =n
 * Useful for profiling and experiments.
 * =h count read-only
 * Returns the total number of packets that have been generated.
 * =h reset write-only
 * Resets the number of generated packets to 0. The InfiniteSource will then
 * generate another LIMIT packets (if it is active).
 * =h data read/write
 * Returns or sets the DATA parameter.
 * =h limit read/write
 * Returns or sets the LIMIT parameter.
 * =h burstsize read/write
 * Returns or sets the BURSTSIZE parameter.
 * =h active read/write
 * Makes the element active or inactive.
 * =a RatedSource, PokeHandlers
 */

class InfiniteSource : public Element { protected:
  
  String _data;
  int _burstsize;
  int _limit;
  int _count;
  bool _active;
  Packet *_packet;
  
  static String read_param(Element *, void *);
  static int change_param(const String &, Element *, void *, ErrorHandler *);
  
 public:
  
  InfiniteSource();
  
  const char *class_name() const		{ return "InfiniteSource"; }
  const char *processing() const		{ return PUSH; }
  void add_handlers();
  
  InfiniteSource *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  void uninitialize();

  void run_scheduled();
  
};

#endif
