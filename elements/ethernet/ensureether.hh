#ifndef ENSUREETHER_HH
#define ENSUREETHER_HH

/*
=c

EnsureEther([ETHERTYPE, SRC, DST])

=s encapsulation, Ethernet

ensures that IP packets are Ethernet encapsulated

=d

Ensures that IP packets are encapsulated in an Ethernet header. Non-IP
packets, and IP packets that look Ethernet-encapsulated, are emitted on the
first output unchanged. Other IP packets are encapsulated in an Ethernet
header before being emitted. If the IP packet looks like it had an Ethernet
header that was stripped off, then that header is used. Otherwise, the header
specified by the arguments is prepended to the packet.

=e

Encapsulate packets in an Ethernet header with type
ETHERTYPE_IP (0x0800), source address 1:1:1:1:1:1, and
destination address 2:2:2:2:2:2:

  EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)

=n

For IP packets you probably want to use ARPQuerier instead.

=a

EtherEncap */

#include <click/element.hh>
#include <click/click_ether.h>

class EnsureEther : public Element { public:
  
  EnsureEther();
  ~EnsureEther();

  const char *class_name() const	{ return "EnsureEther"; }
  EnsureEther *clone() const		{ return new EnsureEther; }

  const char *processing() const	{ return AGNOSTIC; }  
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);
  
 private:

  click_ether _ethh;
  
};

#endif
