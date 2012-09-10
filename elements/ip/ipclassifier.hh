#ifndef CLICK_IPCLASSIFIER_HH
#define CLICK_IPCLASSIFIER_HH
#include "elements/ip/ipfilter.hh"
CLICK_DECLS

/*
=c
IPClassifier(PATTERN_1, ..., PATTERN_N)

=s ip
classifies IP packets by contents

=d

Classifies IP packets according to tcpdump(1)-like patterns. The
IPClassifier has N outputs, each associated with the corresponding pattern
from the configuration string. The input packets must have their IP header
annotation set; CheckIPHeader and MarkIPHeader do this.

Patterns are built from I<pattern primitives>. The primitives
IPClassifier understands are:

=over 8
=item B<ip proto PROTO>

PROTO is an IP protocol name (see below) or a valid IP protocol number.
Matches packets of the given protocol.

=item B<[SRCORDST] host IPADDR>

IPADDR is an IP address and SRCORDST is 'src', 'dst', 'src or dst', or 'src
and dst'. (If SRCORDST is missing, 'src or dst' is assumed.) Matches
packets sent to and/or from the given address.

=item B<[SRCORDST] net NETADDR>

NETADDR is an IP network address (either CIDR-style 'IPADDR/BITS' or
'IPADDR mask MASK') and SRCORDST is as above. Matches packets sent to
and/or from the given network.

=item B<[SRCORDST] [tcp | udp] port PORT>

PORT is a TCP or UDP port name (see below) or number and SRCORDST is as
above. Matches packets sent to and/or from the given TCP or UDP port. If
you leave out 'tcp' or 'udp', then either TCP or UDP is accepted.

=item B<ip vers VERSION>

VERSION is a value between 0 and 15. Matches IP packets with the given
version.

=item B<ip hl HL>

HL is a value between 0 and 15. Matches IP packets with the given header
length.

=item B<ip id ID>

ID is a value between 0 and 65535. Matches IP packets with the given IP ID.

=item B<ip tos TOS>

TOS is a value between 0 and 255. Matches IP packets with the given TOS
value.

=item B<ip dscp DSCP>

DSCP is a value between 0 and 63. Matches IP packets with the given DSCP
value (the upper 6 bits of TOS).

=item B<ip ect>

Matches IP packets with ECN Capable Transport turned on.

=item B<ip ce>

Matches IP packets with ECN Congestion Experienced set.

=item B<ip ttl TTL>

TTL is a value between 0 and 255. Matches IP packets with the given TTL value.

=item B<ip frag>

Matches fragmented IP packets (that is, packets with the more-fragments bit
set and/or a nonzero fragment offset).

=item B<ip unfrag>

Equivalent to 'not ip frag'.

=item B<tcp opt TCPOPT>

TCPOPT is a TCP option name (see below). Matches TCP packets with the given
option.

=item B<tcp win TCPWIN>

TCPWIN is a TCP receive window length. Matches TCP packets with the given
window length. Note that window scaling is not applied.

=item B<icmp type TYPE>

TYPE is a value between 0 and 255 or an ICMP type name (see below). Matches
ICMP packets with the given ICMP type.

=item B<ip[POS:LEN] VALUE>

Matches packets whose IP header field starting at byte position POS, and going
on for LEN bytes, equals VALUE.  You can say B<ip[POS]> instead of
B<ip[POS:1]>.

=item B<transp[POS:LEN] VALUE>

Like B<ip[POS:LEN]>, but for transport header fields.  You can also give
particular transport protocols, such as B<tcp[POS:LEN]>.

=item B<[SRCORDST] ether host ETH>

Matches packets whose source and/or destination Ethernet address matches ETH.
The packet's MAC header annotation must be set.

=item B<true>

Matches every packet.

=item B<false>

Matches no packets.

=back

These primitives can be combined with the connectives 'and', 'or', and 'not'
V<>(synonyms '&&', '||', and '!'), with the ternary operator '?:' (the colon
must be surrounded by spaces!), and with parentheses. For example, '(dst port
www or dst port ssh) and tcp opt syn'.

All primitives except B<tcp opt> accept an optional OPERATION, '==' or '!=',
which can occur before the actual option.  If no OPERATION is specified, '=='
is assumed.  'src host == 10.0.0.10' matches packets whose source host is
10.0.0.10; 'src host != 10.0.0.10' matches packets whose source host I<is not>
10.0.0.10.  Directives with integer values also support the '<', '>', '<=',
and '>=' operations.

For B<port> and B<icmp type> directives, 'DIRECTIVE != VALUE' is not the
same as 'not (DIRECTIVE == VALUE)'. For example, 'src tcp port != 5'
matches TCP packets whose source port is not 5, while '!(src tcp port ==
5)' matches non-TCP packets as well. (The 'src tcp port != 5' option is
effectively equivalent to 'tcp and not src tcp port 5'.) Similarly, 'icmp
type != 4' will not match non-ICMP packets. The same goes for the '<', '>',
'<=', and '>=' relations.

Most primitives also accept bitmasks: 'DIRECTIVE & MASK [[OP] VALUE]'.  For
instance, 'src tcp port & 10 == 8'.

The B<port>, B<icmp type>, and B<tcp opt> directives will only match first
fragments.

You can omit a lot of this syntax. For example, instead of 'ip proto tcp',
you can just say 'tcp'; and similarly for 'port www' (just say 'www'), 'tcp
opt syn' (just say 'syn'), 'net 10.0.0.0/24' (just say '10.0.0.0/24'), and
'ip unfrag' (just say 'unfrag'). You can often eliminate repetitive
qualifiers, too: 'src port 80 or 81' is the same as 'src port 80 or src
port 81'.

A pattern consisting entirely of "-", "any", or "all" matches every packet.

The patterns are scanned in order, and the packet is sent to the output
corresponding to the first matching pattern. Thus more specific patterns
should come before less specific ones. You will get a warning if no packet
will ever match a pattern. Usually, this is because an earlier pattern is
more general, or because your pattern is contradictory ('src port www and
src port ftp').

=n

Valid IP port names: 'echo', 'discard', 'daytime', 'chargen', 'ftp-data',
'ftp', 'ssh', 'telnet', 'smtp', 'domain', 'dns', 'bootps', 'bootpc',
'tftp', 'finger', 'www', 'pop3', 'sunrpc', 'auth', 'nntp', 'ntp',
'netbios-ns', 'netbios-dgm', 'netbios-ssn', 'snmp', 'snmp-trap', 'irc',
'imap3', 'https', 'rip', 'route', 'imaps', 'pop3s'

Valid IP protocol names: 'icmp', 'igmp', 'ipip', 'tcp', 'udp'

Valid TCP options: 'syn', 'fin', 'ack', 'rst', 'psh', 'urg'

Valid ICMP type names: 'echo-reply', 'unreachable', 'sourcequench',
'redirect', 'echo', 'routeradvert', 'routersolicit', 'timeexceeded',
'parameterproblem', 'timestamp', 'timestamp-reply', 'inforeq',
'inforeq-reply', 'maskreq', 'maskreq-reply'

This element correctly handles IP packets with options.

B<[tcp | udp] port>, B<icmp type>, B<tcp opt>, and B<tcp win> directives can
only be true on the first fragment of a fragmented packet.

Every IPClassifier element has an equivalent corresponding IPFilter element
and vice versa. Use the element whose syntax is more convenient for your
needs.

=e

For example,

  IPClassifier(10.0.0.0/24 and syn,
               10.0.0.0/24 and fin ack,
               10.0.0.0/24 and tcp,
               -);

creates an element with four outputs. The first three outputs are for TCP
packets from net 10.0.0.x. SYN packets are sent to output 0, FIN packets
with the ACK bit set to output 1, and all other TCP packets to output 2.
The last output is for all other IP packets, and non-TCP packets from net
10.0.0.x.

  IPClassifier(dst tcp port 8000 or 8080,
               dst tcp port > 1023,
               tcp);

creates an element with three outputs. The first output is for TCP packets
destined for port 8000 or 8080; the second output is for TCP packets
destined for any other user port (that is, port > 1023); and the third
output is for all other TCP packets. Non-TCP packets are dropped.

=h program read-only
Returns a human-readable definition of the program the IPClassifier element
is using to classify packets. At each step in the program, four bytes
of packet data are ANDed with a mask and compared against four bytes of
classifier pattern.

=a Classifier, IPFilter, CheckIPHeader, MarkIPHeader, CheckIPHeader2,
tcpdump(1) */

class IPClassifier : public IPFilter { public:

  IPClassifier() CLICK_COLD;
  ~IPClassifier() CLICK_COLD;

  const char *class_name() const		{ return "IPClassifier"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
