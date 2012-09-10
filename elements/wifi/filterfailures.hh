#ifndef CLICK_FILTERFAILURES_HH
#define CLICK_FILTERFAILURES_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

FilterFailures([I<KEYWORDS>])

=s Wifi

Filters unicast packets that failed to be acknowledged

=d
Filters out packets that have the WIFI_EXTRA_TX_FAIL flag set.
Sends these packets to output 1 if it is present, otherwise it drops the packets.

=h drops read-only
How many packets had the WIFI_EXTRA_TX_FAIL flag set.
=a ExtraDecap
 */


class FilterFailures : public Element { public:

  FilterFailures() CLICK_COLD;
  ~FilterFailures() CLICK_COLD;

  const char *class_name() const		{ return "FilterFailures"; }
  const char *port_count() const		{ return "1/1-3"; }
  const char *processing() const		{ return PROCESSING_A_AH; }

  void add_handlers() CLICK_COLD;
  static String static_print_drops(Element *, void *);
  Packet *simple_action(Packet *);
  int _drops;
};

CLICK_ENDDECLS
#endif
