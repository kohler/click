#ifndef UNQUEUE_HH
#define UNQUEUE_HH
#include <click/element.hh>

/*
 * =c
 * Unqueue([BURSTSIZE])
 * =s packet scheduling
 * pull-to-push converter
 * =d
 * Pulls packets whenever they are available, then pushes them out
 * its single output. Pulls a maximum of BURSTSIZE packets every time
 * it is scheduled. Default BURSTSIZE is 1. If BURSTSIZE
 * is 0, pull until nothing comes back.
 *
 * =a RatedUnqueue, BandwidthRatedUnqueue
 */

class Unqueue : public Element {

  int _burst;
  unsigned _packets;

 public:
  
  Unqueue();
  ~Unqueue();
  
  const char *class_name() const		{ return "Unqueue"; }
  const char *processing() const		{ return PULL_TO_PUSH; }
  
  Unqueue *clone() const			{ return new Unqueue; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  void run_scheduled();

  static String read_param(Element *e, void *);
};

#endif
