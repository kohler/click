/*
 * SetTCPChecksum.{cc,hh} -- sets the TCP header checksum
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
#include "settcpchecksum.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
CLICK_DECLS

SetTCPChecksum::SetTCPChecksum()
  : _fixoff(false)
{
}

SetTCPChecksum::~SetTCPChecksum()
{
}

int
SetTCPChecksum::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_p("FIXOFF", _fixoff)
	.complete();
}

Packet *
SetTCPChecksum::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *iph = p->ip_header();
  click_tcp *tcph = p->tcp_header();
  unsigned plen = ntohs(iph->ip_len) - (iph->ip_hl << 2);
  unsigned csum;

  if (!p->has_transport_header() || plen < sizeof(click_tcp)
      || plen > (unsigned)p->transport_length())
    goto bad;

  if (_fixoff) {
    unsigned off = tcph->th_off << 2;
    if (off < sizeof(click_tcp))
      tcph->th_off = sizeof(click_tcp) >> 2;
    else if (off > plen && !IP_ISFRAG(iph))
      tcph->th_off = plen >> 2;
  }

  tcph->th_sum = 0;
  csum = click_in_cksum((unsigned char *)tcph, plen);
  tcph->th_sum = click_in_cksum_pseudohdr(csum, iph, plen);

  return p;

 bad:
  click_chatter("SetTCPChecksum: bad lengths");
  p->kill();
  return(0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTCPChecksum)
ELEMENT_MT_SAFE(SetTCPChecksum)
