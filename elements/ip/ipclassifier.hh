#ifndef IPCLASSIFIER_HH
#define IPCLASSIFIER_HH
#include "elements/ip/ipfilter.hh"

/*
 * =c
 * IPClassifier(PATTERN_1, ..., PATTERN_N)
 * =s
 * classifies IP packets by contents
 * V<classification>
 * =d
 *
 * Classifies IP packets according to tcpdump(1)-like patterns. The
 * IPClassifier has N outputs, each associated with the corresponding pattern
 * from the configuration string. The input packets must have their IP header
 * annotation set; CheckIPHeader and MarkIPHeader do this.
 *
 * Patterns are built from I<pattern primitives>. The primitives
 * IPClassifier understands are:
 *
 * =over 8
 * =item B<ip proto PROTO>
 *
 * PROTO is an IP protocol name (see below) or a valid IP protocol number.
 * Matches packets of the given protocol.
 *
 * =item B<[SRCORDST] host IPADDR>
 *
 * IPADDR is an IP address and SRCORDST * is `src', `dst', `src or dst', or
 * `src and dst'. (If SRCORDST is missing, * `src or dst' is assumed.) Matches
 * packets sent to and/or from the given * machine.
 *
 * =item B<[SRCORDST] net NETADDR>
 *
 * NETADDR is an IP network address (either CIDR-style `IPADDR/BITS' or
 * `IPADDR mask MASK') and SRCORDST is as above. Matches packets sent to
 * and/or from the given network.
 *
 * =item B<[SRCORDST] [tcp | udp] port PORT>
 *
 * PORT is a TCP or UDP port name (see below) or number and SRCORDST is as
 * above. Matches packets sent to and/or from the given TCP or UDP port. If
 * you leave out `tcp' or `udp', then either TCP or UDP is accepted.
 *
 * =item B<tcp opt TCPOPT>
 *
 * TCPOPT is a TCP option name (see below). Matches TCP packets with the given
 * option.
 *
 * =item B<ip tos TOS>
 *
 * TOS is a value between 0 and 255. Matches IP packets with the given TOS
 * value.
 *
 * =item B<ip dscp DSCP>
 *
 * DSCP is a value between 0 and 63. Matches IP packets with the given DSCP
 * value (the upper 6 bits of TOS).
 *
 * =item B<ip frag>
 *
 * Matches fragmented IP packets (that is, packets with the more-fragments bit
 * set and/or a nonzero fragment offset).
 *
 * =item B<ip unfrag>
 *
 * Equivalent to `not ip frag'.
 *
 * =item B<icmp type TYPE>
 *
 * TYPE is a value between 0 and 255. Matches ICMP packets with the given ICMP
 * type.
 *
 * =item B<true>
 *
 * Matches every packet.
 *
 * =item B<false>
 *
 * Matches no packets.
 *
 * =back
 *
 * These primitives can be combined with the connectives `and', `or', and
 * `not' (synonyms `&&', `||', and `!'), and with parentheses. For example,
 * `(dst port www or dst port ssh) and tcp opt syn'.
 *
 * All primitives except B<tcp opt> accept an optional OPERATION, `==' or
 * `!=', which can occur before the actual option. `src host == 10.0.0.10'
 * matches packets whose source host is 10.0.0.10; `src host != 10.0.0.10'
 * matches packets whose source host I<is not> 10.0.0.10. The B<ip
 * proto>, B<port>, B<ip tos>, B<ip dscp>, and B<icmp type>
 * directives also support `<', `>', `<=', and `>=' operations for limited
 * integer values. (Specifically, you can say `< POW', `> POW-1', `<= POW-1',
 * or `>= POW' if POW is a power of 2.) If no OPERATION is specified, `==' is
 * assumed.
 *
 * For B<port> and B<icmp type> directives, `DIRECTIVE != VALUE' is not the
 * same as `not (DIRECTIVE == VALUE)'. For example, `src tcp port != 5'
 * matches TCP packets whose source port is not 5, while `!(src tcp port ==
 * 5)' matches non-TCP packets as well. (The `src tcp port != 5' option is
 * effectively equivalent to `tcp and not src tcp port 5'.) Similarly, `icmp
 * type != 4' will not match non-ICMP packets. The same goes for the `<', `>',
 * `<=', and `>=' relations, and the B<tcp opt> directives.
 *
 * You can omit a lot of this syntax. For example, instead of `ip proto tcp',
 * you can just say `tcp'; and similarly for `port www' (just say `www'), `tcp
 * opt syn' (just say `syn'), `net 10.0.0.0/24' (just say `10.0.0.0/24'), and
 * `ip unfrag' (just say `unfrag'). You can often eliminate repetitive
 * qualifiers, too: `src port 80 or 81' is the same as `src port 80 or src
 * port 81'.
 *
 * As a special case, a pattern consisting of "-", "any", or "all" matches
 * every packet.
 *
 * The patterns are scanned in order, and the packet is sent to the output
 * corresponding to the first matching pattern. Thus more specific patterns
 * should come before less specific ones. You will get a warning if no packet
 * will ever match a pattern. Usually, this is because an earlier pattern is
 * more general, or because your pattern is contradictory (`src port www and
 * src port ftp').
 *
 * =n
 *
 * Valid IP port names: `echo', `discard', `daytime', `chargen', `ftp-data',
 * `ftp', `ssh', `telnet', `smtp', `domain', `dns', `finger', `www', `auth',
 * `https'
 *
 * Valid IP protocol names: `icmp', `igmp', `ipip', `tcp', `udp'
 *
 * Valid TCP options: `syn', `fin', `ack', `rst', `psh', `urg'
 *
 * This element correctly handles IP packets with options.
 *
 * B<[tcp | udp] port>, B<icmp type>, and B<tcp opt> directives can only be
 * true on the first fragment of a fragmented packet.
 *
 * Every IPClassifier element has an equivalent corresponding IPFilter element
 * and vice versa. Use the element whose syntax is more convenient for your
 * needs.
 *
 *
 * =e
 * For example,
 *
 *   IPClassifier(10.0.0.0/24 and syn,
 *                10.0.0.0/24 and fin ack,
 *                10.0.0.0/24 and tcp,
 *                -);
 *
 * creates an element with four outputs. The first three outputs are for TCP
 * packets from net 10.0.0.x. SYN packets are sent to output 0, FIN packets
 * with the ACK bit set to output 1, and all other TCP packets to output 2.
 * The last output is for all other IP packets, and non-TCP packets from net
 * 10.0.0.x.
 *
 *   IPClassifier(dst tcp port 8000 or 8080,
 *                dst tcp port > 1023,
 *                tcp);
 *
 * creates an element with three outputs. The first output is for TCP packets
 * destined for port 8000 or 8080; the second output is for TCP packets
 * destined for any other user port (that is, port > 1023); and the third
 * output is for all other TCP packets. Non-TCP packets are dropped.
 *
 * =h program read-only
 * Returns a human-readable definition of the program the IPClassifier element
 * is using to classify packets. At each step in the program, four bytes
 * of packet data are ANDed with a mask and compared against four bytes of
 * classifier pattern.
 *
 * =a Classifier, IPFilter, CheckIPHeader, MarkIPHeader, CheckIPHeader2,
 * tcpdump(1) */

class IPClassifier : public IPFilter {

 public:
  
  IPClassifier();
  ~IPClassifier();
  
  const char *class_name() const		{ return "IPClassifier"; }
  const char *processing() const		{ return PUSH; }
  
  IPClassifier *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
};

#endif
