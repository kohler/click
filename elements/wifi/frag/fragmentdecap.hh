#ifndef CLICK_FRAGMENTDECAP_HH
#define CLICK_FRAGMENTDECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
CLICK_DECLS


class FragmentDecap : public Element { public:
  
  FragmentDecap();
  ~FragmentDecap();

  const char *class_name() const	{ return "FragmentDecap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();


  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif
