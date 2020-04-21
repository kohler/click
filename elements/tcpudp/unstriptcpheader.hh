#ifndef CLICK_UNSTRIPTCPHEADER_HH
#define CLICK_UNSTRIPTCPHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * UnstripTCPHeader()
 *
 * =s tcp
 *
 * restores outermost TCP header
 * =d
 *
 * Put outermost TCP header back onto a stripped packet, based on the TCP
 * Header annotation from MarkTCPHeader or CheckTCPHeader. If TCP header
 * already on,forwards packet unmodified.
 *
 * =a StripTCPHeader, CheckIPHeader, MarkIPHeader, StripIPHeader */

class UnstripTCPHeader : public Element { public:

  UnstripTCPHeader() CLICK_COLD;
  ~UnstripTCPHeader() CLICK_COLD;

  const char *class_name() const		{ return "UnstripTCPHeader"; }
  const char *port_count() const		{ return PORTS_1_1; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
