#ifndef IPDECAPPAINT_HH
#define IPDECAPPAINT_HH
#include <click/element.hh>


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


class IPDecapPaint : public Element {
  
  unsigned _nbytes;
  
 public:
  
  IPDecapPaint();
  ~IPDecapPaint();
  
  const char *class_name() const	{ return "IPDecapPaint"; }
  const char *processing() const	{ return AGNOSTIC; }
  

  IPDecapPaint *clone() const		{ return new IPDecapPaint; }

  Packet *simple_action(Packet *);
  
};

#endif


