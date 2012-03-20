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
CLICK_DECLS


PortInfo::PortInfo()
{
}

int
PortInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    for (int i = 0; i < conf.size(); i++) {
	String str = conf[i];
	String name_str = cp_shift_spacevec(str);
	if (!name_str		// allow empty arguments
	    || name_str[0] == '#') // allow comments
	    continue;

	String port_str = cp_shift_spacevec(str);
	uint32_t port;
	int32_t proto = IP_PROTO_TCP_OR_UDP;
	const char *slash = cp_integer(port_str.begin(), port_str.end(), 0, &port);
	if (slash != port_str.end() && *slash == '/') {
	    if (slash + 4 == port_str.end() && memcmp(slash, "/tcp", 4) == 0)
		proto = IP_PROTO_TCP;
	    else if (slash + 4 == port_str.end() && memcmp(slash, "/udp", 4) == 0)
		proto = IP_PROTO_UDP;
	    else if (NameInfo::query_int(NameInfo::T_IP_PROTO, this, port_str.substring(slash + 1, port_str.end()), &proto))
		/* got proto */;
	    else
		continue;
	} else if (slash == port_str.begin() || slash != port_str.end()) {
	    errh->error("expected %<NAME PORT/PROTO%>");
	    continue;
	}

	do {
	    if (proto == IP_PROTO_TCP_OR_UDP) {
		NameInfo::define(NameInfo::T_TCP_PORT, this, name_str, &port, 4);
		NameInfo::define(NameInfo::T_UDP_PORT, this, name_str, &port, 4);
	    } else
		NameInfo::define(NameInfo::T_IP_PORT + proto, this, name_str, &port, 4);
	    name_str = cp_shift_spacevec(str);
	} while (name_str && name_str[0] != '#');
    }

    return errh->nerrors() ? -1 : 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PortInfo)
ELEMENT_HEADER(<click/standard/portinfo.hh>)
