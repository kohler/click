#ifndef CLICK_FRAGMENTENCAP_HH
#define CLICK_FRAGMENTENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

class FragmentEncap : public Element { public:
  
  FragmentEncap();
  ~FragmentEncap();

  const char *class_name() const	{ return "FragmentEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();


  bool _debug;

  unsigned _frag_length;
  unsigned _packet_num;
 private:

};

CLICK_ENDDECLS
#endif
