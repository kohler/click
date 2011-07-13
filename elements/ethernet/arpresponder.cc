// -*- c-basic-offset: 4 -*-
/*
 * arpresponder.{cc,hh} -- element that responds to ARP queries
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "arpresponder.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

ARPResponder::ARPResponder()
{
}

ARPResponder::~ARPResponder()
{
}

int
ARPResponder::add(Vector<Entry> &v, const String &arg, ErrorHandler *errh) const
{
    int old_vsize = v.size();
    Vector<String> words;
    cp_spacevec(arg, words);

    Vector<Entry> entries;
    EtherAddress ena;
    bool have_ena = false;

    for (int i = 0; i < words.size(); ++i) {
	IPAddress addr, mask;
	if (IPPrefixArg(true).parse(words[i], addr, mask, this)) {
	    v.push_back(Entry());
	    v.back().dst = addr & mask;
	    v.back().mask = mask;
	} else if (EtherAddressArg().parse(words[i], ena, this)) {
	    if (have_ena) {
		v.resize(old_vsize);
		return errh->error("more than one ETH");
	    }
	    have_ena = true;
	} else {
	    v.resize(old_vsize);
	    return errh->error("expected IP/MASK ETH");
	}
    }

    // check for an argument that is both IP address and Ethernet address
    for (int i = 0; !have_ena && i < words.size(); ++i)
	if (EtherAddressArg().parse(words[i], ena, this))
	    have_ena = true;

    if (v.size() == old_vsize)
	return errh->error("missing IP/MASK");
    if (!have_ena) {
	v.resize(old_vsize);
	return errh->error("missing ETH");
    }
    for (int i = old_vsize; i < v.size(); ++i)
	v[i].ena = ena;
    return 0;
}

int
ARPResponder::entry_compare(const void *ap, const void *bp, void *user_data)
{
    int a = *reinterpret_cast<const int *>(ap),
	b = *reinterpret_cast<const int *>(bp);
    const Entry *entries = reinterpret_cast<Entry *>(user_data);
    const Entry &ea = entries[a], &eb = entries[b];

    if (ea.dst == eb.dst && ea.mask == eb.mask)
	return b - a;		// keep later match
    else if ((ea.dst & eb.mask) == eb.dst)
	return -1;
    else if ((eb.dst & ea.mask) == ea.dst)
	return 1;
    else
	return a - b;
}

void
ARPResponder::normalize(Vector<Entry> &v, bool warn, ErrorHandler *errh)
{
    Vector<int> permute;
    for (int i = 0; i < v.size(); ++i)
	permute.push_back(i);
    click_qsort(permute.begin(), permute.size(), sizeof(int), entry_compare, v.begin());

    Vector<Entry> nv;
    for (int i = 0; i < permute.size(); ++i) {
	const Entry &e = v[permute[i]];
	if (nv.empty() || nv.back().dst != e.dst || nv.back().mask != e.mask)
	    nv.push_back(e);
	else if (warn)
	    errh->warning("multiple entries for %s", e.dst.unparse_with_mask(e.mask).c_str());
    }
    nv.swap(v);
}

int
ARPResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Vector<Entry> v;
    for (int i = 0; i < conf.size(); i++) {
	PrefixErrorHandler perrh(errh, "argument " + String(i) + ": ");
	add(v, conf[i], &perrh);
    }
    if (!errh->nerrors()) {
	normalize(v, true, errh);
	_v.swap(v);
	return 0;
    } else
	return -1;
}

Packet *
ARPResponder::make_response(const uint8_t target_eth[6], /* them */
                            const uint8_t target_ip[4],
                            const uint8_t src_eth[6], /* me */
                            const uint8_t src_ip[4],
			    const Packet *p /* only used for annotations */)
{
    WritablePacket *q = Packet::make(sizeof(click_ether) + sizeof(click_ether_arp));
    if (q == 0) {
	click_chatter("in arp responder: cannot make packet!");
	return 0;
    }

    // in case of FromLinux, set the device annotation: want to make it seem
    // that ARP response came from the device that the query arrived on
    if (p) {
	q->set_device_anno(p->device_anno());
	SET_VLAN_TCI_ANNO(q, VLAN_TCI_ANNO(p));
    }

    click_ether *e = (click_ether *) q->data();
    q->set_ether_header(e);
    memcpy(e->ether_dhost, target_eth, 6);
    memcpy(e->ether_shost, src_eth, 6);
    e->ether_type = htons(ETHERTYPE_ARP);

    click_ether_arp *ea = (click_ether_arp *) (e + 1);
    ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
    ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
    ea->ea_hdr.ar_hln = 6;
    ea->ea_hdr.ar_pln = 4;
    ea->ea_hdr.ar_op = htons(ARPOP_REPLY);
    memcpy(ea->arp_sha, src_eth, 6);
    memcpy(ea->arp_spa, src_ip, 4);
    memcpy(ea->arp_tha, target_eth, 6);
    memcpy(ea->arp_tpa, target_ip, 4);

    return q;
}

Packet *
ARPResponder::simple_action(Packet *p)
{
    const click_ether *e = (const click_ether *) p->data();
    const click_ether_arp *ea = (const click_ether_arp *) (e + 1);
    Packet *q = 0;
    if (p->length() >= sizeof(*e) + sizeof(click_ether_arp)
	&& e->ether_type == htons(ETHERTYPE_ARP)
	&& ea->ea_hdr.ar_hrd == htons(ARPHRD_ETHER)
	&& ea->ea_hdr.ar_pro == htons(ETHERTYPE_IP)
	&& ea->ea_hdr.ar_op == htons(ARPOP_REQUEST)) {
	IPAddress ipa((const unsigned char *) ea->arp_tpa);
	if (const EtherAddress *ena = lookup(ipa))
	    q = make_response(ea->arp_sha, ea->arp_spa, ena->data(), ea->arp_tpa, p);
    }
    if (q)
	p->kill();
    else
	checked_output_push(1, p);
    return q;
}

String
ARPResponder::read_handler(Element *e, void *)
{
    ARPResponder *ar = static_cast<ARPResponder *>(e);
    StringAccum sa;
    for (int i = 0; i < ar->_v.size(); i++)
	sa << ar->_v[i].dst.unparse_with_mask(ar->_v[i].mask) << ' ' << ar->_v[i].ena << '\n';
    return sa.take_string();
}

int
ARPResponder::lookup_handler(int, String &str, Element *e, const Handler *, ErrorHandler *errh)
{
    ARPResponder *ar = static_cast<ARPResponder *>(e);
    IPAddress a;
    if (IPAddressArg().parse(str, a, ar)) {
	if (const EtherAddress *ena = ar->lookup(a))
	    str = ena->unparse();
	else
	    str = String();
	return 0;
    } else
	return errh->error("expected IP address");
}

int
ARPResponder::add_handler(const String &s, Element *e, void *, ErrorHandler *errh)
{
    ARPResponder *ar = static_cast<ARPResponder *>(e);
    Vector<Entry> v(ar->_v);
    if (ar->add(v, s, errh) >= 0) {
	normalize(v, false, 0);
	ar->_v.swap(v);
	return 0;
    } else
	return -1;
}

int
ARPResponder::remove_handler(const String &s, Element *e, void *, ErrorHandler *errh)
{
    ARPResponder *ar = static_cast<ARPResponder *>(e);
    IPAddress addr, mask;
    if (!IPPrefixArg(true).parse(s, addr, mask, ar))
	return errh->error("expected IP/MASK");
    addr &= mask;
    for (Vector<Entry>::iterator it = ar->_v.begin(); it != ar->_v.end(); ++it)
	if (it->dst == addr && it->mask == mask) {
	    ar->_v.erase(it);
	    return 0;
	}
    return errh->error("%s not found", addr.unparse_with_mask(mask).c_str());
}


void
ARPResponder::add_handlers()
{
    add_read_handler("table", read_handler, 0);
    set_handler("lookup", Handler::OP_READ | Handler::READ_PARAM, lookup_handler);
    add_write_handler("add", add_handler, 0);
    add_write_handler("remove", remove_handler, 0);
}

EXPORT_ELEMENT(ARPResponder)
CLICK_ENDDECLS
