#ifndef CLICK_IPNAMEINFO_HH
#define CLICK_IPNAMEINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

IPNameInfo()

=s ip

stores name information about IP packets

=d

Contains IP-related name mappings, such as the names for common IP protocols.
This element should not be used in configurations.

IPNameInfo installs the following name mappings by default:

B<IP protocols>: dccp, icmp, igmp, ipip, payload, sctp, tcp, tcpudp, transp,
udp.

B<TCP/UDP ports>: auth, bootpc, bootps, chargen, daytime, discard, dns,
domain, echo, finger, ftp, ftp-data, https, imap3, imaps, irc, netbios-dgm,
netbios-ns, netbios-ssn, nntp, ntp, pop3, pop3s, rip, route, smtp, snmp,
snmp-trap, ssh, sunrpc, telnet, tftp, www.

B<ICMP types>: echo, echo-reply, inforeq, inforeq-reply, maskreq,
maskreq-reply, parameterproblem, redirect, routeradvert, routersolicit,
sourcequench, timeexceeded, timestamp, timestamp-reply, unreachable.

B<ICMP unreachable codes>: filterprohibited, host, hostprecedence,
hostprohibited, hostunknown, isolated, needfrag, net, netprohibited,
netunknown, port, precedencecutoff, protocol, srcroutefail, toshost, tosnet.

B<ICMP redirect codes>: host, net, toshost, tosnet.

B<ICMP timeexceeded codes>: reassembly, transit.

B<ICMP parameterproblem codes>: erroratptr, length, missingopt.

At user level, IPNameInfo additionally reads /etc/services and /etc/protocols
to populate the IP protocol and TCP/UDP port databases.
*/

class IPNameInfo : public Element { public:

    const char *class_name() const		{ return "IPNameInfo"; }

    static void static_initialize();
    static void static_cleanup();

};

CLICK_ENDDECLS
#endif
