/* -*- c-basic-offset: 2 -*- */
#ifndef ICMPREWRITER_HH
#define ICMPREWRITER_HH
#include <click/element.hh>
#include <click/click_udp.h>
#include <click/click_icmp.h>
#include "elements/ip/iprw.hh"
#include "elements/icmp/icmppingrewriter.hh"

/*
=c

ICMPRewriter(MAPS)

=s ICMP

rewrites ICMP packets based on IP rewriter mappings

=d

Rewrites ICMP error packets by changing their source and/or destination
addresses and some of their payloads. It checks MAPS, a space-separated list
of IPRewriter-like elements, to see how to rewrite. This lets source quenches,
redirects, TTL-expired messages, and so forth pass through a NAT gateway.

ICMP error packets are sent in response to normal IP packets, and include a
small portion of the relevant IP packet data. If the IP packet had been sent
through IPRewriter, ICMPPingRewriter, or a similar element, then the ICMP
packet will be in response to the rewritten address. ICMPRewriter takes such
ICMP error packets and checks a list of IPRewriters for a relevant mapping. If
a mapping is found, ICMPRewriter will rewrite the ICMP packet so it appears
like a response to the original packet and emit the result on output 0.

ICMPRewriter may have one or two outputs. If it has one, then any
non-rewritten ICMP error packets, and any ICMP packets that are not errors,
are dropped. If it has two, then these kinds of packets are emitted on output
1.

=n

ICMPRewriter supports the following ICMP types: destination unreachable, time
exceeded, parameter problem, source quench, and redirect.

MAPS elements may have element class IPAddrRewriter, IPRewriter, TCPRewriter,
ICMPPingRewriter, or other related classes.

=a

IPAddrRewriter, IPRewriter, ICMPPingRewriter, TCPRewriter */

class ICMPRewriter : public Element { public:

  ICMPRewriter();
  ~ICMPRewriter();

  const char *class_name() const	{ return "ICMPRewriter"; }
  const char *processing() const	{ return PUSH; }
  ICMPRewriter *clone() const		{ return new ICMPRewriter; }

  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);

  void push(int, Packet *);
  
 protected:

  Vector<IPRw *> _maps;
  Vector<ICMPPingRewriter *> _ping_maps;

  void rewrite_packet(WritablePacket *, click_ip *, click_udp *,
		      const IPFlowID &, IPRw::Mapping *);
  void rewrite_ping_packet(WritablePacket *, click_ip *, icmp_sequenced *,
			   const IPFlowID &, ICMPPingRewriter::Mapping *);
  
};

#endif
