#ifndef CLICK_PRINTSEQ_HH
#define CLICK_PRINTSEQ_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
 * =c
 * PrintSeq([TAG] [, KEYWORDS])
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

class PrintSeq : public Element {
  
  String _label;
  
 public:
  
  PrintSeq();
  ~PrintSeq();
  
  const char *class_name() const		{ return "PrintSeq"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

  bool _print_anno;
  bool _print_checksum;
  unsigned _offset;
  unsigned  _bytes;
};

CLICK_ENDDECLS
#endif
