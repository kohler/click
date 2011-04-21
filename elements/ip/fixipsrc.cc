/*
 * fixipsrc.{cc,hh} -- element sets IP source if Fix IP Source annotation on
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
#include "fixipsrc.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <click/packet_anno.hh>
CLICK_DECLS

FixIPSrc::FixIPSrc()
{
}

FixIPSrc::~FixIPSrc()
{
}

int
FixIPSrc::configure(Vector<String> &conf, ErrorHandler *errh)
{
    IPAddress a;
    if (Args(conf, this, errh).read_mp("IPADDR", a).complete() < 0)
	return -1;
    _my_ip = a.in_addr();
    return 0;
}

WritablePacket *
FixIPSrc::fix_it(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  SET_FIX_IP_SRC_ANNO(p, 0);
#if 0
  click_chatter("FixIPSrc changed %x to %x",
                ip->ip_src.s_addr,
                _my_ip.s_addr);
#endif
  ip->ip_src = _my_ip;
  int hlen = ip->ip_hl << 2;
  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum((unsigned char *)ip, hlen);
  return p;
}

Packet *
FixIPSrc::simple_action(Packet *p)
{
  if (FIX_IP_SRC_ANNO(p) && p->has_network_header())
    p = fix_it(p);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FixIPSrc)
ELEMENT_MT_SAFE(FixIPSrc)
