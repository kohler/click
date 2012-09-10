#ifndef CLICK_ALIGN_HH
#define CLICK_ALIGN_HH
#include <click/element.hh>
CLICK_DECLS

/* =c
 * Align(MODULUS, OFFSET)
 * =s basicmod
 * aligns packet data
 * =d
 *
 * Aligns packet data. Each input packet is aligned so that its first byte is
 * OFFSET bytes off from a MODULUS-byte boundary. This may involve a packet
 * copy.
 *
 * MODULUS must be 2, 4, or 8.
 * =n
 *
 * The click-align(1) tool will insert this element automatically wherever it
 * is required.
 *
 * =e
 *   ... -> Align(4, 0) -> ...
 *
 * =a AlignmentInfo, click-align(1) */

class Align : public Element {

  int _offset;
  int _mask;

 public:

  Align() CLICK_COLD;

  const char *class_name() const		{ return "Align"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);

};

CLICK_ENDDECLS
#endif
