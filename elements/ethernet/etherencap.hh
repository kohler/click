#ifndef ETHERENCAP_HH
#define ETHERENCAP_HH

/*
 * =c
 * EtherEncap(ETHERTYPE, SADDR, DADDR)
 * =s encapsulates packets in Ethernet header
 * =d
 * Encapsulates each packet in an Ethernet header.
 * The ETHERTYPE specified in the configuration argument
 * should be in host order; the element
 * will convert it to network byte order.
 * =e
 * Encapsulate packets in an Ethernet header with type
 * ETHERTYPE_IP (0x0800), source address 1:1:1:1:1:1, and
 * destination address 2:2:2:2:2:2:
 * 
 *   EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
 *
 * =n
 * For IP packets you probably want to use ARPQuerier instead.
 *
 * =a
 * ARPQuerier
 */

#include "element.hh"
class EtherAddress;

class EtherEncap : public Element {
  
 public:
  
  EtherEncap();
  ~EtherEncap();

  const char *class_name() const		{ return "EtherEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  EtherEncap *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);
  
private:

  unsigned char _dst[6];
  unsigned char _src[6];
  unsigned short _netorder_type;
  int _type;

};

#endif
