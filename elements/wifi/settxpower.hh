#ifndef CLICK_SETTXPOWER_HH
#define CLICK_SETTXPOWER_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * SetTXPower()
 * 
 * =s wifi
 * 
 * Sets the Wifi TXPower Annotation on a packet.
 * =over 8
 *
 * =item POWER
 * 
 * Unsigned integer. Valid powers are > 0 and < 200
 *
 * =item AUTO
 * 
 * Boolean. Use auto power scaling. Default is false.
 *
 */

class SetTXPower : public Element { public:
  
  SetTXPower();
  ~SetTXPower();
  
  const char *class_name() const		{ return "SetTXPower"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  
  Packet *simple_action(Packet *);

  void add_handlers();
  static String power_read_handler(Element *e, void *);
  static String auto_read_handler(Element *e, void *);
 private:

  int _power;
  bool _auto;
};

CLICK_ENDDECLS
#endif
