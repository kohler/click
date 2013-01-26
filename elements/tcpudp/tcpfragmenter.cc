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
{
}

TCPFragmenter::~TCPFragmenter()
{
}

int
TCPFragmenter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint16_t mtu;
    if (Args(conf, this, errh)
	.read("MTU", mtu)
	.complete() < 0)
	return -1;

    if (mtu == 0)
        return errh->error("MTU cannot be 0");

    _mtu = mtu;

    return 0;
}

void
TCPFragmenter::push(int, Packet *p)
{
    int32_t hlen;
    int32_t tcp_len;
    {
        const click_ip *ip = p->ip_header();
        const click_tcp *tcp = p->tcp_header();
        hlen = (ip->ip_hl<<2) + (tcp->th_off<<2);
        tcp_len = ntohs(ip->ip_len) - hlen;
    }

    int max_tcp_len = _mtu - hlen;

    if (!_mtu || max_tcp_len <= 0 || tcp_len < max_tcp_len) {
        output(0).push(p);
        return;
    }

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

        tcp->th_seq = htonl(ntohl(tcp->th_seq) + offset);
        tcp->th_sum = 0;

        // now calculate tcp header cksum
        int plen = q->end_data() - (uint8_t*)tcp;
        unsigned csum = click_in_cksum((unsigned char *)tcp, plen);
        tcp->th_sum = click_in_cksum_pseudohdr(csum, ip, plen);
        output(0).push(q);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TCPFragmenter)
ELEMENT_MT_SAFE(TCPFragmenter)
