#ifndef INCSEQNO_HH
#define INCSEQNO_HH

/*
 * =c
 * IncrementSeqNo([I<KEYWORDS>])
 *
 * =s Grid
 * =d 
 *
 * Store a 32-bit unsigned integer sequence number in packets, at a
 * specified offset.  The number is stored in network byte order, and
 * is incremented with each packet that passes through the element.
 *
 * Keywords are:
 *
 * =over 8
 *
 * =item OFFSET
 *
 * Unsigned integer.  Byte offset at which to store sequence number.
 * Defaults to 0.
 *
 * =item FIRST
 *
 * Unsigned integer.  First sequence number to use.  Defaults to 0.
 *
 * =back
 *
 * =h seq read-only
 *
 * The next sequence number that will be written into a packet.
 *
 * =a 
 * InfiniteSource */

#include <click/element.hh>

CLICK_DECLS

class IncrementSeqNo : public Element  {
  public:

  IncrementSeqNo();
  ~IncrementSeqNo();

  const char *class_name() const		{ return "IncrementSeqNo"; }
  const char *processing() const		{ return AGNOSTIC; }
  IncrementSeqNo *clone() const;
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  
  Packet *simple_action(Packet *);

private:

  static String next_seq(Element *e, void *);

  uint32_t _seqno;
  unsigned int _offset;
};

CLICK_ENDDECLS
#endif
