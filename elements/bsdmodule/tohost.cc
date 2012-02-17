// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * tohost.{cc,hh} -- element passes packets to kernel
 * Luigi Rizzo, Marko Zec
 *
 * Copyright (c) 2001-2004 International Computer Science Institute
 * Copyright (c) 2004 University of Zagreb
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
#include <click/args.hh>
CLICK_DECLS

// for watching when devices go offline
static AnyDeviceMap to_host_map;
#if 0 /* MARKO XXX not yet in BSD */
static struct notifier_block device_notifier;
extern "C" {
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, vo
id *v);
}
#endif

void
ToHost::static_initialize()
{
    to_host_map.initialize();
#if 0 /* MARKO XXX not yet ready */
    device_notifier.notifier_call = device_notifier_hook;
    device_notifier.priority = 1;
    device_notifier.next = 0;
    register_netdevice_notifier(&device_notifier);
#endif
}

void
ToHost::static_cleanup()
{
#if 0 /* MARKO XXX */
    unregister_netdevice_notifier(&device_notifier);
#endif
}

ToHost::ToHost()
    : _sniffers(false), _allow_nonexistent(false), _drops(0)
{
}

ToHost::~ToHost()
{
}

int
ToHost::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_p("DEVNAME", _devname)
	.read("SNIFFERS", _sniffers)
	.read("ALLOW_NONEXISTENT", _allow_nonexistent)
	.complete();
}

int
ToHost::initialize(ErrorHandler *errh)
{
    // We find the device here, rather than in 'initialize', to avoid warnings
    // about "device down" with FromHost devices -- FromHost brings up its
    // device during initialize().
    return (_devname ? find_device(_allow_nonexistent, &to_host_map, errh) : 0);
}

void
ToHost::cleanup(CleanupStage)
{
    clear_device(&to_host_map);
}

void
ToHost::push(int, Packet *p)
{
    struct ifnet *ifp = _dev;
    struct mbuf *m = p->steal_m();
    struct ether_header *eh = mtod(m, struct ether_header *);
    if (m == NULL) {
	click_chatter("ToHost: steal_m failed");
	return ;
    }

    if (!ifp)
	ifp = m->m_pkthdr.rcvif;

    // check that device exists
    if (!ifp) {
	if (++_drops == 1)
	    click_chatter("%p{element}: device not set and packet has rcvif=NULL, dropping", this);
	m_freem(m);
	return;
    }

    m->m_pkthdr.rcvif = &_dev_click; // tell click-ether-input to ignore this
#if 1
    /* XXX: ether_input() is now declared static. -bms */
    (*ifp->if_input)(ifp, m);
#else
    ether_input(ifp, eh, m);
#endif
}

String
ToHost::read_handler(Element *e, void *)
{
    ToHost *th = static_cast<ToHost *>(e);
    return String(th->_drops);
}

void
ToHost::add_handlers()
{
    add_read_handler("drops", read_handler, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(AnyDevice bsdmodule)
EXPORT_ELEMENT(ToHost)
