/*
 * arpquerier.{cc,hh} -- ARP resolver element
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2005 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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
#include "arpquerier.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

ARPQuerier::ARPQuerier()
    : _arpt(0), _my_arpt(false)
{
}

ARPQuerier::~ARPQuerier()
{
}

void *
ARPQuerier::cast(const char *name)
{
    if (strcmp(name, "ARPTable") == 0)
	return _arpt;
    else if (strcmp(name, "ARPQuerier") == 0)
	return this;
    else
	return Element::cast(name);
}

int
ARPQuerier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t capacity, entry_capacity;
    Timestamp timeout;
    bool have_capacity, have_entry_capacity, have_timeout, have_broadcast;
    Element *arpt = 0;
    if (cp_va_kparse_remove_keywords(conf, this, errh,
		"CAPACITY", cpkC, &have_capacity, cpUnsigned, &capacity,
		"ENTRY_CAPACITY", cpkC, &have_entry_capacity, cpUnsigned, &entry_capacity,
		"TIMEOUT", cpkC, &have_timeout, cpUnsigned, &timeout,
		"BROADCAST", cpkC, &have_broadcast, cpIPAddress, &_my_bcast_ip,
		"TABLE", 0, cpElement, &arpt,
		cpEnd) < 0)
	return -1;

    if (!arpt) {
	Vector<String> subconf;
	if (have_capacity)
	    subconf.push_back("CAPACITY " + String(capacity));
	if (have_entry_capacity)
	    subconf.push_back("ENTRY_CAPACITY " + String(entry_capacity));
	if (have_timeout)
	    subconf.push_back("TIMEOUT " + timeout.unparse());
	_arpt = new ARPTable;
	_arpt->attach_router(router(), -1);
	_arpt->configure(subconf, errh);
	_my_arpt = true;
    } else if (!(_arpt = static_cast<ARPTable *>(arpt->cast("ARPTable"))))
	return errh->error("bad TABLE");

    IPAddress my_mask;
    if (conf.size() == 1)
	conf.push_back(conf[0]);
    if (cp_va_kparse(conf, this, errh,
		     "IP", cpkP+cpkM, cpIPAddressOrPrefix, &_my_ip, &my_mask,
		     "ETH", cpkP+cpkM, cpEthernetAddress, &_my_en,
		     cpEnd) < 0)
	return -1;
    if (!have_broadcast) {
	_my_bcast_ip = _my_ip | ~my_mask;
	if (_my_bcast_ip == _my_ip)
	    _my_bcast_ip = 0xFFFFFFFFU;
    }
    return 0;
}

int
ARPQuerier::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t capacity, entry_capacity;
    Timestamp timeout;
    bool have_capacity, have_entry_capacity, have_timeout, have_broadcast;
    IPAddress my_bcast_ip;
    
    if (cp_va_kparse_remove_keywords(conf, this, errh,
		"CAPACITY", cpkC, &have_capacity, cpUnsigned, &capacity,
		"ENTRY_CAPACITY", cpkC, &have_entry_capacity, cpUnsigned, &entry_capacity,
		"TIMEOUT", cpkC, &have_timeout, cpUnsigned, &timeout,
		"BROADCAST", cpkC, &have_broadcast, cpIPAddress, &my_bcast_ip,
		"TABLE", 0, cpIgnore,
		cpEnd) < 0)
	return -1;

    IPAddress my_ip, my_mask;
    EtherAddress my_en;
    if (conf.size() == 1)
	conf.push_back(conf[0]);
    if (cp_va_kparse(conf, this, errh,
		     "IP", cpkP+cpkM, cpIPAddressOrPrefix, &my_ip, &my_mask,
		     "ETH", cpkP+cpkM, cpEthernetAddress, &my_en,
		     cpEnd) < 0)
	return -1;
    if (!have_broadcast) {
	my_bcast_ip = my_ip | ~my_mask;
	if (my_bcast_ip == my_ip)
	    my_bcast_ip = 0xFFFFFFFFU;
    }

    if ((my_ip != _my_ip || my_en != _my_en) && _my_arpt)
	_arpt->clear();

    _my_ip = my_ip;
    _my_en = my_en;
    _my_bcast_ip = my_bcast_ip;
    if (_my_arpt && have_capacity)
	_arpt->set_capacity(capacity);
    if (_my_arpt && have_entry_capacity)
	_arpt->set_entry_capacity(entry_capacity);
    if (_my_arpt && have_timeout)
	_arpt->set_timeout(timeout);
    return 0;
}

int
ARPQuerier::initialize(ErrorHandler *)
{
    _arp_queries = 0;
    _drops = 0;
    _arp_responses = 0;
    return 0;
}

void
ARPQuerier::cleanup(CleanupStage stage)
{
    if (_my_arpt) {
	_arpt->cleanup(stage);
	delete _arpt;
    }
}

void
ARPQuerier::take_state(Element *e, ErrorHandler *)
{
    ARPQuerier *arpq = (ARPQuerier *) e->cast("ARPQuerier");
    if (!arpq || _my_ip != arpq->_my_ip || _my_en != arpq->_my_en
	|| _my_bcast_ip != arpq->_my_bcast_ip || _my_arpt != arpq->_my_arpt)
	return;

    if (_my_arpt) {
	ARPTable *t = _arpt;
	_arpt = arpq->_arpt;
	arpq->_arpt = t;
	_arpt->set_capacity(t->capacity());
	_arpt->set_entry_capacity(t->entry_capacity());
	_arpt->set_timeout(t->timeout());
    }

    _arp_queries = arpq->_arp_queries;
    _drops = arpq->_drops;
    _arp_responses = arpq->_arp_responses;
}

void
ARPQuerier::send_query_for(Packet *p)
{
    WritablePacket *q = Packet::make(sizeof(click_ether) + sizeof(click_ether_arp));
    if (!q) {
	click_chatter("in arp querier: cannot make packet!");
	return;
    }
  
    click_ether *e = (click_ether *) q->data();
    q->set_ether_header(e);
    memset(e->ether_dhost, 0xff, 6);
    memcpy(e->ether_shost, _my_en.data(), 6);
    e->ether_type = htons(ETHERTYPE_ARP);

    click_ether_arp *ea = (click_ether_arp *) (e + 1);
    ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
    ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
    ea->ea_hdr.ar_hln = 6;
    ea->ea_hdr.ar_pln = 4;
    ea->ea_hdr.ar_op = htons(ARPOP_REQUEST);
    memcpy(ea->arp_sha, _my_en.data(), 6);
    memcpy(ea->arp_spa, _my_ip.data(), 4);
    memset(ea->arp_tha, 0, 6);
    IPAddress want_ip = p->dst_ip_anno();
    memcpy(ea->arp_tpa, want_ip.data(), 4);

    q->set_timestamp_anno(p->timestamp_anno());

    _arp_queries++;
    output(noutputs() - 1).push(q);
}

/*
 * If the packet's IP address is in the table, add an ethernet header
 * and push it out.
 * Otherwise push out a query packet.
 * May save the packet in the ARP table for later sending.
 * May call p->kill().
 */
void
ARPQuerier::handle_ip(Packet *p, bool response)
{
    // delete packet if we are not configured
    if (!_my_ip) {
	p->kill();
	++_drops;
	return;
    }

    // make room for Ethernet header
    WritablePacket *q;
    if (response) {
	assert(!p->shared());
	q = p->uniqueify();
    } else if (!(q = p->push_mac_header(sizeof(click_ether)))) {
	++_drops;
	return;
    } else
	q->ether_header()->ether_type = htons(ETHERTYPE_IP);

    IPAddress dst_ip = q->dst_ip_anno();
    EtherAddress *dst_eth = reinterpret_cast<EtherAddress *>(&q->ether_header()->ether_dhost);
    int r;

    // Easy case: requires only read lock
  retry_read_lock:
    r = _arpt->lookup(dst_ip, dst_eth, 60 * CLICK_HZ);
    if (r >= 0 && !dst_eth->is_broadcast()) {
	memcpy(&q->ether_header()->ether_shost, _my_en.data(), 6);
	output(0).push(q);
    } else if (dst_ip.addr() == 0xFFFFFFFFU || dst_ip == _my_bcast_ip) {
	// Check special IP addresses
	*dst_eth = EtherAddress::broadcast();
	memcpy(&q->ether_header()->ether_shost, _my_en.data(), 6);
	output(0).push(q);
	r = 0;
    } else if (!dst_ip) {
	static bool zero_warned = false;  
	if (!zero_warned) {
	    click_chatter("%s: would query for 0.0.0.0; missing dest IP addr annotation?", declaration().c_str());
	    zero_warned = true;
	}
	++_drops;
	q->kill();
	r = 0;
    } else {
	r = _arpt->append_query(dst_ip, q);
	if (r == -EAGAIN)
	    goto retry_read_lock;
    }

    if (r > 0)			// poll
	send_query_for(q);
}

/*
 * Got an ARP response.
 * Update our ARP table.
 * If there was a packet waiting to be sent, return it.
 */
void
ARPQuerier::handle_response(Packet *p)
{
    if (p->length() < sizeof(click_ether) + sizeof(click_ether_arp))
	return;

    ++_arp_responses;
  
    click_ether *ethh = (click_ether *) p->data();
    click_ether_arp *arph = (click_ether_arp *) (ethh + 1);
    IPAddress ipa = IPAddress(arph->arp_spa);
    EtherAddress ena = EtherAddress(arph->arp_sha);
    if (ntohs(ethh->ether_type) == ETHERTYPE_ARP
	&& ntohs(arph->ea_hdr.ar_hrd) == ARPHRD_ETHER
	&& ntohs(arph->ea_hdr.ar_pro) == ETHERTYPE_IP
	&& ntohs(arph->ea_hdr.ar_op) == ARPOP_REPLY
	&& !ena.is_group()) {
	Packet *cached_packet;
	_arpt->insert(ipa, ena, &cached_packet);

	// Send out packets in the order in which they arrived
	while (cached_packet) {
	    Packet *next = cached_packet->next();
	    handle_ip(cached_packet, true);
	    cached_packet = next;
	}
    }
}

void
ARPQuerier::push(int port, Packet *p)
{
    if (port == 0)
	handle_ip(p, false);
    else {
	handle_response(p);
	p->kill();
    }
}

String
ARPQuerier::read_handler(Element *e, void *thunk)
{
    ARPQuerier *q = (ARPQuerier *)e;
    switch (reinterpret_cast<uintptr_t>(thunk)) {
      case h_table:
	return q->_arpt->read_handler(q->_arpt, (void *) (uintptr_t) ARPTable::h_table);
      case h_stats:
	return
	    String(q->_drops.value() + q->_arpt->drops()) + " packets killed\n" +
	    String(q->_arp_queries.value()) + " ARP queries sent\n";
      default:
	return String();
    }
}

int
ARPQuerier::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    ARPQuerier *q = (ARPQuerier *) e;
    switch (reinterpret_cast<uintptr_t>(thunk)) {
      case h_insert:
	return q->_arpt->write_handler(str, q->_arpt, (void *) (uintptr_t) ARPTable::h_insert, errh);
      case h_delete:
	return q->_arpt->write_handler(str, q->_arpt, (void *) (uintptr_t) ARPTable::h_delete, errh);
      case h_clear:
	q->_arp_queries = q->_drops = q->_arp_responses = 0;
	q->_arpt->clear();
	return 0;
      default:
	return -1;
    }
}

void
ARPQuerier::add_handlers()
{
    add_read_handler("table", read_handler, h_table);
    add_read_handler("stats", read_handler, h_stats);
    add_data_handlers("queries", Handler::OP_READ, &_arp_queries);
    add_data_handlers("responses", Handler::OP_READ, &_arp_responses);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_data_handlers("broadcast", Handler::OP_READ, &_my_bcast_ip);
    add_data_handlers("ipaddr", Handler::OP_READ | Handler::OP_WRITE, &_my_ip);
    add_write_handler("insert", write_handler, h_insert);
    add_write_handler("delete", write_handler, h_delete);
    add_write_handler("clear", write_handler, h_clear);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ARPQuerier)
ELEMENT_MT_SAFE(ARPQuerier)
