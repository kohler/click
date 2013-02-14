/*
 * tcpipencap.{cc,hh} -- element encapsulates packet in TCP/IP header
 * Cliff Frey
 *
 * Copyright (c) 2010 Meraki, Inc.
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include "tcpfragmenter.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

TCPFragmenter::TCPFragmenter()
    : _mtu(0), _mtu_anno(-1)
{
    _fragments = 0;
    _fragmented_count = 0;
    _count = 0;
}

TCPFragmenter::~TCPFragmenter()
{
}

int
TCPFragmenter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint16_t mtu;
    int mtu_anno = -1;

    if (Args(conf, this, errh)
	.read("MTU", mtu)
	.read("MTU_ANNO", AnnoArg(2), mtu_anno)
	.complete() < 0)
	return -1;

    if (mtu == 0 && mtu_anno == -1)
	return errh->error("At least one of MTU and MTU_ANNO must be set");

    _mtu = mtu;
    _mtu_anno = mtu_anno;
    return 0;
}

void
TCPFragmenter::push(int, Packet *p)
{
    int mtu = _mtu;
    if (_mtu_anno >= 0 && p->anno_u16(_mtu_anno) &&
	(!mtu || mtu > p->anno_u16(_mtu_anno)))
	mtu = p->anno_u16(_mtu_anno);

    int32_t hlen;
    int32_t tcp_len;
    {
        const click_ip *ip = p->ip_header();
        const click_tcp *tcp = p->tcp_header();
        hlen = (ip->ip_hl<<2) + (tcp->th_off<<2);
        tcp_len = ntohs(ip->ip_len) - hlen;
    }

    int max_tcp_len = mtu - hlen;

    _count++;
    if (!mtu || max_tcp_len <= 0 || tcp_len < max_tcp_len) {
        output(0).push(p);
        return;
    }

    _fragmented_count++;
    for (int offset = 0; offset < tcp_len; offset += max_tcp_len) {
        Packet *p_clone;
        if (offset + max_tcp_len < tcp_len)
            p_clone = p->clone();
        else {
            p_clone = p;
            p = 0;
        }
        if (!p_clone)
            break;
        WritablePacket *q = p_clone->uniqueify();
        p_clone = 0;
        click_ip *ip = q->ip_header();
        click_tcp *tcp = q->tcp_header();
        uint8_t *tcp_data = ((uint8_t *)tcp) + (tcp->th_off<<2);
        int this_len = tcp_len - offset > max_tcp_len ? max_tcp_len : tcp_len - offset;
        if (offset != 0)
            memcpy(tcp_data, tcp_data + offset, this_len);
        q->take(tcp_len - this_len);
        ip->ip_len = htons(q->end_data() - q->network_header());
        ip->ip_sum = 0;
#if HAVE_FAST_CHECKSUM
        ip->ip_sum = ip_fast_csum((unsigned char *)ip, q->network_header_length() >> 2);
#else
        ip->ip_sum = click_in_cksum((unsigned char *)ip, q->network_header_length());
#endif

        if ((tcp->th_flags & TH_FIN) && offset + mtu < tcp_len)
            tcp->th_flags ^= TH_FIN;

        tcp->th_seq = htonl(ntohl(tcp->th_seq) + offset);
        tcp->th_sum = 0;

        // now calculate tcp header cksum
        int plen = q->end_data() - (uint8_t*)tcp;
        unsigned csum = click_in_cksum((unsigned char *)tcp, plen);
        tcp->th_sum = click_in_cksum_pseudohdr(csum, ip, plen);
        _fragments++;
        output(0).push(q);
    }
}

void
TCPFragmenter::add_handlers()
{
    add_data_handlers("fragments", Handler::OP_READ, &_fragments);
    add_data_handlers("fragmented_count", Handler::OP_READ, &_fragmented_count);
    add_data_handlers("count", Handler::OP_READ, &_count);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPFragmenter)
ELEMENT_MT_SAFE(TCPFragmenter)
