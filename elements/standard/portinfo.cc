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
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#if HAVE_NETDB_H
# include <netdb.h>
#endif
CLICK_DECLS

PortInfo::PortInfo()
    : _map(0)
{
    MOD_INC_USE_COUNT;
}

PortInfo::~PortInfo()
{
    MOD_DEC_USE_COUNT;
}

int
PortInfo::add_info(const Vector<String> &conf, const String &prefix,
		   ErrorHandler *errh)
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
	const char *slash = cp_unsigned(port_str.begin(), port_str.end(), 0, &port);
	if (slash == port_str.begin() || slash == port_str.end() || *slash != '/' || port > 0xFFFF)
	    errh->error("expected 'NAME PORT/PROTO', got '%s'", conf[i].c_str());
	else {
	    int protoinfo;
	    if (slash + 4 == port_str.end() && memcmp(slash, "/udp", 4) == 0)
		protoinfo = INFO_UDP;
	    else if (slash + 4 == port_str.end() && memcmp(slash, "/tcp", 4) == 0)
		protoinfo = INFO_TCP;
	    else
		continue;

	    int &v = _map.find_force(prefix + name_str);
	    if (!(v & protoinfo) && (!v || (v & 0xFFFFU) == port))
		v = (v & 0xFFFF0000U) | protoinfo | port;

	    for (name_str = cp_pop_spacevec(str);
		 name_str && name_str[0] != '#';
		 name_str = cp_pop_spacevec(str)) {
		int &v = _map.find_force(prefix + name_str);
		if (!(v & protoinfo) && (!v || (v & 0xFFFFU) == port))
		    v = (v & 0xFFFF0000U) | protoinfo | port;
	    }
	}
    }
    
    return (errh->nerrors() == before ? 0 : -1);
}

int
PortInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    // find prefix, which includes slash
    String prefix;
    int last_slash = id().find_right('/');
    if (last_slash >= 0)
	prefix = id().substring(0, last_slash + 1);
    else
	prefix = String();

    // put everything in the first PortInfo
    const Vector<Element *> &ev = router()->elements();
    for (int i = 0; i <= eindex(); i++)
	if (PortInfo *si = (PortInfo *)ev[i]->cast("PortInfo")) {
	    router()->set_attachment("PortInfo", si);
	    return si->add_info(conf, prefix, errh);
	}

    // should never get here
    return -1;
}

int
PortInfo::query(const String &name, int have_mask, const String &eid) const
{
    String prefix = eid;
    int slash = prefix.find_right('/');
    prefix = prefix.substring(0, (slash < 0 ? 0 : slash + 1));

    while (1) {
	int e = _map[prefix + name];
	if (e & have_mask)
	    return e;
	else if (!prefix)
	    return 0;

	slash = prefix.find_right('/', prefix.length() - 2);
	prefix = prefix.substring(0, (slash < 0 ? 0 : slash + 1));
    }
}

PortInfo *
PortInfo::find_element(Element *e)
{
    if (!e)
	return 0;
    else
	return static_cast<PortInfo *>(e->router()->attachment("PortInfo"));
}

bool
PortInfo::query(const String &s, int ip_p, uint16_t &store, Element *e)
{
    if (ip_p != IP_PROTO_TCP && ip_p != IP_PROTO_UDP)
	return false;
    
    if (PortInfo *infoe = find_element(e))
	if (int p = infoe->query(s, (ip_p == IP_PROTO_TCP ? INFO_TCP : INFO_UDP), e->id())) {
	    store = p & 0xFFFF;
	    return true;
	}

#if HAVE_NETDB_H
    if (struct servent *srv = getservbyname(s.c_str(), (ip_p == IP_PROTO_TCP ? "tcp" : "udp"))) {
	store = ntohs(srv->s_port);
	return true;
    }
#endif
    
    return false;
}

EXPORT_ELEMENT(PortInfo)
ELEMENT_HEADER(<click/standard/portinfo.hh>)

// template instance
#include <click/vector.cc>
CLICK_ENDDECLS
