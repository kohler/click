#ifndef COLLISIONDETECTION_HH
#define COLLISIONDETECTION_HH


/*
 * =c
 * CollisionDetection([I<KEYWORDS>])
 * =s Wifi
 * Perform collission detection
 *
 * =d
 *
 * blah blah blah.
 *
 * =back
 *
 */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>

CLICK_DECLS

class CollisionDetection : public Element {
public:
  
  CollisionDetection();
  ~CollisionDetection();
  
  const char *class_name() const		{ return "CollisionDetection"; }
  const char *processing() const		{ return PUSH; }

  int initialize (ErrorHandler *);
  void add_handlers();
  
  static String static_read_collisions(Element *e, void *);
  static String static_read_delay(Element *e, void *);
  static String static_read_p(Element *e, void *);

  int configure(Vector<String> &, ErrorHandler *);

  void push(int port, Packet *);
  void run_timer();

  Timer _timer;
  Packet *_p;

  int _collisions;
  struct timeval _busy_until;
  struct timeval _delay;
};

CLICK_ENDDECLS
#endif
