#ifndef ICMPREWRITER_HH
#define ICMPREWRITER_HH
#include <click/element.hh>
#include <click/click_udp.h>
#include "elements/ip/iprw.hh"

/*
=c

ICMPRewriter(MAPS)

=s ICMP

rewrites ICMP packets based on IP rewriter mappings

=d

Rewrites ICMP packets by changing their source and/or destination addresses
and some of their payloads. This lets source quenches, redirects, TTL-expired
messages, and so forth pass through a NAT gateway.

MAPS is a space-separated list of IPRewriter-like elements.

=a

IPRewriter, ICMPPingRewriter, TCPRewriter */

class ICMPRewriter : public Element { public:

  ICMPRewriter();
  ~ICMPRewriter();

  const char *class_name() const	{ return "ICMPRewriter"; }
  const char *processing() const	{ return PUSH; }
  ICMPRewriter *clone() const		{ return new ICMPRewriter; }

  void notify_ninputs(int);
  int configure(const Vector<String> &, ErrorHandler *);

  void push(int, Packet *);
  
 protected:

  Vector<IPRw *> _maps;

  void rewrite_packet(WritablePacket *, click_ip *, click_udp *,
		      const IPFlowID &, IPRw::Mapping *);
  
};

#endif
