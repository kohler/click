/*
 * ip6encap.{cc,hh} -- element encapsulates packet in IP6 header
 * Roman Chertov
 *
 * Copyright (c) 2008 Santa Barbara Labs, LLC
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
#include "ip6encap.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

IP6Encap::IP6Encap()
{
    _use_dst_anno = false;
}

IP6Encap::~IP6Encap()
{
}

int
IP6Encap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    IP6Address src;
    String     dst_str;
    uint32_t   flow = 0;
    int        proto;
    uint8_t    hlim = 250;
    uint8_t    ip_class = 0;

    memset(&_iph6, 0, sizeof(click_ip6));

    if (Args(conf, this, errh)
	.read_mp("PROTO", NamedIntArg(NameInfo::T_IP_PROTO), proto)
	.read_mp("SRC", src)
	.read_mp("DST", AnyArg(), dst_str)
	.read("HLIM", hlim)
	.read("CLASS", ip_class)
	.complete() < 0)
        return -1;

    if (proto < 0 || proto > 255)
        return errh->error("bad IP protocol");

    _use_dst_anno = dst_str == "DST_ANNO";
    if (_use_dst_anno)
        memset(&_iph6.ip6_dst.s6_addr, 0, sizeof(_iph6.ip6_dst.s6_addr));
    else if (!cp_ip6_address(dst_str, _iph6.ip6_dst.s6_addr, this))
        return errh->error("DST argument should be IP address or 'DST_ANNO'");
    // set up IP6 header
    _iph6.ip6_flow = htonl((6 << IP6_V_SHIFT) | (ip_class << IP6_CLASS_SHIFT) | flow);
    _iph6.ip6_plen = 0;
    _iph6.ip6_nxt = proto;
    _iph6.ip6_hlim = hlim;
    _iph6.ip6_src = src;
    return 0;
}


Packet *
IP6Encap::simple_action(Packet *p_in)
{
    WritablePacket *p = p_in->push(sizeof(click_ip6));
    if (!p)
        return 0;
    const IP6Address &a = DST_IP6_ANNO(p);

    click_ip6 *ip6 = reinterpret_cast<click_ip6 *>(p->data());

    memcpy(ip6, &_iph6, sizeof(click_ip6));
    if (_use_dst_anno && a)  // use_dst_anno
        ip6->ip6_dst = a;
    else
        SET_DST_IP6_ANNO(p, ip6->ip6_dst);

    ip6->ip6_plen = htons(p->length() - sizeof(click_ip6));
    p->set_ip6_header(ip6, sizeof(click_ip6));

    return p;
}

String
IP6Encap::read_handler(Element *e, void *thunk)
{
    IP6Encap *ip6e = static_cast<IP6Encap *>(e);

    switch ((intptr_t)thunk) {
        case 0:
            return IP6Address(ip6e->_iph6.ip6_src).unparse();
        case 1:
            if (ip6e->_use_dst_anno)
                return "DST_ANNO";
            else
                return IP6Address(ip6e->_iph6.ip6_dst).unparse();
        default:
            return "<error>";
    }
}

void
IP6Encap::add_handlers()
{
    add_read_handler("src", read_handler, 0, Handler::CALM);
    add_write_handler("src", reconfigure_keyword_handler, "1 SRC");
    add_read_handler("dst", read_handler, 1, Handler::CALM);
    add_write_handler("dst", reconfigure_keyword_handler, "2 DST");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IP6Encap)
ELEMENT_MT_SAFE(IP6Encap)
