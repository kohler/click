#ifndef CLICK_ENSUREETHER_HH
#define CLICK_ENSUREETHER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

EnsureEther([ETHERTYPE, SRC, DST])

=s ethernet

ensures that IP packets are Ethernet encapsulated

=d

Ensures that IP packets are encapsulated in an Ethernet header. Non-IP
packets, and IP packets that look Ethernet-encapsulated, are emitted on the
first output unchanged. Other IP packets are encapsulated in an Ethernet
header before being emitted. If the IP packet looks like it had an Ethernet
header that was stripped off, then that header is used. Otherwise, the header
specified by the arguments is prepended to the packet.

=e

Encapsulate packets without an Ethernet header with type
ETHERTYPE_IP (0x0800), source address 1:1:1:1:1:1, and
destination address 2:2:2:2:2:2:

  EnsureEther(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)

=a

EtherEncap, EtherRewrite */

class EnsureEther : public Element { public:

  EnsureEther() CLICK_COLD;
  ~EnsureEther() CLICK_COLD;

  const char *class_name() const	{ return "EnsureEther"; }
  const char *port_count() const	{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);

 private:

  click_ether _ethh;

};

CLICK_ENDDECLS
#endif
