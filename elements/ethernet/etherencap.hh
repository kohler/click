#ifndef ETHERENCAP_HH
#define ETHERENCAP_HH

/*
 * =c
 * EtherEncap(ETHERTYPE, SADDR, DADDR)
 * =s
 * encapsulates packets in Ethernet header
 * V<encapsulation>
 *
 * =d
 *
 * Encapsulates each packet in the Ethernet header specified by its arguments.
 * ETHERTYPE should be in host order.
 *
 * =e
 * Encapsulate packets in an Ethernet header with type
 * ETHERTYPE_IP (0x0800), source address 1:1:1:1:1:1, and
 * destination address 2:2:2:2:2:2:
 * 
 *   EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
 *
 * =n
 *
 * For IP packets you probably want to use ARPQuerier instead.
 *
 * =a
 * ARPQuerier */

#include <click/element.hh>
#include <click/click_ether.h>

class EtherEncap : public Element { public:
  
  EtherEncap();
  ~EtherEncap();

  const char *class_name() const	{ return "EtherEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  EtherEncap *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);
  
 private:

  click_ether _ethh;

};

#endif
