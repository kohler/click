#ifndef CLICK_PRINTDSR_HH
#define CLICK_PRINTDSR_HH
#include <click/element.hh>
#include <click/string.hh>
#include "rtmdsr.hh"
CLICK_DECLS

/*
 * =c
 * PrintDSR([TAG] [, KEYWORDS])
 * =s debuggin
 * =d
 * Assumes input packets are DSR packets (ie a sr_pkt struct from 
 * RTMDSR.hh). Prints out a description of those packets.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =a
 * Print, RTMDSR
 */

class PrintDSR : public Element {
  
  String _label;
  uint16_t _et;     // This protocol's ethertype.
  
 public:
  
  PrintDSR();
  ~PrintDSR();
  
  const char *class_name() const		{ return "PrintDSR"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  PrintDSR *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
