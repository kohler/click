#ifndef CLICK_DEBUGBRIDGE_HH
#define CLICK_DEBUGBRIDGE_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

class DebugBridge : public Element {
  
  String _label;
  
 public:
  
  DebugBridge();
  ~DebugBridge();
  
  const char *class_name() const		{ return "DebugBridge"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  DebugBridge *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
