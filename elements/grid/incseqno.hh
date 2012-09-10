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
 * specified offset.  The number is incremented with each packet that
 * passes through the element.
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
 * =item NET_BYTE_ORDER
 *
 * Boolean.  Should the sequence number be stored in network byte
 * order?  If not, host byte order is used.  Defaults to false.
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

  IncrementSeqNo() CLICK_COLD;
  ~IncrementSeqNo() CLICK_COLD;

  const char *class_name() const		{ return "IncrementSeqNo"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void add_handlers() CLICK_COLD;

  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

private:

  static String next_seq(Element *e, void *);
  static int write_seq(const String &in_s, Element *e, void *vparam,
		       ErrorHandler *errh);
  uint32_t _seqno;
  unsigned int _offset;
  bool _use_net_byteorder;
};

CLICK_ENDDECLS
#endif
