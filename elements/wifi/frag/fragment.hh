#ifndef CLICK_FRAGMENT_HH
#define CLICK_FRAGMENT_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

class Fragment : public Element { public:
  
  Fragment();
  ~Fragment();

  const char *class_name() const	{ return "Fragment"; }
  const char *processing() const	{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  void push(int, Packet *);


  void add_handlers();


  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif
