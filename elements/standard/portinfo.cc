// -*- c-basic-offset: 4; related-file-name: "../../include/click/standard/portinfo.hh" -*-
/*
 * portinfo.{cc,hh} -- element stores TCP/UDP port information
 * Eddie Kohler
 *
 * Copyright (c) 2004 The Regents of the University of California
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
#include <click/standard/portinfo.hh>
#include <click/nameinfo.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#if HAVE_NETDB_H
# include <netdb.h>
#endif
CLICK_DECLS

namespace {

const StaticNameDB::Entry known_ports[] = {
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


PortInfo::PortInfo()
{
}

PortInfo::~PortInfo()
{
}

void
PortInfo::static_initialize()
{
#if CLICK_USERLEVEL && HAVE_NETDB_H
    NameInfo::installdb(new ServicesNameDB(NameInfo::T_TCP_PORT), 0);
    NameInfo::installdb(new ServicesNameDB(NameInfo::T_UDP_PORT), 0);
#endif
    NameDB *tcpdb = new StaticNameDB(NameInfo::T_TCP_PORT, String(), known_ports, sizeof(known_ports) / sizeof(known_ports[0]));
    NameInfo::installdb(tcpdb, 0);
    NameDB *udpdb = new StaticNameDB(NameInfo::T_UDP_PORT, String(), known_ports, sizeof(known_ports) / sizeof(known_ports[0]));
    NameInfo::installdb(udpdb, 0);
}

int
PortInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int before = errh->nerrors();
  
    for (int i = 0; i < conf.size(); i++) {
	String str = conf[i];
	String name_str = cp_pop_spacevec(str);
	if (!name_str		// allow empty arguments
	    || name_str[0] == '#') // allow comments
	    continue;

	String port_str = cp_pop_spacevec(str);
	uint32_t port;
	int proto = 0;
	const char *slash = cp_unsigned(port_str.begin(), port_str.end(), 0, &port);
	if (slash != port_str.end() && *slash == '/') {
	    if (slash + 4 == port_str.end() && memcmp(slash, "/tcp", 4) == 0)
		proto = IP_PROTO_TCP;
	    else if (slash + 4 == port_str.end() && memcmp(slash, "/udp", 4) == 0)
		proto = IP_PROTO_UDP;
	    else
		continue;
	} else if (slash == port_str.begin() || slash != port_str.end()) {
	    errh->error("expected 'NAME PORT/PROTO', got '%s'", conf[i].c_str());
	    continue;
	}

	do {
	    if (proto == 0 || proto == IP_PROTO_TCP)
		NameInfo::define(NameInfo::T_TCP_PORT, this, name_str, &port, 4);
	    if (proto == 0 || proto == IP_PROTO_UDP)
		NameInfo::define(NameInfo::T_UDP_PORT, this, name_str, &port, 4);
	    name_str = cp_pop_spacevec(str);
	} while (name_str && name_str[0] != '#');
    }
    
    return (errh->nerrors() == before ? 0 : -1);
}

EXPORT_ELEMENT(PortInfo)
ELEMENT_HEADER(<click/standard/portinfo.hh>)

// template instance
#include <click/vector.cc>
CLICK_ENDDECLS
