#ifndef CLICK_PRINTSR_HH
#define CLICK_PRINTSR_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
 * =c
 * PrintSR([TAG] [, KEYWORDS])
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

class PrintSR : public Element {
  
  String _label;
  
 public:
  
  PrintSR();
  ~PrintSR();
  
  const char *class_name() const		{ return "PrintSR"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

  bool _print_anno;
  bool _print_checksum;
};

CLICK_ENDDECLS
#endif
