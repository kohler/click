#ifndef CLICK_SETSRFLAG_HH
#define CLICK_SETSRFLAG_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
 * =c
 * SetSRFlag([TAG] [, KEYWORDS])
 * =s debugging
 * =d
 * Assumes input packets are SR packets (ie a sr_pkt struct from 
 * sr.hh). Prints out a description of those packets.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =a
 * Print, SR
 */

class SetSRFlag : public Element {
  
  uint16_t _flags;
 public:
  
  SetSRFlag();
  ~SetSRFlag();
  
  const char *class_name() const		{ return "SetSRFlag"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

  void add_handlers();
  static String static_print_flags(Element *e, void *);
  String print_flags();
};

CLICK_ENDDECLS
#endif
