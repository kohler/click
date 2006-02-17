#ifndef CLICK_RANDOMSOURCE_HH
#define CLICK_RANDOMSOURCE_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * =c
 * RandomSource(LENGTH)
 * =s basicsources
 * generates random packets whenever scheduled
 * =d
 *
 * Creates packets, of the indicated length, filled with random bytes.
 * Packets' timestamp annotations are set to the current time.
 *
 * =a InfiniteSource
 */

class RandomSource : public Element { public:
  
  RandomSource();
  ~RandomSource(); 
 
  const char *class_name() const		{ return "RandomSource"; }
  const char *port_count() const		{ return PORTS_0_1; }
  const char *processing() const		{ return AGNOSTIC; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *pull(int);
  bool run_task();

 protected:
  
  int _length;
  Task _task;
  
  Packet *make_packet();

};

CLICK_ENDDECLS
#endif
