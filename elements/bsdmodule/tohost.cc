// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * tohost.{cc,hh} -- element passes packets to kernel
 * Luigi Rizzo
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include <click/glue.hh>
#include "tohost.hh"
#include <click/error.hh>
#include <click/confparse.hh>

ToHost::ToHost()
{
    MOD_INC_USE_COUNT;
    add_input();
}

ToHost::~ToHost()
{
    MOD_DEC_USE_COUNT;
}

int
ToHost::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_parse(conf, this, errh,
		    cpString, "interface name", &_devname,
		    cpEnd) < 0 )
	return -1;
    if (find_device(false, errh) < 0)
        return -1;
    return 0;
}

void
ToHost::push(int, Packet *p)
{
    struct mbuf *m = p->steal_m();
    struct ether_header *eh = mtod(m, struct ether_header *);
    if (m == NULL) {
	click_chatter("ToHost: steal_m failed");
	return ;
    }
    m->m_pkthdr.rcvif = NULL; // tell click-ether-input to ignore this
    m_adj(m, ETHER_HDR_LEN);
    ether_input(device(), eh, m) ;
}

ELEMENT_REQUIRES(AnyDevice bsdmodule)
EXPORT_ELEMENT(ToHost)
