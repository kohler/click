/*
 * icmperror.{cc,hh} -- element constructs ICMP error packets
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/click_icmp.h>
#include <click/click_ip.h>
#include <click/ipaddressset.hh>
#include "icmperror.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>

ICMPError::ICMPError()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
  _code = _type = -1;
  _bad_addrs = 0;
}

ICMPError::~ICMPError()
{
  MOD_DEC_USE_COUNT;
  delete[] _bad_addrs;
}

int
ICMPError::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String bad_addr_str;
  IPAddressSet bad_addrs;
  if (cp_va_parse(conf, this, errh,
                  cpIPAddress, "Source IP address", &_src_ip,
                  cpInteger, "ICMP Type", &_type,
                  cpInteger, "ICMP Code", &_code,
		  cpOptional,
		  cpIPAddressSet, "bad IP addresses", &bad_addrs,
		  0) < 0)
    return -1;

  delete[] _bad_addrs;
  _n_bad_addrs = bad_addrs.size();
  _bad_addrs = bad_addrs.list_copy();

  return 0;
}

bool
ICMPError::is_error_type(int type)
{
  return(type == ICMP_DST_UNREACHABLE ||  	// 3
         type == ICMP_SOURCE_QUENCH ||		// 4
         type == ICMP_REDIRECT ||		// 5
         type == ICMP_TYPE_TIME_EXCEEDED ||	// 11
         type == ICMP_PARAMETER_PROBLEM);	// 12
}

int
ICMPError::initialize(ErrorHandler *errh)
{
  if (_type < 0 || _code < 0 || _src_ip.addr() == 0)
    return errh->error("not configured");
  if(is_error_type(_type) == false)
    return errh->error("ICMP type %d is not an error type", _type);
  return 0;
}

/*
 * Is an IP address unicast?
 */
bool
ICMPError::unicast(struct in_addr aa)
{
  unsigned int a = aa.s_addr;
  unsigned int ha = ntohl(a);

  /* limited broadcast */
  if(ha == 0xffffffff)
    return(0);
  
  /* Class D multicast */
  if((ha & 0xf0000000u) == 0xe0000000u)
    return(0);

  /* limited broadcast */
  for (int i = 0; i < _n_bad_addrs; i++)
    if (a == _bad_addrs[i])
      return 0;
  
  return(1);
}

/*
 * Is a source IP address valid as defined in RFC1812 5.3.7
 * or 4.2.2.11 or 4.2.3.1?
 */
bool
ICMPError::valid_source(struct in_addr aa)
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
bool
ICMPError::has_route_opt(const click_ip *ip)
{
  int opts = (ip->ip_hl << 2) - sizeof(click_ip);
  u_char *base = (u_char *) (ip + 1);
  int i, optlen;

  for(i = 0; i < opts; i += optlen){
    int opt = base[i];
    if(opt == IPOPT_LSRR || opt == IPOPT_SSRR)
      return(1);
    if(opt == IPOPT_EOL)
      break;
    if(opt == IPOPT_NOP){
      optlen = 1;
    } else {
      optlen = base[i+1];
    }
  }

  return(0);
}

Packet *
ICMPError::simple_action(Packet *p)
{
  WritablePacket *q = 0;
  const click_ip *ipp = p->ip_header();
  click_ip *nip;
  struct icmp_generic *icp;
  unsigned hlen, xlen;
  static int id = 1;

  if (!ipp)
    goto out;

  hlen = ipp->ip_hl * 4;

  /* These "don'ts" are from RFC1812 4.3.2.7: */

  /* Don't reply to ICMP error messages. */
  if(ipp->ip_p == IP_PROTO_ICMP) {
    icp = (struct icmp_generic *) (((char *)ipp) + hlen);
    if(hlen + 4 > p->length() || is_error_type(icp->icmp_type))
      goto out;
  }

  /* Don't respond to packets with IP broadcast destinations. */
  if(unicast(ipp->ip_dst) == 0)
    goto out;

  /* Don't respond to e.g. Ethernet broadcasts or multicasts. */
  if (p->packet_type_anno() == Packet::BROADCAST || p->packet_type_anno() == Packet::MULTICAST)
    goto out;

  /* Don't respond is src is net 0 or invalid. */
  if(valid_source(ipp->ip_src) == 0)
    goto out;

  /* Don't respond to fragments other than the first. */
  if(ntohs(ipp->ip_off) & IP_OFFMASK){
    goto out;
  }

  /* Don't send a redirect for a source-routed packet. 5.2.7.2 */
  if(_type == 5 && has_route_opt(ipp))
    goto out;

  /* send back IP header and 8 bytes of payload */
  xlen = p->length() - p->network_header_offset();
  if (xlen > hlen + 8)
    xlen = hlen + 8;

  q = Packet::make(sizeof(click_ip) + sizeof(struct icmp_generic) + xlen);
  // guaranteed that packet data is aligned
  memset(q->data(), '\0', q->length());
  nip = reinterpret_cast<click_ip *>(q->data());
  nip->ip_v = 4;
  nip->ip_hl = sizeof(click_ip) >> 2;
  nip->ip_len = htons(q->length());
  nip->ip_id = htons(id++);
  nip->ip_p = IP_PROTO_ICMP; /* icmp */
  nip->ip_ttl = 200;
  nip->ip_src = _src_ip.in_addr();
  nip->ip_dst = ipp->ip_src;
  nip->ip_sum = in_cksum((unsigned char *)nip, sizeof(click_ip));

  icp = (struct icmp_generic *) (nip + 1);
  icp->icmp_type = _type;
  icp->icmp_code = _code;

  if(_type == 12 && _code == 0){
    /* Set the Parameter Problem pointer. */
    ((struct icmp_param *) icp)->pointer = ICMP_PARAM_PROB_ANNO(p);
  }
  if(_type == 5){
    /* Redirect */
    ((struct icmp_redirect *) icp)->gateway = p->dst_ip_anno().addr();
  }

  memcpy((void *)(icp + 1), p->network_header(), xlen);
  icp->icmp_cksum = in_cksum((unsigned char *)icp, sizeof(icmp_generic) + xlen);

  q->set_dst_ip_anno(IPAddress(nip->ip_dst));
  SET_FIX_IP_SRC_ANNO(q, 1);
  q->set_ip_header(nip, sizeof(click_ip));

 out:
  p->kill();
  return(q);
}

EXPORT_ELEMENT(ICMPError)
ELEMENT_MT_SAFE(ICMPError)
