#ifndef CLICK_PRINTPOWER_HH
#define CLICK_PRINTPOWER_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
 * =c
 * PrintPower([TAG] [, KEYWORDS])
 * =s debugging
 * =d
 * Assumes input packets are Wifi packets (ie a wifi_pkt struct from 
 * wifi.hh). Prints out a description of those packets.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =a
 * Print, Wifi
 */

class PrintPower : public Element {
 public:
  
  PrintPower();
  ~PrintPower();
  
  const char *class_name() const		{ return "PrintPower"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
