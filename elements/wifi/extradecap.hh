#ifndef CLICK_EXTRADECAP_HH
#define CLICK_EXTRADECAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c
ExtraDecap()

=s wifi, decapsulation

=d
Removes the extra header and copies to to Packet->anno(). This contains
informatino such as rssi, noise, bitrate, etc.

=a ExtraEncap
*/

class ExtraDecap : public Element { public:
  
  ExtraDecap();
  ~ExtraDecap();

  const char *class_name() const	{ return "ExtraDecap"; }
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
