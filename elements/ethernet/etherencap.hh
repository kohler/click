#ifndef ETHERENCAP_HH
#define ETHERENCAP_HH

/*
 * =c
 * EtherEncap(EtherType, SourceAddr, DestAddr)
 * =d
 * Encapsulates each packet in an ethernet header.
 * The EtherType specified in the configuration argument
 * should be in host order; the element
 * will convert it to network byte order.
 * =e
 * Encapsulate packets in an ethernet header with type
 * ETHERTYPE_IP (0x0800):
 * 
 * EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
 *
 * =n
 * For IP packets you probably want to use ArpQuerier instead.
 *
 * =a ArpQuerier
 */

#include "element.hh"

class EtherAddress;

class EtherEncap : public Element {
  
 public:
  
  EtherEncap();
  ~EtherEncap();

  const char *class_name() const		{ return "EtherEncap"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  EtherEncap *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
private:

  unsigned char _dst[6];
  unsigned char _src[6];
  unsigned short _netorder_type;
  int _type;

};

#endif
