#ifndef DEBUGBRIDGE_HH
#define DEBUGBRIDGE_HH
#include <click/element.hh>
#include <click/string.hh>

class DebugBridge : public Element {
  
  String _label;
  
 public:
  
  DebugBridge();
  ~DebugBridge();
  
  const char *class_name() const		{ return "DebugBridge"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  DebugBridge *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
};

#endif
