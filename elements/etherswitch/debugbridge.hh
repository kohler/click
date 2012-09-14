#ifndef CLICK_DEBUGBRIDGE_HH
#define CLICK_DEBUGBRIDGE_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

class DebugBridge : public Element {

  String _label;

 public:

  DebugBridge() CLICK_COLD;
  ~DebugBridge() CLICK_COLD;

  const char *class_name() const	{ return "DebugBridge"; }
  const char *port_count() const	{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
