#ifndef CLICK_SETWIFIEXTRAFLAG_HH
#define CLICK_SETWIFIEXTRAFLAG_HH
#include <click/element.hh>
#include <click/glue.hh>

CLICK_DECLS

/*
 * =c
 * 
 * SetWifiExtraFlag(Unsigned)
 * 
 * =s Wifi
 * 
 * Sets the Wifi flags on a packet.
 * 
 */

class SetWifiExtraFlag : public Element { public:
  
  SetWifiExtraFlag();
  ~SetWifiExtraFlag();
  
  const char *class_name() const		{ return "SetWifiExtraFlag"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  Packet *simple_action(Packet *);

  void add_handlers();
  unsigned _flag;
};

CLICK_ENDDECLS
#endif
