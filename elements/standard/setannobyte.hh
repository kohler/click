#ifndef CLICK_SETANNOBYTE_HH
#define CLICK_SETANNOBYTE_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetAnnoByte(OFFSET, VALUE)

=s annotations

sets packet user annotations

=d

Sets each packet's user annotation byte at OFFSET to VALUE, an integer
0..255.  Permissible values for OFFSET are 0 to n-1, inclusive, where
n is typically 24.

=h offset read-only  
Returns OFFSET
=h value read-only   
Returns VALUE 

=a Paint */

class SetAnnoByte : public Element {
  
  unsigned int _offset;
  unsigned char _value;
 public:
  
  SetAnnoByte();
  ~SetAnnoByte();
  
  const char *class_name() const		{ return "SetAnnoByte"; }
  const char *processing() const		{ return AGNOSTIC; }

  bool can_live_reconfigure() const             { return true; }
  
  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);

  void add_handlers();
  static String offset_read_handler(Element *e, void *);
  static String value_read_handler(Element *e, void *);
};

CLICK_ENDDECLS
#endif
