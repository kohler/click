#ifndef UNSTRIPIPHEADER_HH
#define UNSTRIPIPHEADER_HH

/*
 * =c
 * UnstripIPHeader()
 * =s restores outermost IP header
 * V<strips>
 * =d
 *
 * Put outermost IP header back onto a striped packet, based on the IP Header
 * annotation from MarkIPHeader or CheckIPHeader. If IP header already on,
 * forwards packet unmodified.
 *
 * =a CheckIPHeader, MarkIPHeader, StripIPHeader */

#include "element.hh"

class UnstripIPHeader : public Element {

 public:
  
  UnstripIPHeader();
  
  const char *class_name() const		{ return "UnstripIPHeader"; }
  UnstripIPHeader *clone() const		{ return new UnstripIPHeader; }

  Packet *simple_action(Packet *);
};

#endif
