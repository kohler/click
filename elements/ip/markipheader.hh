#ifndef MARKIPHEADER_HH
#define MARKIPHEADER_HH

/*
 * =c
 * MarkIPHeader([OFFSET])
 * =d
 * Marks packets as IP packets by setting the IP Header annotation.
 * The IP header starts OFFSET bytes into the packet.
 * Default OFFSET is 0.
 *
 * =a CheckIPHeader
 * =a CheckIPHeader2
 * =a StripIPHeader
 */

#include "element.hh"

class MarkIPHeader : public Element {

  int _offset;
  
 public:
  
  MarkIPHeader();
  
  const char *class_name() const		{ return "MarkIPHeader"; }
  int configure(const String &, ErrorHandler *);
  
  MarkIPHeader *clone() const;

  Packet *simple_action(Packet *);
  
};

#endif
