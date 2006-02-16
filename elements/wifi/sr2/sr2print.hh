#ifndef CLICK_SR2PRINT_HH
#define CLICK_SR2PRINT_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
 * =c
 * SR2Print([TAG] [, KEYWORDS])
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

class SR2Print : public Element {
  
  String _label;
  
 public:
  
  SR2Print();
  ~SR2Print();
  
  const char *class_name() const		{ return "SR2Print"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  static String sr_to_string(struct sr2packet *);
  bool _print_anno;
  bool _print_checksum;
};

CLICK_ENDDECLS
#endif
