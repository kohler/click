#ifndef CLICK_FILTERTX_HH
#define CLICK_FILTERTX_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
FilterTX([, I<KEYWORDS>])

=s Wifi

Filter out wireless transmission feedback packets

=d

Filters out packets that were sent by this node, and
received via transmit feedback.
Sends these packets to output 1 if it is present,
otherwise it drops the packets.

=a ExtraEncap, ExtraDecap
*/


class FilterTX : public Element { public:

  FilterTX() CLICK_COLD;
  ~FilterTX() CLICK_COLD;

  const char *class_name() const		{ return "FilterTX"; }
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PROCESSING_A_AH; }

  void add_handlers() CLICK_COLD;
  static String static_print_drops(Element *, void *);
  static String static_print_max_failures(Element *, void *);
  static int static_write_max_failures(const String &arg, Element *e,
				void *, ErrorHandler *errh);

  static String static_print_allow_success(Element *, void *);
  static int static_write_allow_success(const String &arg, Element *e,
				void *, ErrorHandler *errh);
  Packet *simple_action(Packet *);

  int _drops;
};

CLICK_ENDDECLS
#endif
