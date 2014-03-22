/*
 * arpquerier.{cc,hh} -- ARP resolver element
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2005 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
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
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

ARPQuerier::ARPQuerier()
    : _arpt(0), _my_arpt(false), _zero_warned(false)
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
    uint32_t capacity, entry_capacity, entry_packet_capacity, capacity_slim_factor;
    Timestamp timeout, poll_timeout(60);
    bool have_capacity, have_entry_capacity, have_entry_packet_capacity, have_capacity_slim_factor, have_timeout, have_broadcast,
	broadcast_poll = false;
    _arpt = 0;
    if (Args(this, errh).bind(conf)
	.read("CAPACITY", capacity).read_status(have_capacity)
	.read("ENTRY_CAPACITY", entry_capacity).read_status(have_entry_capacity)
	.read("ENTRY_PACKET_CAPACITY", entry_packet_capacity).read_status(have_entry_packet_capacity)
	.read("CAPACITY_SLIM_FACTOR", capacity_slim_factor).read_status(have_capacity_slim_factor)
	.read("TIMEOUT", timeout).read_status(have_timeout)
	.read("BROADCAST", _my_bcast_ip).read_status(have_broadcast)
	.read("TABLE", ElementCastArg("ARPTable"), _arpt)
	.read("POLL_TIMEOUT", poll_timeout)
	.read("BROADCAST_POLL", broadcast_poll)
	.consume() < 0)
	return -1;

    if (!_arpt) {
	Vector<String> subconf;
	if (have_capacity)
	    subconf.push_back("CAPACITY " + String(capacity));
	if (have_entry_capacity)
	    subconf.push_back("ENTRY_CAPACITY " + String(entry_capacity));
	if (have_entry_packet_capacity)
	    subconf.push_back("ENTRY_PACKET_CAPACITY " + String(entry_packet_capacity));
	if (have_capacity_slim_factor)
	    subconf.push_back("CAPACITY_SLIM_FACTOR " + String(capacity_slim_factor));
	if (have_timeout)
	    subconf.push_back("TIMEOUT " + timeout.unparse());
	_arpt = new ARPTable;
	_arpt->attach_router(router(), -1);
	_arpt->configure(subconf, errh);
	_my_arpt = true;
    }

    IPAddress my_mask;
    if (conf.size() == 1)
	conf.push_back(conf[0]);
    if (Args(conf, this, errh)
	.read_mp("IP", IPPrefixArg(true), _my_ip, my_mask)
	.read_mp("ETH", _my_en)
	.complete() < 0)
	return -1;

    if (!have_broadcast) {
	_my_bcast_ip = _my_ip | ~my_mask;
	if (_my_bcast_ip == _my_ip)
	    _my_bcast_ip = 0xFFFFFFFFU;
    }

    _broadcast_poll = broadcast_poll;
    if ((uint32_t) poll_timeout.sec() >= (uint32_t) 0xFFFFFFFFU / CLICK_HZ)
	_poll_timeout_j = 0;
    else
	_poll_timeout_j = poll_timeout.jiffies();

    return 0;
}

int
ARPQuerier::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t capacity, entry_capacity, entry_packet_capacity, capacity_slim_factor;
    Timestamp timeout, poll_timeout(Timestamp::make_jiffies((click_jiffies_t) _poll_timeout_j));
    bool have_capacity, have_entry_capacity, have_entry_packet_capacity, have_capacity_slim_factor, have_timeout, have_broadcast,
	broadcast_poll(_broadcast_poll);
    IPAddress my_bcast_ip;

    if (Args(this, errh).bind(conf)
	.read("CAPACITY", capacity).read_status(have_capacity)
	.read("ENTRY_CAPACITY", entry_capacity).read_status(have_entry_capacity)
	.read("ENTRY_PACKET_CAPACITY", entry_packet_capacity).read_status(have_entry_packet_capacity)
	.read("CAPACITY_SLIM_FACTOR", capacity_slim_factor).read_status(have_capacity_slim_factor)
	.read("TIMEOUT", timeout).read_status(have_timeout)
	.read("BROADCAST", my_bcast_ip).read_status(have_broadcast)
	.read_with("TABLE", AnyArg())
	.read("POLL_TIMEOUT", poll_timeout)
	.read("BROADCAST_POLL", broadcast_poll)
	.consume() < 0)
	return -1;

    IPAddress my_ip, my_mask;
    EtherAddress my_en;
    if (conf.size() == 1)
	conf.push_back(conf[0]);
    if (Args(conf, this, errh)
	.read_mp("IP", IPPrefixArg(true), my_ip, my_mask)
	.read_mp("ETH", my_en)
	.complete() < 0)
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
    if (_my_arpt && have_entry_packet_capacity)
	_arpt->set_entry_packet_capacity(entry_packet_capacity);
    if (_my_arpt && have_capacity_slim_factor)
	_arpt->set_capacity_slim_factor(capacity_slim_factor);
    if (_my_arpt && have_timeout)
	_arpt->set_timeout(timeout);

    _broadcast_poll = broadcast_poll;
    if ((uint32_t) poll_timeout.sec() >= (uint32_t) 0xFFFFFFFFU / CLICK_HZ)
	_poll_timeout_j = 0;
    else
	_poll_timeout_j = poll_timeout.jiffies();

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
ARPQuerier::take_state(Element *e, ErrorHandler *errh)
{
    ARPQuerier *arpq = (ARPQuerier *) e->cast("ARPQuerier");
    if (!arpq || _my_ip != arpq->_my_ip || _my_en != arpq->_my_en
	|| _my_bcast_ip != arpq->_my_bcast_ip)
	return;

    if (_my_arpt && arpq->_my_arpt)
	_arpt->take_state(arpq->_arpt, errh);
    _arp_queries = arpq->_arp_queries;
    _drops = arpq->_drops;
    _arp_responses = arpq->_arp_responses;
}

void
ARPQuerier::send_query_for(const Packet *p, bool ether_dhost_valid)
{
    // Uses p's IP and Ethernet headers.

    static_assert(Packet::default_headroom >= sizeof(click_ether), "Packet::default_headroom must be at least 14.");
    WritablePacket *q = Packet::make(Packet::default_headroom - sizeof(click_ether),
				     NULL, sizeof(click_ether) + sizeof(click_ether_arp), 0);
    if (!q) {
	click_chatter("in arp querier: cannot make packet!");
	return;
    }

    click_ether *e = (click_ether *) q->data();
    q->set_ether_header(e);
    if (ether_dhost_valid && likely(!_broadcast_poll))
	memcpy(e->ether_dhost, p->ether_header()->ether_dhost, 6);
    else
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
    SET_VLAN_TCI_ANNO(q, VLAN_TCI_ANNO(p));

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
    EtherAddress *dst_eth = reinterpret_cast<EtherAddress *>(q->ether_header()->ether_dhost);
    int r;

    // Easy case: requires only read lock
  retry_read_lock:
    r = _arpt->lookup(dst_ip, dst_eth, _poll_timeout_j);
    if (r >= 0) {
	assert(!dst_eth->is_broadcast());
	if (r > 0)
	    send_query_for(q, true);
	// ... and send packet below.
    } else if (dst_ip.addr() == 0xFFFFFFFFU || dst_ip == _my_bcast_ip) {
	memset(dst_eth, 0xff, 6);
	// ... and send packet below.
    } else if (dst_ip.is_multicast()) {
	uint8_t *dst_addr = q->ether_header()->ether_dhost;
	dst_addr[0] = 0x01;
	dst_addr[1] = 0x00;
	dst_addr[2] = 0x5E;
	uint32_t addr = ntohl(dst_ip.addr());
	dst_addr[3] = (addr >> 16) & 0x7F;
	dst_addr[4] = addr >> 8;
	dst_addr[5] = addr;
	// ... and send packet below.
    } else {
	// Zero or unknown address: do not send the packet.
	if (!dst_ip) {
	    if (!_zero_warned) {
		click_chatter("%s: would query for 0.0.0.0; missing dest IP addr annotation?", declaration().c_str());
		_zero_warned = true;
	    }
	    ++_drops;
	    q->kill();
	} else {
	    r = _arpt->append_query(dst_ip, q);
	    if (r == -EAGAIN)
		goto retry_read_lock;
	    if (r < 0)
		q->kill();
	    if (r > 0)
		send_query_for(q, false); // q is on the ARP entry's queue
	    // if r >= 0, do not q->kill() since it is stored in some ARP entry.
	}
	return;
    }

    // It's time to emit the packet with our Ethernet address as source.  (Set
    // the source address immediately before send in case the user changes the
    // source address while packets are enqueued.)
    memcpy(&q->ether_header()->ether_shost, _my_en.data(), 6);
    output(0).push(q);
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
    case h_count:
	return String(q->_arpt->count());
    case h_length:
	return String(q->_arpt->length());
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
    add_read_handler("count", read_handler, h_count);
    add_read_handler("length", read_handler, h_length);
    add_data_handlers("queries", Handler::OP_READ, &_arp_queries);
    add_data_handlers("responses", Handler::OP_READ, &_arp_responses);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_data_handlers("broadcast", Handler::OP_READ | Handler::OP_WRITE, &_my_bcast_ip);
    add_data_handlers("ipaddr", Handler::OP_READ | Handler::OP_WRITE, &_my_ip);
    add_write_handler("insert", write_handler, h_insert);
    add_write_handler("delete", write_handler, h_delete);
    add_write_handler("clear", write_handler, h_clear);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ARPQuerier)
ELEMENT_REQUIRES(ARPTable)
ELEMENT_MT_SAFE(ARPQuerier)
