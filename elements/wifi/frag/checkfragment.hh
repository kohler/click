#ifndef CLICK_CHECKFRAGMENT_HH
#define CLICK_CHECKFRAGMENT_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
CLICK_DECLS


class CheckFragment : public Element { public:
  
  CheckFragment();
  ~CheckFragment();

  const char *class_name() const	{ return "CheckFragment"; }
  const char *processing() const		{ return "a/ah"; }
  
  void notify_noutputs(int);
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();
  
  bool _header_only;

 private:

};

CLICK_ENDDECLS
#endif
