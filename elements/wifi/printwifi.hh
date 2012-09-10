#ifndef CLICK_PRINTWIFI_HH
#define CLICK_PRINTWIFI_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
 * =c
 * PrintWifi([TAG] [, KEYWORDS])
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

class PrintWifi : public Element {

  String _label;

 public:

  PrintWifi() CLICK_COLD;
  ~PrintWifi() CLICK_COLD;

  const char *class_name() const		{ return "PrintWifi"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

  bool _print_anno;
  bool _print_checksum;
  bool _timestamp;
};

CLICK_ENDDECLS
#endif
