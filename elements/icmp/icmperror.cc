/*
 * icmperror.{cc,hh} -- element constructs ICMP error packets
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
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
#include <clicknet/icmp.h>
#include <clicknet/ip.h>
#include "icmperror.hh"
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/nameinfo.hh>
CLICK_DECLS

ICMPError::ICMPError()
  : _type(-1), _code(-1)
{
}

ICMPError::~ICMPError()
{
}

bool
ICMPError::is_error_type(int type)
{
    return type == ICMP_UNREACH || type == ICMP_SOURCEQUENCH
	|| type == ICMP_REDIRECT || type == ICMP_TIMXCEED
	|| type == ICMP_PARAMPROB;
}

int
ICMPError::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String code_str = "0";
    unsigned mtu = 576, pmtu = 0;
    IPAddress src_ip;
    int type, code;
    Vector<IPAddress> bad_addrs;
    bool use_fix_anno = true;

    if (Args(conf, this, errh)
	.read_mp("SRC", src_ip)
	.read_mp("TYPE", NamedIntArg(NameInfo::T_ICMP_TYPE), type)
	.read_p("CODE", WordArg(), code_str)
	.read_p("BADADDRS", bad_addrs)
	.read("MTU", mtu)
	.read("PMTU", pmtu)
	.read("SET_FIX_ANNO", use_fix_anno)
	.complete() < 0)
	return -1;

    if (type < 0 || type > 255)
	return errh->error("ICMP type must be between 0 and 255");
    else if (!is_error_type(type))
	return errh->error("ICMP type %d is not an error type", type);
    if (!NameInfo::query_int(NameInfo::T_ICMP_CODE + type, this, code_str, &code)
	|| code < 0 || code > 255)
	return errh->error("argument 2 takes ICMP code (integer between 0 and 255)");

    _src_ip = src_ip;
    _type = type;
    _code = code;
    _bad_addrs.swap(bad_addrs);
    _mtu = mtu;
    _pmtu = pmtu;
    _use_fix_anno = use_fix_anno;
    return 0;
}

/*
 * Is an IP address unicast?
 */
bool
ICMPError::unicast(struct in_addr aa) const
{
  uint32_t a = aa.s_addr;
  uint32_t ha = ntohl(a);

  /* limited broadcast */
  if(ha == 0xffffffff)
    return(0);

  /* Class D multicast */
  if((ha & 0xf0000000u) == 0xe0000000u)
    return(0);

  /* limited broadcast */
  if (find(_bad_addrs.begin(), _bad_addrs.end(), IPAddress(a)) < _bad_addrs.end())
    return 0;

  return(1);
}

/*
 * Is a source IP address valid as defined in RFC1812 5.3.7
 * or 4.2.2.11 or 4.2.3.1?
 */
bool
ICMPError::valid_source(struct in_addr aa) const
{
  unsigned int a = aa.s_addr;
  unsigned int ha = ntohl(a);
  unsigned net = (ha >> 24) & 0xff;

  /* broadcast, multicast, or (local) directed broadcast */
  if(unicast(aa) == 0)
    return(0);

  /* local net or host: */
  if(net == 0)
    return(0);

  /* 127.0.0.1 */
  if(net == 127)
    return(0);

  /* Class E */
  if((net & 0xf0) == 0xf0)
    return(0);

  return(1);
}

/*
 * Does a packet contain a source route option?
 */
const uint8_t *
ICMPError::valid_source_route(const click_ip *iph)
{
  const uint8_t *oa = (const uint8_t *)iph;
  int hlen = iph->ip_hl << 2;

  for (int oi = sizeof(click_ip); oi < hlen; ) {
    // handle one-byte options
    unsigned type = oa[oi];
    if (type == IPOPT_NOP) {
      oi++;
      continue;
    } else if (type == IPOPT_EOL)
      return 0;

    // otherwise, get option length
    int xlen = oa[oi + 1];
    if (xlen < 2 || oi + xlen > hlen)
      return 0;
    else if ((type == IPOPT_LSRR || type == IPOPT_SSRR)
	     && oa[oi + 2] >= 4 && oa[oi + 2] % 4 == 0
	     && oa[oi + 2] <= xlen + 1)
      return oa + oi;
    else
      oi += xlen;
  }

  return 0;
}

Packet *
ICMPError::simple_action(Packet *p)
{
  WritablePacket *q = 0;
  const click_ip *ipp = p->ip_header();
  const uint8_t *source_route;
  click_ip *nip;
  click_icmp *icp;
  unsigned hlen, xlen;
  static int id = 1;

  if (!p->has_network_header())
    goto out;

  hlen = ipp->ip_hl << 2;

  /* These "don'ts" are from RFC1812 4.3.2.7: */

  /* Don't reply to ICMP error messages. */
  if(ipp->ip_p == IP_PROTO_ICMP) {
    const click_icmp *icmph = p->icmp_header();
    if(hlen + 4 > p->length() || is_error_type(icmph->icmp_type))
      goto out;
  }

  /* Don't respond to packets with IP broadcast destinations. */
  if(!unicast(ipp->ip_dst))
    goto out;

  /* Don't respond to e.g. Ethernet broadcasts or multicasts. */
  if (p->packet_type_anno() == Packet::BROADCAST || p->packet_type_anno() == Packet::MULTICAST)
    goto out;

  /* Don't respond is src is net 0 or invalid. */
  if(!valid_source(ipp->ip_src))
    goto out;

  /* Don't respond to fragments other than the first. */
  if(!IP_FIRSTFRAG(ipp))
    goto out;

  source_route = valid_source_route(ipp);
  if (source_route) {
    /* Don't send a redirect for a source-routed packet. 5.2.7.2 */
    if (_type == ICMP_REDIRECT)
      goto out;

    /* Ignore source route if ICMP Parameter Problem concerns the source
       route. 4.3.2.6 */
    if (_type == ICMP_PARAMPROB && _code == ICMP_PARAMPROB_ERRATPTR
	&& source_route <= ((const uint8_t *)ipp + ICMP_PARAMPROB_ANNO(p))
	&& ((const uint8_t *)ipp + ICMP_PARAMPROB_ANNO(p)) < (source_route + source_route[1]))
      source_route = 0;
  }

  // maximum size of ICMP packet is 576 bytes. 4.3.2.3
  q = Packet::make(_mtu);	// we made it configurable
  if (!q)
    goto out;

  // prepare IP header; guaranteed that packet data is aligned
  nip = reinterpret_cast<click_ip *>(q->data());

  nip->ip_v = 4;
  nip->ip_tos = 0;		// XXX should be same as incoming datagram?
  nip->ip_id = htons(id++);
  nip->ip_off = 0;
  nip->ip_ttl = 200;
  nip->ip_p = IP_PROTO_ICMP;
  nip->ip_sum = 0;
  nip->ip_src = _src_ip.in_addr();
  nip->ip_dst = ipp->ip_src;

  // include reversed source route if appropriate 4.3.2.6
  if (source_route) {
    uint8_t *o = (uint8_t *) (nip + 1);
    int olen = source_route[2] - 1;
    o[0] = source_route[0];	// copy option type
    o[1] = olen;
    o[2] = 4;
    o[olen] = IPOPT_EOL;
    o += 3;
    for (const uint8_t *oo = source_route + source_route[2] - 5; oo >= source_route + 3; oo -= 4, o += 4)
      memcpy(o, oo, 4);
    nip->ip_hl = (sizeof(click_ip) + olen + 3) >> 2;
  } else
    nip->ip_hl = sizeof(click_ip) >> 2;
  q->set_ip_header(nip, nip->ip_hl << 2);

  // now, prepare ICMP header
  icp = q->icmp_header();
  icp->icmp_type = _type;
  icp->icmp_code = _code;
  icp->icmp_cksum = 0;
  icp->padding = 0;

  // set ICMP particulars
  if (_type == ICMP_PARAMPROB && _code == ICMP_PARAMPROB_ERRATPTR)
    /* Set the Parameter Problem pointer. */
    ((click_icmp_paramprob *) icp)->icmp_pointer = ICMP_PARAMPROB_ANNO(p);
  if (_type == ICMP_REDIRECT)
    ((click_icmp_redirect *) icp)->icmp_gateway = p->dst_ip_anno();
  if (_type == ICMP_UNREACH && _code == ICMP_UNREACH_NEEDFRAG)
    ((click_icmp_needfrag *) icp)->icmp_nextmtu = htons(_pmtu);

  // copy packet contents
  xlen = q->end_data() - (uint8_t *)(icp + 1);
  if ((int)xlen > p->network_length()) {
    q->take(xlen - p->network_length());
    xlen = p->network_length();
  }
  memcpy((uint8_t *)(icp + 1), p->network_header(), xlen);
  icp->icmp_cksum = click_in_cksum((unsigned char *)icp, sizeof(click_icmp) + xlen);

  // finish off IP header
  nip->ip_len = htons(q->network_length());
  nip->ip_sum = click_in_cksum((unsigned char *)nip, nip->ip_hl << 2);

  // set annotations
  q->set_dst_ip_anno(IPAddress(nip->ip_dst));
  if (_use_fix_anno)
    SET_FIX_IP_SRC_ANNO(q, 1);
  q->timestamp_anno().assign_now();

 out:
  p->kill();
  return(q);
}

void
ICMPError::add_handlers()
{
    add_data_handlers("src", Handler::OP_READ | Handler::OP_WRITE, &_src_ip);
    add_data_handlers("mtu", Handler::OP_READ | Handler::OP_WRITE, &_mtu);
    add_data_handlers("pmtu", Handler::OP_READ | Handler::OP_WRITE, &_pmtu);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMPError)
ELEMENT_MT_SAFE(ICMPError)
