#ifndef STRIPIPHEADER_HH
#define STRIPIPHEADER_HH

/*
 * =c
 * StripIPHeader()
 * =d
 * Removes the outermost IP header from IP packets based on the IP Header annotation.
 *
 * =a CheckIPHeader
 * =a CheckIPHeader2
 * =a MarkIPHeader
 */

#include "element.hh"

class StripIPHeader : public Element {

 public:
  
  StripIPHeader();
  
  const char *class_name() const		{ return "StripIPHeader"; }
  StripIPHeader *clone() const			{ return new StripIPHeader; }

  Packet *simple_action(Packet *);
  
};

#endif
