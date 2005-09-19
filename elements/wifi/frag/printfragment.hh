#ifndef CLICK_PRINTFRAGMENT_HH
#define CLICK_PRINTFRAGMENT_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
CLICK_DECLS

class PrintFragment : public Element { public:
  
  PrintFragment();
  ~PrintFragment();

  const char *class_name() const	{ return "PrintFragment"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  String _label;
 private:

};

CLICK_ENDDECLS
#endif
