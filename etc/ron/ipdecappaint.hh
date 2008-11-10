#ifndef IPDECAPPAINT_HH
#define IPDECAPPAINT_HH

/*
 * =c
 * IPDecapPaint()
 * =s encapsulation, IP
 * Strips encapsulation header and annotation byte from front of packets. Packets that
 * are not IP in IP (IP protocol 4) are dropped.
 * =d
 * Removes the outermost IP header from IP packets based on the IP Header annotation.
 *
 * =a IPEncapPaint, IPEncap, IPEncap2, CheckIPHeader, CheckIPHeader2, MarkIPHeader, UnstripIPHeader
 */

#include <click/element.hh>

class IPDecapPaint : public Element {

  unsigned _nbytes;

 public:

  IPDecapPaint();
  ~IPDecapPaint();

  const char *class_name() const	{ return "IPDecapPaint"; }
  const char *port_count() const	{ return "1/1"; }
  const char *processing() const	{ return AGNOSTIC; }

  Packet *simple_action(Packet *);
  void add_handlers();
};

#endif


