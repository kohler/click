/*
 * ipnameinfo.{cc,hh} -- element stores IP name-to-number mappings
 * Eddie Kohler
 *
 * Copyright (c) 2005 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "ipnameinfo.hh"
#include <click/nameinfo.hh>
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#if CLICK_USERLEVEL && HAVE_NETDB_H
# include <netdb.h>
#endif
CLICK_DECLS

static const StaticNameDB::Entry ip_protos[] = {
    { "icmp", IP_PROTO_ICMP },
    { "igmp", IP_PROTO_IGMP },
    { "ipip", IP_PROTO_IPIP },
    { "tcp", IP_PROTO_TCP },
    { "tcpudp", IP_PROTO_TCP_OR_UDP },
    { "transp", IP_PROTO_TRANSP },
    { "udp", IP_PROTO_UDP }
};

static const StaticNameDB::Entry icmp_types[] = {
    { "echo", ICMP_ECHO },
    { "echo-reply", ICMP_ECHOREPLY },
    { "inforeq", ICMP_IREQ },
    { "inforeq-reply", ICMP_IREQREPLY },
    { "maskreq", ICMP_MASKREQ },
    { "maskreq-reply", ICMP_MASKREQREPLY },
    { "parameterproblem", ICMP_PARAMPROB },
    { "redirect", ICMP_REDIRECT },
    { "routeradvert", ICMP_ROUTERADVERT },
    { "routersolicit", ICMP_ROUTERSOLICIT },
    { "sourcequench", ICMP_SOURCEQUENCH },
    { "timeexceeded", ICMP_TIMXCEED },
    { "timestamp", ICMP_TSTAMP },
    { "timestamp-reply", ICMP_TSTAMPREPLY },
    { "unreachable", ICMP_UNREACH },
};

static const StaticNameDB::Entry icmp_unreach_codes[] = {
    { "filterprohibited", ICMP_UNREACH_FILTER_PROHIB },
    { "host", ICMP_UNREACH_HOST },
    { "hostprecedence", ICMP_UNREACH_HOST_PRECEDENCE },
    { "hostprohibited", ICMP_UNREACH_HOST_PROHIB },
    { "hostunknown", ICMP_UNREACH_HOST_UNKNOWN },
    { "isolated", ICMP_UNREACH_ISOLATED },
    { "needfrag", ICMP_UNREACH_NEEDFRAG },
    { "net", ICMP_UNREACH_NET },
    { "netprohibited", ICMP_UNREACH_NET_PROHIB },
    { "netunknown", ICMP_UNREACH_NET_UNKNOWN },
    { "port", ICMP_UNREACH_PORT },
    { "precedencecutoff", ICMP_UNREACH_PRECEDENCE_CUTOFF },
    { "protocol", ICMP_UNREACH_PROTOCOL },
    { "srcroutefail", ICMP_UNREACH_SRCFAIL },
    { "toshost", ICMP_UNREACH_TOSHOST },
    { "tosnet", ICMP_UNREACH_TOSNET }
};

static const StaticNameDB::Entry icmp_redirect_codes[] = {
    { "host", ICMP_REDIRECT_HOST },
    { "net", ICMP_REDIRECT_NET },
    { "toshost", ICMP_REDIRECT_TOSHOST },
    { "tosnet", ICMP_REDIRECT_TOSNET }
};

static const StaticNameDB::Entry icmp_timxceed_codes[] = {
    { "reassembly", ICMP_TIMXCEED_REASSEMBLY },
    { "transit", ICMP_TIMXCEED_TRANSIT }
};

static const StaticNameDB::Entry icmp_paramprob_codes[] = {
    { "erroratptr", ICMP_PARAMPROB_ERRATPTR },
    { "length", ICMP_PARAMPROB_LENGTH },
    { "missingopt", ICMP_PARAMPROB_OPTABSENT }
};

static const StaticNameDB::Entry known_ports[] = {
    { "auth", 113 },
    { "bootpc", 68 },
    { "bootps", 67 },
    { "chargen", 19 },
    { "daytime", 13 },
    { "discard", 9 },
    { "dns", 53 },
    { "domain", 53 },
    { "echo", 7 },
    { "finger", 79 },
    { "ftp", 21 },
    { "ftp-data", 20 },
    { "https", 443 },
    { "imap3", 220 },
    { "imaps", 993 },
    { "irc", 194 },
    { "netbios-dgm", 138 },
    { "netbios-ns", 137 },
    { "netbios-ssn", 139 },
    { "nntp", 119 },
    { "ntp", 123 },
    { "pop3", 110 },
    { "pop3s", 995 },
    { "rip", 520 },
    { "route", 520 },
    { "smtp", 25 },
    { "snmp", 161 },
    { "snmp-trap", 162 },
    { "ssh", 22 },
    { "sunrpc", 111 },
    { "telnet", 23 },
    { "tftp", 69 },
    { "www", 80 }
};

namespace {

#if CLICK_USERLEVEL && HAVE_NETDB_H
class ServicesNameDB : public NameDB { public:
    ServicesNameDB(uint32_t type)	: NameDB(type, String(), 4) { }
    bool query(const String &name, void *value, int vsize);
};
bool
ServicesNameDB::query(const String &name, void *value, int vsize)
{
    assert(vsize == 4);
    if (const struct servent *srv = getservbyname(name.c_str(), (type() == IP_PROTO_TCP ? "tcp" : "udp"))) {
	*reinterpret_cast<uint32_t*>(value) = ntohs(srv->s_port);
	return true;
    } else
	return false;
}
#endif

}


static NameDB *dbs[10];

void
IPNameInfo::static_initialize()
{
    dbs[0] = new StaticNameDB(NameInfo::T_IP_PROTO, String(), ip_protos, sizeof(ip_protos) / sizeof(ip_protos[0]));
    dbs[1] = new StaticNameDB(NameInfo::T_ICMP_TYPE, String(), icmp_types, sizeof(icmp_types) / sizeof(icmp_types[0]));
    dbs[2] = new StaticNameDB(NameInfo::T_ICMP_CODE + ICMP_UNREACH, String(), icmp_unreach_codes, sizeof(icmp_unreach_codes) / sizeof(icmp_unreach_codes[0]));
    dbs[3] = new StaticNameDB(NameInfo::T_ICMP_CODE + ICMP_REDIRECT, String(), icmp_redirect_codes, sizeof(icmp_redirect_codes) / sizeof(icmp_redirect_codes[0]));
    dbs[4] = new StaticNameDB(NameInfo::T_ICMP_CODE + ICMP_TIMXCEED, String(), icmp_timxceed_codes, sizeof(icmp_timxceed_codes) / sizeof(icmp_timxceed_codes[0]));
    dbs[5] = new StaticNameDB(NameInfo::T_ICMP_CODE + ICMP_PARAMPROB, String(), icmp_paramprob_codes, sizeof(icmp_paramprob_codes) / sizeof(icmp_paramprob_codes[0]));
#if CLICK_USERLEVEL && HAVE_NETDB_H
    dbs[6] = new ServicesNameDB(NameInfo::T_TCP_PORT);
    dbs[7] = new ServicesNameDB(NameInfo::T_UDP_PORT);
#endif
    dbs[8] = new StaticNameDB(NameInfo::T_TCP_PORT, String(), known_ports, sizeof(known_ports) / sizeof(known_ports[0]));
    dbs[9] = new StaticNameDB(NameInfo::T_UDP_PORT, String(), known_ports, sizeof(known_ports) / sizeof(known_ports[0]));
    for (int i = 0; i < 10; i++)
	if (dbs[i])
	    NameInfo::installdb(dbs[i], 0);
}

void
IPNameInfo::static_cleanup()
{
    for (int i = 0; i < 10; i++)
	if (dbs[i]) {
	    NameInfo::removedb(dbs[i]);
	    delete dbs[i];
	    dbs[i] = 0;
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPNameInfo)
