#ifndef CLICK_ICMPPINGRESPONDER_HH
#define CLICK_ICMPPINGRESPONDER_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

ICMPPingResponder()

=s ICMP

responds to ICMP echo requests

=d

Respond to ICMP echo requests. Incoming packets must have their IP header
annotations set. The corresponding reply is generated for any ICMP echo
request and emitted on output 0. The reply's destination IP address annotation
is set appropriately, its paint annotation is cleared, and its timestamp is
set to the current time. Other annotations are copied from the input packet.
IP packets other than ICMP echo requests are emitted on the second output, if
there are two outputs; otherwise, they are dropped.

=head1 BUGS

ICMPPingResponder does not pay attention to source route options; it should.

=a

ICMPSendPings, ICMPError */

class ICMPPingResponder : public Element { public:
  
  ICMPPingResponder();
  ~ICMPPingResponder();
  
  const char *class_name() const	{ return "ICMPPingResponder"; }
  const char *processing() const	{ return "a/ah"; }  
  ICMPPingResponder *clone() const;

  void notify_noutputs(int);
  
  Packet *simple_action(Packet *);
  
};

CLICK_ENDDECLS
#endif
