/*
 * icmperror.{cc,hh} -- element constructs ICMP error packets
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
#include <clicknet/icmp.h>
#include <clicknet/ip.h>
#include <click/ipaddressset.hh>
#include "icmperror.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

ICMPError::ICMPError()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
  _code = _type = -1;
  _bad_addrs = 0;
  if (cp_register_stringlist_argtype("ICMP.type", "ICMP message type", cpArgAllowNumbers) == 0)
    cp_extend_stringlist_argtype("ICMP.type",
				 "echo-reply", ICMP_ECHOREPLY,
				 "unreachable", ICMP_UNREACH,
				 "sourcequench", ICMP_SOURCEQUENCH,
				 "redirect", ICMP_REDIRECT,
				 "echo", ICMP_ECHO,
				 "routeradvert", ICMP_ROUTERADVERT,
				 "routersolicit", ICMP_ROUTERSOLICIT,
				 "timeexceeded", ICMP_TIMXCEED,
				 "parameterproblem", ICMP_PARAMPROB,
				 "timestamp", ICMP_TSTAMP,
				 "timestamp-reply", ICMP_TSTAMPREPLY,
				 "inforeq", ICMP_IREQ,
				 "inforeq-reply", ICMP_IREQREPLY,
				 "maskreq", ICMP_MASKREQ,
				 "maskreq-reply", ICMP_MASKREQREPLY,
				 (const char *)0);
  if (cp_register_stringlist_argtype("ICMP.code", "ICMP message code", cpArgAllowNumbers) == 0) {
    cp_extend_stringlist_argtype("ICMP.code",
				 "net", ICMP_UNREACH_NET,
				 "host", ICMP_UNREACH_HOST,
				 "protocol", ICMP_UNREACH_PROTOCOL,
				 "port", ICMP_UNREACH_PORT,
				 "needfrag", ICMP_UNREACH_NEEDFRAG,
				 /* other UNREACH constants missing */
				 "transit", ICMP_TIMXCEED_TRANSIT,
				 "reassembly", ICMP_TIMXCEED_REASSEMBLY,
				 "erroratptr", ICMP_PARAMPROB_ERRATPTR,
				 "missingopt", ICMP_PARAMPROB_OPTABSENT,
				 "length", ICMP_PARAMPROB_LENGTH,
				 (const char *)0);
    static_assert(ICMP_UNREACH_NET == ICMP_REDIRECT_NET && ICMP_UNREACH_HOST == ICMP_REDIRECT_HOST);
  }
}

ICMPError::~ICMPError()
{
  MOD_DEC_USE_COUNT;
  delete[] _bad_addrs;
  cp_unregister_argtype("ICMP.type");
  cp_unregister_argtype("ICMP.code");
}

int
ICMPError::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String bad_addr_str;
  IPAddressSet bad_addrs;
  _code = 0;
  if (cp_va_parse(conf, this, errh,
                  cpIPAddress, "source IP address", &_src_ip,
                  "ICMP.type", "ICMP error type", &_type,
		  cpOptional,
                  "ICMP.code", "ICMP error code", &_code,
		  cpIPAddressSet, "bad IP addresses", &bad_addrs,
		  cpKeywords,
		  "BADADDRS", cpIPAddressSet, "bad IP addresses", &bad_addrs,
		  0) < 0)
    return -1;
  if (_type < 0 || _type > 255 || _code < 0 || _code > 255)
    return errh->error("ICMP type and code must be between 0 and 255");

  delete[] _bad_addrs;
  _n_bad_addrs = bad_addrs.size();
  _bad_addrs = bad_addrs.list_copy();

  return 0;
}

bool
ICMPError::is_error_type(int type)
{
  return type == ICMP_UNREACH || type == ICMP_SOURCEQUENCH
    || type == ICMP_REDIRECT ||	type == ICMP_TIMXCEED
    || type == ICMP_PARAMPROB;
}

int
ICMPError::initialize(ErrorHandler *errh)
{
  if (_type < 0 || _code < 0 || _src_ip.addr() == 0)
    return errh->error("not configured");
  if (is_error_type(_type) == false)
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
  click_icmp *icp;
  unsigned hlen, xlen;
  static int id = 1;

  if (!ipp)
    goto out;

  hlen = ipp->ip_hl * 4;

  /* These "don'ts" are from RFC1812 4.3.2.7: */

  /* Don't reply to ICMP error messages. */
  if(ipp->ip_p == IP_PROTO_ICMP) {
    icp = (click_icmp *) (((char *)ipp) + hlen);
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

  q = Packet::make(sizeof(click_ip) + sizeof(click_icmp) + xlen);
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
  nip->ip_sum = click_in_cksum((unsigned char *)nip, sizeof(click_ip));

  icp = (click_icmp *) (nip + 1);
  icp->icmp_type = _type;
  icp->icmp_code = _code;

  if (_type == ICMP_PARAMPROB && _code == ICMP_PARAMPROB_ERRATPTR) {
    /* Set the Parameter Problem pointer. */
    ((click_icmp_paramprob *) icp)->icmp_pointer = ICMP_PARAMPROB_ANNO(p);
  }
  if (_type == ICMP_REDIRECT) {
    /* Redirect */
    ((click_icmp_redirect *) icp)->icmp_gateway = p->dst_ip_anno();
  }

  memcpy((void *)(icp + 1), p->network_header(), xlen);
  icp->icmp_cksum = click_in_cksum((unsigned char *)icp, sizeof(click_icmp) + xlen);

  q->set_dst_ip_anno(IPAddress(nip->ip_dst));
  SET_FIX_IP_SRC_ANNO(q, 1);
  q->set_ip_header(nip, sizeof(click_ip));

 out:
  p->kill();
  return(q);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMPError)
ELEMENT_MT_SAFE(ICMPError)
