#ifndef CLICK_PRINTDSR_HH
#define CLICK_PRINTDSR_HH
#include <click/element.hh>
#include <click/string.hh>
#include "srcr.hh"
CLICK_DECLS

/*
 * =c
 * PrintSRCR([TAG] [, KEYWORDS])
 * =s debuggin
 * =d
 * Assumes input packets are DSR packets (ie a sr_pkt struct from 
 * SRCR.hh). Prints out a description of those packets.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =a
 * Print, SRCR
 */

class PrintSRCR : public Element {
  
  String _label;
  uint16_t _et;     // This protocol's ethertype.
  
 public:
  
  PrintSRCR();
  ~PrintSRCR();
  
  const char *class_name() const		{ return "PrintSRCR"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  PrintSRCR *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
