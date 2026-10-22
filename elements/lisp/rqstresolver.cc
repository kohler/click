/*
 * rqstresolver.{cc,hh} -- ETR element answering Map-Requests (LISPMapReplier)
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
#include "rqstresolver.hh"
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include "lispcommon.hh"
CLICK_DECLS

/*************************************************************************************************************/

LISPMapReplier::LISPMapReplier()
{
    _rqst_received = _rply_sent = 0;
}

/*************************************************************************************************************/

LISPMapReplier::~LISPMapReplier()
{
}

/*************************************************************************************************************/

int
LISPMapReplier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Args args(conf, this, errh);
    args.read_mp("BASE", ElementCastArg("LISPEtrDatabase"), _base);
    _hdr.read_args(args); // PROTO, SRC, DST and the IP keywords
    if (args.complete() < 0 || _hdr.finish_configure(this, errh) < 0)
	return -1;

    return 0;
}

/*************************************************************************************************************/

int
LISPMapReplier::initialize(ErrorHandler *)
{
    return 0;
}

/*************************************************************************************************************/

/*************************************************************************************************************/

void
LISPMapReplier::handle_request(Packet *p)
{
    _rqst_received++;
    // incoming Map-Request (named in_* so they don't hide the member
    // _iph, the template for the reply's IP header)
    click_ip *in_iph = lisp_ip_header(p);
    click_udp *in_udph = lisp_udp_header(in_iph);
    click_lisp_map_request *m_rqsh = (click_lisp_map_request *)(in_udph + 1);
    //** CHECK THE UDP-HEADER VALIDITY
    if (in_udph->uh_dport != htons(LISP_CONTROL_PORT)) {
	click_chatter("%p{element}: not sent to port 4342, ignored", this);
	return;
    }

    //** CHECK THE LISP-HEADER VALIDITY
    IPAddress eid = m_rqsh->rec_eid_prefix;
    // reply to the ITR-RLOC carried in the request (the IP source is the
    // Map-Resolver when the request was forwarded)
    IPAddress itr_adr = in_iph->ip_src;
    if (m_rqsh->itr_rloc_afi == htons(LISP_AFI_IP))
	itr_adr = m_rqsh->itr_rloc;

    WritablePacket *q = (WritablePacket *)_base->reply_record(eid, itr_adr);
    if (!q)
	return;

    q = q->push(LISPIPHeader::headers + sizeof(click_lisp_map_reply));
    if (!q)
	return;
    memset(q->data(), 0,
	LISPIPHeader::headers
	    + sizeof(
		click_lisp_map_reply)); // headers pushed on uninitialized headroom
    click_lisp_map_reply *mrh = reinterpret_cast<click_lisp_map_reply *>(
	q->data() + LISPIPHeader::headers);

    // IP/UDP: to the ITR-RLOC, back to the request's source port
    _hdr.fill(q, itr_adr, htons(LISP_CONTROL_PORT), in_udph->uh_sport);

    //** SET THE LISP-HEADER
    mrh->type_flags = htonl(((uint32_t)LISP_MAP_REPLY << LISP_CTRL_TYPE_SHIFT)
	| 1); // P=E=S=0, one record
    mrh->nonce1 = m_rqsh->nonce1;
    mrh->nonce2 = m_rqsh->nonce2;

    LISPIPHeader::finish_udp(q); // UDP checksum last
    output(0).push(q);
    _rply_sent++;
}

/*********************************************************************************************/

void
LISPMapReplier::push(int port, Packet *p)
{
    if (!port)
	handle_request(p);
    p->kill();
}

/*************************************************************************************************************/

void
LISPMapReplier::add_handlers()
{
    add_data_handlers("received rqst", Handler::OP_READ, &_rqst_received);
    add_data_handlers(
	"requests", Handler::OP_READ, &_rqst_received); // Click-style name
    add_data_handlers("rply sent ", Handler::OP_READ, &_rply_sent);
    add_data_handlers(
	"replies", Handler::OP_READ, &_rply_sent); // Click-style name
}

/*************************************************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPMapReplier)
ELEMENT_REQUIRES(LISPEtrDatabase LISPCommon)
ELEMENT_MT_SAFE(LISPMapReplier)
