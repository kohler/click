#ifndef CLICK_LOCATIONHISTOGRAM_HH
#define CLICK_LOCATIONHISTOGRAM_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * LocationHistogram()
 * 
 * =s wifi
 * 
 * Accumulate locationhistogram for each ethernet src you hear a packet from.
 * =over 8
 *
 *
 */


class LocationHistogram : public Element { public:
  
  LocationHistogram();
  ~LocationHistogram();
  
  const char *class_name() const		{ return "LocationHistogram"; }
  const char *processing() const		{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void push (int port, Packet *p_in);

  void add_handlers();


  unsigned _length;
  Vector<int> _byte_errors;
};

CLICK_ENDDECLS
#endif
