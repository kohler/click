#ifndef CLICK_SETRTS_HH
#define CLICK_SETRTS_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <elements/wifi/sr/ettmetric.hh>
CLICK_DECLS

/*
=c

SetRTS(Bool)

=s wifi

=d
Enable/disable RTS/CTS for a packet

=h rts read/write
Enable/disable rts/cts for a packet.

=a ExtraEncap, ExtraDecap
*/

class SetRTS : public Element { public:
  
  SetRTS();
  ~SetRTS();
  
  const char *class_name() const		{ return "SetRTS"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  Packet *simple_action(Packet *);

  void add_handlers();
  bool _rts;
};

CLICK_ENDDECLS
#endif
