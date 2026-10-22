/*
 * lispencap.{cc,hh} -- element encapsulates IP packets in LISP/UDP/IP (RFC 9300)
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
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include <clicknet/lisp.h>
#include "lispencap.hh"
#include "lispcommon.hh"
#include <clicknet/ip.h>
#include <clicknet/udp.h>

CLICK_DECLS

/*********************************************************************************************/

LISPEncap::LISPEncap()
{
    _use_nonce = 1;
    _solicit_rqst = _loc_sb = _use_ins_id = _set_udp_sum = 0;
    _ins_id = 0;
    _packet_count = 0;
}

/*********************************************************************************************/

LISPEncap::~LISPEncap()
{
}

/*********************************************************************************************/

int
LISPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Args args(conf, this, errh);
    _hdr.read_args(args);            // PROTO, SRC, DST and the IP keywords
    args.read("UseNce", _use_nonce); // historical keyword names
    args.read("SNRqst", _solicit_rqst);
    args.read("useLSB", _loc_sb);
    args.read("useIID", _use_ins_id);
    args.read("SetUDPSum", _set_udp_sum);
    args.read("NONCE", _use_nonce); // Click-style names
    args.read("ECHO_NONCE", _solicit_rqst);
    args.read("LSB", _loc_sb);
    args.read("IID", _ins_id);
    args.read("UDP_CHECKSUM", _set_udp_sum);
    if (args.complete() < 0 || _hdr.finish_configure(this, errh) < 0)
	return -1;
    if (_ins_id > 0xFFFFFF)
	return errh->error("IID must fit in 24 bits");
    if (_ins_id)
	_use_ins_id = true;

    return 0;
}

int
LISPEncap::initialize(ErrorHandler *)
{
    return 0;
}

/*********************************************************************************************/

// Source port for an encapsulated packet: 0xC000-0xFFFF, from the inner flow.
static uint16_t
flow_entropy_port(const click_ip *in, uint32_t len)
{
    return 0xC000 | (lisp_flow_hash(in, len) & 0x3FFF);
}

/*********************************************************************************************/

void
LISPEncap::encapsulation(Packet *p_in)
{
    ++_packet_count;
    WritablePacket *p
	= p_in->push(sizeof(click_lisp) + sizeof(click_udp) + sizeof(click_ip));
    if (!p)
	return;

    click_ip *oh_iph = reinterpret_cast<click_ip *>(p->data());
    click_udp *udph = reinterpret_cast<click_udp *>(oh_iph + 1);
    click_lisp *lisph = reinterpret_cast<click_lisp *>(udph + 1);

    //** OUTER IP AND UDP HEADERS
    // RFC 9300: destination port 4341; the source port carries a hash of
    // the inner flow so that ECMP routers can spread LISP traffic.  The
    // UDP checksum stays 0 (allowed for IPv4) unless UDP_CHECKSUM is set.
    uint16_t sport
	= flow_entropy_port(reinterpret_cast<const click_ip *>(lisph + 1),
	    p->length()
		- (sizeof(click_ip) + sizeof(click_udp) + sizeof(click_lisp)));
    _hdr.fill(p, IPAddress(), htons(sport), htons(LISP_DATA_PORT));

    //** SET THE LISP-HEADER (built in host order, stored in network order)
    // This site has a single RLOC (locator 0), which is up.
    const uint32_t lsb = 0x1;
    uint32_t w = 0, iid_lsb = 0;
    if (_use_nonce) {
	w |= LISP_F_N | (click_random() & LISP_NONCE_MASK);
	if (_solicit_rqst)
	    w |= LISP_F_E; // echo-nonce request, only valid with N
    } else {
	w |= LISP_F_V; // source and destination map-versions = 0
    }

    if (_use_ins_id) {
	w |= LISP_F_I;
	iid_lsb = (_ins_id & LISP_NONCE_MASK) << LISP_IID_SHIFT;
	if (_loc_sb) {
	    w |= LISP_F_L;
	    iid_lsb |= lsb & LISP_LSB8_MASK;
	}
    } else if (_loc_sb) {
	w |= LISP_F_L;
	iid_lsb = lsb;
    }
    lisph->lisp_flags_nonce = htonl(w);
    lisph->lisp_iid_lsb = htonl(iid_lsb);

    // optional UDP checksum: computed last, over the outer header
    if (_set_udp_sum)
	LISPIPHeader::finish_udp(p);

    output(0).push(p);
}

/*********************************************************************************************/

void
LISPEncap::push(int port, Packet *p)
{
    if (!port)
	encapsulation(p);
}

/*********************************************************************************************/

void
LISPEncap::add_handlers()
{
    add_data_handlers("packets count", Handler::OP_READ, &_packet_count);
    add_data_handlers(
	"count", Handler::OP_READ, &_packet_count); // Click-style name
}

/*********************************************************************************************/

CLICK_ENDDECLS

EXPORT_ELEMENT(LISPEncap)
ELEMENT_REQUIRES(LISPCommon)
ELEMENT_MT_SAFE(LISPEncap)
