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
#include <click/confparse.hh>
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#if CLICK_USERLEVEL && HAVE_NETDB_H
# include <netdb.h>
# include <click/userutils.hh>
# ifndef _PATH_PROTOCOLS
#  define _PATH_PROTOCOLS "/etc/protocols"
# endif
# ifndef _PATH_SERVICES
#  define _PATH_SERVICES "/etc/services"
# endif
#endif
CLICK_DECLS

static const StaticNameDB::Entry ip_protos[] = {
    { "dccp", IP_PROTO_DCCP },
    { "gre", IP_PROTO_GRE },
    { "icmp", IP_PROTO_ICMP },
    { "igmp", IP_PROTO_IGMP },
    { "ipip", IP_PROTO_IPIP },
    { "payload", IP_PROTO_PAYLOAD },
    { "sctp", IP_PROTO_SCTP },
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
    { "pptp", 1723 },
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
    ServicesNameDB(uint32_t type, ServicesNameDB *other);
    ~ServicesNameDB();
    bool query(const String &name, void *value, size_t vsize);
  private:
    DynamicNameDB *_db;
    bool _read_db;
    ServicesNameDB *_next;
    ServicesNameDB *_prev;
    void read_services();
};

ServicesNameDB::ServicesNameDB(uint32_t type, ServicesNameDB *other)
    : NameDB(type, String(), 4), _db(0), _read_db(false)
{
    if (other) {
	_next = other;
	_prev = other->_prev;
	other->_prev = _prev->_next = this;
    } else
	_next = _prev = this;
}

ServicesNameDB::~ServicesNameDB()
{
    _next->_prev = _prev;
    _prev->_next = _next;
    delete _db;
}

void
ServicesNameDB::read_services()
{
#if 0
    // On my Linux laptop, using the 'getprotoent()/getservent()' interface
    // takes approximately 0.1s more than the hand-coded version below
    // (which I coded before realizing 'getprotoent()/getservent()' exists).

    if (type() == NameInfo::T_IP_PROTO) {
	if (!_db)
	    _db = new DynamicNameDB(type(), String(), 4);
	while (struct protoent *p = getprotoent()) {
	    uint32_t proto = p->p_proto;
	    _db->define(p->p_name, &proto, 4);
	    for (const char **a = (const char **) p->p_aliases; a && *a; a++)
		_db->define(*a, &proto, 4);
	}
    } else
	while (struct servent *s = getservent()) {
	    uint32_t port = ntohs(s->s_port), ptype;
	    if (!NameInfo::query_int(NameInfo::T_IP_PROTO, 0, s->s_proto, &ptype))
		continue;
	    ptype += NameInfo::T_IP_PORT;
	    ServicesNameDB *db;
	    for (db = _next; db->type() != ptype && db != this; db = db->_next)
		/* nada */;
	    if (db->type() != ptype)
		continue;
	    if (!db->_db)
		db->_db = new DynamicNameDB(type(), String(), 4);
	    db->_db->define(s->s_name, &port, 4);
	    for (const char **a = (const char **) s->s_aliases; a && *a; a++)
		db->_db->define(*a, &port, 4);
	}
#else
    // Hand-coded version.

    bool proto = (type() == NameInfo::T_IP_PROTO);
    String text = file_string(proto ? _PATH_PROTOCOLS : _PATH_SERVICES);
    if (!text)
	return;

    Vector<String> names;
    const char *s = text.begin(), *end = text.end();
    while (s < end) {
	const char *eol = s;
	while (eol < end && *eol != '#' && *eol != '\r' && *eol != '\n')
	    eol++;

	// first word: main name
	const char *bn, *en;
	for (bn = s; bn < eol && isspace((unsigned char) *bn); bn++)
	    /* nada */;
	for (en = bn; en < eol && !isspace((unsigned char) *en); en++)
	    /* nada */;

	// second word: protocol type
	const char *bt, *et;
	uint32_t pnum = 0;
	uint32_t ptype;
	int names_pos;
	ServicesNameDB *db;
	for (bt = en; bt < eol && isspace((unsigned char) *bt); bt++)
	    /* nada */;
	for (et = bt; et < eol && isdigit((unsigned char) *et) && pnum < 65536; et++)
	    pnum = 10*pnum + *et - '0';
	if (et == bt || pnum >= (proto ? 256 : 65536))
	    goto skip_to_eol;
	if (proto)
	    ptype = NameInfo::T_IP_PROTO;
	else {
	    if (et >= eol || (*et != '/' && *et != ','))
		goto skip_to_eol;
	    for (bt = et = et + 1; et < eol && !isspace((unsigned char) *et); et++)
		/* nada */;
	    if (!NameInfo::query_int(NameInfo::T_IP_PROTO, 0, text.substring(bt, et), &ptype))
		goto skip_to_eol;
	    ptype += NameInfo::T_IP_PORT;
	}

	// find the database
	for (db = _next; db->type() != ptype && db != this; db = db->_next)
	    /* nada */;
	if (db->type() != ptype)
	    goto skip_to_eol;

	// a series of assignments
	if (!db->_db)
	    db->_db = new DynamicNameDB(ptype, "", 4);

	// collect names.  Names often equal the names from the previous line,
	// so don't double-allocate strings for that case.
	names_pos = 0;
	do {
	    if (names_pos < names.size()
		&& !names[names_pos].equals(bn, en - bn))
		names.erase(names.begin() + names_pos, names.end());
	    // Don't use text.substring since that preserves a lot of garbage
	    // from the file's comments and such.
	    if (names_pos >= names.size())
		names.push_back(String(bn, en));
	    ++names_pos;

	    // move to next name
	    for (bn = et; bn < eol && isspace((unsigned char) *bn); bn++)
		/* nada */;
	    for (en = bn; en < eol && !isspace((unsigned char) *en); en++)
		/* nada */;
	    et = en;
	} while (bn != en);

	// actually make definitions
	for (String *n = names.begin(); n != names.begin() + names_pos; ++n)
	    db->_db->define(*n, &pnum, 4);

    skip_to_eol:
	s = eol;
	while (s < end && *s != '\r' && *s != '\n')
	    s++;
	while (s < end && (*s == '\r' || *s == '\n'))
	    s++;
    }
#endif

    // mark all relevant databases as read
    ServicesNameDB *db = this;
    do {
	db->_read_db = true;
	db = db->_next;
    } while (db != this);
}

bool
ServicesNameDB::query(const String &name, void *value, size_t vsize)
{
    assert(vsize == 4);

    // Check common case: integer
    int32_t crap;
    if (IntArg().parse(name, crap))
	return false;

    if (!_read_db)
	read_services();

    if (type() == NameInfo::T_IP_PROTO) {
	if (!_db) {
	    if (const struct protoent *proto = getprotobyname(name.c_str())) {
		*reinterpret_cast<uint32_t*>(value) = proto->p_proto;
		return true;
	    }
	} else if (_db->query(name, value, vsize))
	    return true;
    }

    if (type() >= NameInfo::T_IP_PORT && type() < NameInfo::T_IP_PORT + 256) {
	if (!_db) {
	    int proto = type() - NameInfo::T_IP_PORT;
	    const char *proto_name;
	    if (proto == IP_PROTO_TCP)
		proto_name = "tcp";
	    else if (proto == IP_PROTO_UDP)
		proto_name = "udp";
	    else if (const struct protoent *pe = getprotobynumber(proto))
		proto_name = pe->p_name;
	    else
		return false;
	    if (const struct servent *srv = getservbyname(name.c_str(), proto_name)) {
		*reinterpret_cast<uint32_t*>(value) = ntohs(srv->s_port);
		return true;
	    }
	} else if (_db->query(name, value, vsize))
	    return true;
    }

    return false;
}
#endif

}


static NameDB *dbs[13];

void
IPNameInfo::static_initialize()
{
#if CLICK_USERLEVEL && HAVE_NETDB_H
    dbs[0] = new ServicesNameDB(NameInfo::T_IP_PROTO, 0);
#endif
    dbs[1] = new StaticNameDB(NameInfo::T_IP_PROTO, String(), ip_protos, sizeof(ip_protos) / sizeof(ip_protos[0]));
    dbs[2] = new StaticNameDB(NameInfo::T_ICMP_TYPE, String(), icmp_types, sizeof(icmp_types) / sizeof(icmp_types[0]));
    dbs[3] = new StaticNameDB(NameInfo::T_ICMP_CODE + ICMP_UNREACH, String(), icmp_unreach_codes, sizeof(icmp_unreach_codes) / sizeof(icmp_unreach_codes[0]));
    dbs[4] = new StaticNameDB(NameInfo::T_ICMP_CODE + ICMP_REDIRECT, String(), icmp_redirect_codes, sizeof(icmp_redirect_codes) / sizeof(icmp_redirect_codes[0]));
    dbs[5] = new StaticNameDB(NameInfo::T_ICMP_CODE + ICMP_TIMXCEED, String(), icmp_timxceed_codes, sizeof(icmp_timxceed_codes) / sizeof(icmp_timxceed_codes[0]));
    dbs[6] = new StaticNameDB(NameInfo::T_ICMP_CODE + ICMP_PARAMPROB, String(), icmp_paramprob_codes, sizeof(icmp_paramprob_codes) / sizeof(icmp_paramprob_codes[0]));
#if CLICK_USERLEVEL && HAVE_NETDB_H
    ServicesNameDB *portdb;
    dbs[7] = portdb = new ServicesNameDB(NameInfo::T_TCP_PORT, 0);
    dbs[8] = new ServicesNameDB(NameInfo::T_UDP_PORT, portdb);
    dbs[9] = new ServicesNameDB(NameInfo::T_IP_PORT + IP_PROTO_SCTP, portdb);
    dbs[10] = new ServicesNameDB(NameInfo::T_IP_PORT + IP_PROTO_DCCP, portdb);
#endif
    dbs[11] = new StaticNameDB(NameInfo::T_TCP_PORT, String(), known_ports, sizeof(known_ports) / sizeof(known_ports[0]));
    dbs[12] = new StaticNameDB(NameInfo::T_UDP_PORT, String(), known_ports, sizeof(known_ports) / sizeof(known_ports[0]));
    for (int i = 0; i < 13; i++)
	if (dbs[i])
	    NameInfo::installdb(dbs[i], 0);
}

void
IPNameInfo::static_cleanup()
{
    for (int i = 0; i < 13; i++)
	if (dbs[i]) {
	    delete dbs[i];
	    dbs[i] = 0;
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPNameInfo)
