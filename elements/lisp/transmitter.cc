/*
 * transmitter.{cc,hh} -- element counting decapsulated packets per VM (LISPVMCounter)
 * Franck Messaoudi
 *
 * Copyright (c) 2014-2026 Franck Messaoudi
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
#include "transmitter.hh"
#include "lispcommon.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>
#include <clicknet/ip.h>
#include <clicknet/ether.h>

CLICK_DECLS

LISPVMCounter::LISPVMCounter()
{
    _received_packet = _transmit_packet = _use_mac = 0;
}
LISPVMCounter::~LISPVMCounter()
{
}

int
LISPVMCounter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	    .read_p("adr1", _adr1)
	    .read_p("adr2", _adr2)
	    .read_p("adr3", _adr3)
	    .read_p("adr4", _adr4)
	    .read_all("VM", _vms) // any number of VM addresses
	    .read("USE_MAC",
		_use_mac) // count by destination MAC (Ethernet frames)
	    .complete()
	< 0)
	return -1;

    return 0;
}

int
LISPVMCounter::initialize(ErrorHandler *)
{
    EtherAddress mac = EtherAddress();

    IPAddress vms[4] = {_adr1, _adr2, _adr3, _adr4};
    for (int i = 0; i < 4; ++i)
	if (vms[i])
	    insert(mac, vms[i]);
    for (int i = 0; i < _vms.size(); ++i)
	insert(mac, _vms[i]);
    return 0;
}

void
LISPVMCounter::insert(const EtherAddress &mac, IPAddress adr)
{
    _lock.acquire_write();
    TransEntry nw_entry;
    nw_entry.eid = adr;
    nw_entry.eth = mac;

    _transtable.push_back(nw_entry);
    _lock.release_write();
}

void
LISPVMCounter::remove_vm(IPAddress vm_adr)
{
    _lock.acquire_write();

    TransTable::iterator end = _transtable.end();
    for (TransTable::iterator it = _transtable.begin(); it != end; ++it) {
	if (it->eid == vm_adr) {
	    _transtable.erase(it);
	    break;
	}
    }
    _lock.release_write();
    return;
}

void
LISPVMCounter::packet_handle(Packet *p)
{
    _lock.acquire_write();

    int indce = -1;
    ++_received_packet;

    if (_use_mac) {
	click_ether *ethh = (click_ether *)(p->data());
	EtherAddress mac_adr(ethh->ether_dhost);

	indce = ethlookup(mac_adr);

    } else {
	click_ip *iph = lisp_ip_header(p);
	IPAddress ip_adr = iph->ip_dst;
	indce = iplookup(ip_adr);
    }

    if (indce < 0) {
	_lock.release_write();
	return;
    }

    _transtable[indce].packet_count += 1;
    ++_transmit_packet;

    _lock.release_write();
    return;
}

void
LISPVMCounter::push(int port, Packet *p)
{
    if (!port)
	packet_handle(p);
    if (noutputs())
	output(0).push(p); // deliver towards the VMs
    else
	p->kill();
}

String
LISPVMCounter::read_handler(Element *e, void *user_data)
{
    LISPVMCounter *trans = (LISPVMCounter *)e;
    StringAccum sa;

    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_table:

	for (int i = 0; i < trans->_transtable.size(); i++) {
	    sa << trans->_transtable[i].eid << ' '
	       << trans->_transtable[i].packet_count << '\n';
	}
    }

    return sa.take_string();
}

void
LISPVMCounter::add_handlers()
{
    add_read_handler("table", read_handler, h_table);
    add_data_handlers("receiv packet", Handler::OP_READ, &_received_packet);
    add_data_handlers(
	"received", Handler::OP_READ, &_received_packet); // Click-style name
    add_data_handlers("transmit packet", Handler::OP_READ, &_transmit_packet);
    add_data_handlers(
	"counted", Handler::OP_READ, &_transmit_packet); // Click-style name
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPVMCounter)
ELEMENT_MT_SAFE(LISPVMCounter)
