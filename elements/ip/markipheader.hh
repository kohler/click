#ifndef MARKIPHEADER_HH
#define MARKIPHEADER_HH

/*
 * =c
 * MarkIPHeader([OFFSET])
 * =s IP, annotations
 * sets IP header annotation
 * =d
 *
 * Marks packets as IP packets by setting the IP Header annotation. The IP
 * header starts OFFSET bytes into the packet. Default OFFSET is 0.
 *
 * Does not check length fields for sanity, shorten packets to the IP length,
 * or set the destination IP address annotation. Use CheckIPHeader or
 * CheckIPHeader2 for that.
 *
 * =a CheckIPHeader, CheckIPHeader2, StripIPHeader */

#include <click/element.hh>

class MarkIPHeader : public Element {

  int _offset;
  
 public:
  
  MarkIPHeader();
  ~MarkIPHeader();
  
  const char *class_name() const		{ return "MarkIPHeader"; }
  MarkIPHeader *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
};

#endif
