#ifndef CLICK_MARKIP6HEADER_HH
#define CLICK_MARKIP6HEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * MarkIP6Header([OFFSET])
 * =s ip6
 *
 * =d
 *
 * Marks packets as IP6 packets by setting the IP6 Header annotation. The IP6
 * header starts OFFSET bytes into the packet. Default OFFSET is 0. Does not
 * check length fields for sanity or shorten packets to the IP length; use
 * CheckIPHeader or CheckIPHeader2 for that.
 *
 * =a CheckIP6Header, CheckIP6Header2, StripIP6Header */

class MarkIP6Header : public Element {

  int _offset;

 public:

  MarkIP6Header();
  ~MarkIP6Header();

  const char *class_name() const		{ return "MarkIP6Header"; }
  const char *port_count() const		{ return PORTS_1_1; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
