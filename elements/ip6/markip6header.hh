#ifndef MARKIP6HEADER_HH
#define MARKIP6HEADER_HH

/*
 * =c
 * MarkIP6Header([OFFSET])
 * =d
 *
 * Marks packets as IP6 packets by setting the IP6 Header annotation. The IP6
 * header starts OFFSET bytes into the packet. Default OFFSET is 0. Does not
 * check length fields for sanity or shorten packets to the IP length; use
 * CheckIPHeader or CheckIPHeader2 for that.
 *
 * =a CheckIP6Header
 * =a CheckIP6Header2
 * =a StripIP6Header */

#include "element.hh"

class MarkIP6Header : public Element {

  int _offset;
  
 public:
  
  MarkIP6Header();
  
  const char *class_name() const		{ return "MarkIP6Header"; }
  int configure(const Vector<String> &, ErrorHandler *);
  
  MarkIP6Header *clone() const;

  Packet *simple_action(Packet *);
  
};

#endif
