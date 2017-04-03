#ifndef CLICK_IP6CLASSIFIER_HH
#define CLICK_IP6CLASSIFIER_HH
#include "elements/standard/classification.hh"
#include <click/element.hh>
#include <click/vector.hh>
#include "ip6classifier_AST.hh"
#include <click/timestamp.hh>

CLICK_DECLS

/*
=c

IP6Classifier(PATTERN_1, ..., PATTERN_N)

=s ip

classifies IP packets by contents

=d

This element does not do bound checking. Be sure the elements you gave have
their network header and transport layer header set. On behalve of the
transport layer header make sure it is a legimitate packet. On working with
extension headers, be sure they are not ill formed.

Patterns are built from I<pattern primitives>. The primitives
IPClassifier understands are:

=over 8
=item B<ip proto PROTO>

PROTO is a a valid IP protocol number (it is valid if it is between 0 and 255).
Matches packets of the given protocol.

=item B<[SRCORDST] host IP6ADDR>

IP6ADDR is an IPv6 address and SRCORDST is 'src' or 'dst'. 
Matches packets sent to and/or from the given  
source or destination address.

=item B<[SRCORDST] net NETADDR>

NETADDR is an IPv6 network address (either CIDR-style 'IP6ADDR/BITS' or
'IP6ADDR mask MASK') and SRCORDST is as above. Matches packets sent to
and/or from the given network.

=item B<[SRCORDST] [tcp | udp] port PORT>

PORT is a TCP or UDP port name (see below) or number and SRCORDST is as
above. Matches packets sent to and/or from the given TCP or UDP port. If
you leave out 'tcp' or 'udp', then either TCP or UDP is accepted.

=item B<ip6 vers VERSION>

VERSION is a value between 0 and 15. Matches IPv6 packets with the given
version.

=item B<ip6 plen PLEN>

PLEN is a value between 0 and 65535. Matches IPv6 packets with the packet payload
of PLEN bytes (extension headers if present are considered payload).

=item B<ip6 flow FLOW>

FLOW is a value between 0 and 1048575. Matches IPv6 packets with the given flow
label.

=item B<ip6 dscp DSCP>

DSCP is a value between 0 and 63. Matches IPv6 packets with the given DSCP
value (the upper 6 bits of traffic class).

=item B<ip6 ecn ECN>

ECN is a value between 0 and 3. Matches IPv6 packets with the ecn bits set 
to ECN (the lower 2 bits of traffic class).

=item B<ip6 ce>

Matches IPv6 packets with ECN Congestion Experienced set.

=item B<ip6 hlim HLIM>

HLIM is a value between 0 and 255. Matches IPv6 packets with the given HLIM value.

=item B<ip6 frag>

Matches fragmented IPv6 packets (that is, packets with the at least one fragmentation
extension header)

=item B<ip6 unfrag>

Equivalent to 'not ip6 frag'.

=item B<tcp opt TCPOPT>

TCPOPT is a TCP option name (see below). Matches TCP packets with the given
option.

=item B<tcp win TCPWIN>

TCPWIN is a TCP receive window length. Matches TCP packets with the given
window length. Note that window scaling is not applied.

=item B<icmp type TYPE>

TYPE is a value between 0 and 255 or an ICMP type name (see below). Matches
ICMP packets with the given ICMP type.

=item B<true>

Matches every packet.

=item B<false>

Matches no packets.

=back

These primitives can be combined with the connectives 'and', 'or', and 'not'.

All primitives accept an optional OPERATION, '==' or '!=', '>', '<', '>=', '<='
which can occur before the actual option unless stated otherwise.  If no OPERATION 
is specified, '==' is assumed.  An example: 'src host == 10.0.0.10' matches 
packets whose source host is 10.0.0.10; 'src host != 10.0.0.10' matches packets 
whose source host I<is not> 10.0.0.10.

=e

  classifier :: IP6Classifier(src host fa80::0202:b3ff:fe1e:8329 or hlim 20, dst host fa80::0202:b3ff:fe1e:8330 and frag)
  [0] classifier -> Print(matches the first pattern) -> Discard;
  [1] classifier -> Print(matches the second pattern) -> Discard;
  
=h program read-only
Returns a human-readable definition of the program the IPFilter element
is using to classify packets. At each step in the program, four bytes
of packet data are ANDed with a mask and compared against four bytes of
classifier pattern.

=a

IPClassifier, Classifier, CheckIP6Header, MarkIP6Header, tcpdump(1) */

class IP6Classifier : public Element { 
public:
    IP6Classifier() CLICK_COLD;
    ~IP6Classifier() CLICK_COLD;

    const char *class_name() const		{ return "IP6Classifier"; }
    const char *port_count() const		{ return "1/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    
    void push(int, Packet *packet);

private:
    Vector<ip6classification::AST> ast_list; // a list of ASTs (abstract syntax trees); there is 1 AST per pattern passed by an end-user.
};

CLICK_ENDDECLS
#endif /* IP6Classifier */
