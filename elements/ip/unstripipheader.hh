#ifndef CLICK_UNSTRIPIPHEADER_HH
#define CLICK_UNSTRIPIPHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * UnstripIPHeader()
 * =s encapsulation, IP
 * restores outermost IP header
 * =d
 *
 * Put outermost IP header back onto a striped packet, based on the IP Header
 * annotation from MarkIPHeader or CheckIPHeader. If IP header already on,
 * forwards packet unmodified.
 *
 * =a CheckIPHeader, MarkIPHeader, StripIPHeader */

class UnstripIPHeader : public Element { public:
  
  UnstripIPHeader();
  ~UnstripIPHeader();
  
  const char *class_name() const		{ return "UnstripIPHeader"; }
  UnstripIPHeader *clone() const		{ return new UnstripIPHeader; }

  Packet *simple_action(Packet *);
  
};

CLICK_ENDDECLS
#endif
