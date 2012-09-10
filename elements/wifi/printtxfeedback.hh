#ifndef CLICK_PRINTTXFEEDBACK_HH
#define CLICK_PRINTTXFEEDBACK_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
 * =c
 * PrintTXFeedback([TAG] [, KEYWORDS])
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

class PrintTXFeedback : public Element {

  String _label;

 public:

  PrintTXFeedback() CLICK_COLD;
  ~PrintTXFeedback() CLICK_COLD;

  const char *class_name() const		{ return "PrintTXFeedback"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

  bool _print_anno;
  bool _print_checksum;
  unsigned _offset;
};

CLICK_ENDDECLS
#endif
