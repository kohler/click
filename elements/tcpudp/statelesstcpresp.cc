// -*- c-basic-offset: 4 -*-
/*
 * statelesstcpresp.{cc,hh} -- rewrites packet source and destination
 * Eddie Kohler
 *
 * Copyright (c) 2005 Regents of the University of California
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
#include "statelesstcpresp.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

int
StatelessTCPResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return 0;
}

Packet *
StatelessTCPResponder::handle_syn(Packet *p, const click_tcp *tcph)
{
    // read options, find MSS
    uint16_t mss = 576;
    const uint8_t *o = (const uint8_t *) (tcph + 1);
    const uint8_t *endo = (const uint8_t *) tcph + (tcph->th_off << 2);
    if (endo > p->end_data())
	endo = p->end_data();
    while (o < endo) {
	if (*o == TCPOPT_EOL)
	    break;
	else if (*o == TCPOPT_NOP)
	    /* nada */;
	else if (o[1] < 2 || o + o[1] > endo)
	    break;
	else if (*o == TCPOPT_MAXSEG && o[1] == TCPOLEN_MAXSEG) {
	    mss = (o[2] << 8) | o[3];
	    o += TCPOLEN_MAXSEG;
	} else
	    o += o[1];
    }

    // construct sequence number
    return (maxseg << 16) + (tcph->th_sport << 8);
}

Packet *
StatelessTCPResponder::simple_action(Packet *p)
{
    const click_ip *iph = p->ip_header();
    if (!p->has_network_header()
	|| iph->ip_p != IP_PROTO_TCP || !IP_FIRSTFRAG(iph)
	|| p->transport_length() < sizeof(click_tcp)) {
	checked_output_push(1, p);
	return 0;
    }

    const click_tcp *tcph = p->tcp_header();
    tcp_seq_t ackno;
    uint32_t ntcp_hl;
    if (tcph->th_flags & TH_RST) {
	_resets++;
	p->kill();
	return 0;
    } else if ((tcph->th_flags & (TH_SYN | TH_ACK)) == TH_SYN) {
	// handle SYN
	ackno = handle_syn(p, tcph);
	ntcp_hl = sizeof(click_tcp) + 4;
    } else if ((tcph->th_flags & (TH_SYN | TH_ACK)) != TH_ACK) {
      evil:
	_evil++;
	p->kill();
	return 0;
    } else {
	ackno = ntohl(tcph->th_ack);
	ntcp_hl = sizeof(click_tcp);
    }

    // unpack ackno
    tcp_seq_t ackno_base = ackno - (tcph->th_sport << 8);
    uint32_t mss = (ackno_base >> 16);
    uint32_t pos = (ackno_base & 0xFFFFU);
    if (mss < 20 || pos > _data.length() + 2)
	goto evil;
    if (mss > ntohs(tcph->th_win))
	mss = ntohs(tcph->th_win);
    if (tcph->th_flags & TH_SYN)
	mss = 0;
    if (pos + mss > _data.length() + 2)
	mss = _data.length() + 2 - pos;

    WritablePacket *q = Packet::make(sizeof(click_ip) + ntcp_hl + mss);
    q->set_network_header(q->data(), sizeof(click_ip));

    click_ip *nip = q->ip_header();
    nip->ip_v = 4;
    nip->ip_hl = sizeof(click_ip) >> 2;
    nip->ip_tos = 0;
    nip->ip_id = htons(_id);
    _id += 2;
    nip->ip_off = 0;
    nip->ip_ttl = 200;
    nip->ip_p = IP_PROTO_TCP;
    nip->ip_sum = 0;
    nip->ip_src = ip->ip_dst;
    nip->ip_dst = ip->ip_src;

    click_tcp *ntcp = q->tcp_header();
    ntcp->th_sport = tcp->th_dport;
    ntcp->th_dport = tcp->th_sport;
    ntcp->th_seq = ntohl(ackno);
    ntcp->th_ack = tcp->th_seq;
    ntcp->th_flags2 = 0;
    ntcp->th_off = (ntcp_hl >> 2);
    ntcp->th_flags = (tcph->th_flags & TH_SYN ? TH_SYN | TH_ACK : TH_ACK);
    if (pos < _data.length() + 2 && pos + mss == _data.length() + 2)
	ntcp->th_flags |= TH_FIN;
    ntcp->th_win = 0xFFFF;
    ntcp->th_sum = 0;
    ntcp->th_urp = 0;

    if (tcph->th_flags & TH_SYN) {
	uint8_t *o = (uint8_t *) (ntcp + 1);
	o[0] = TCPOPT_MAXSEG;
	o[1] = 4;
	o[2] = (1460 / 256);
	o[3] = (1460 % 256);
    }

    if (pos + mss == _data.length() + 2)
	mss--;
    if (mss > 0)
	memcpy((uint8_t *) ntcp + ntcp_hl, _data.data() + pos - 1, mss);

    // checksums galore
    uint32_t len = sizeof(click_ip) + ntcp_hl + mss;
    nip->ip_len = htons(len);
    nip->ip_sum = click_in_cksum(nip, nip->ip_hl << 2);
    uint32_t sum1 = click_in_cksum(ntcp, ntcp_hl + mss);
    ntcp->th_sum = click_in_cksum_pseudohdr(sum1, nip, len);

    p->kill();
    return q;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(StatelessTCPResponder)
