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

Sets each packet's user annotation at OFFSET to VALUE, an integer 0..255. 

=n

VALUE is stored in user annotation OFFSET.

=a Paint */

class SetAnnoByte : public Element {
  
  unsigned char _value;
  int _offset;
 public:
  
  SetAnnoByte();
  ~SetAnnoByte();
  
  const char *class_name() const		{ return "SetAnnoByte"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetAnnoByte *clone() const;
  
  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);

  void add_handlers();
  static String offset_read_handler(Element *e, void *);
  static String value_read_handler(Element *e, void *);
};

CLICK_ENDDECLS
#endif
