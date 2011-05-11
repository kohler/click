/*
 * icmp6error.{cc,hh} -- element constructs ICMP6 error packets
 * Peilei Fan, Frederik Scholaert
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
#include <clicknet/icmp6.h>
#include <clicknet/ip6.h>
#include <clicknet/ip.h>
#include "icmp6error.hh"
#include <click/ip6address.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

ICMP6Error::ICMP6Error()
{
  _code = _type = -1;
}

ICMP6Error::~ICMP6Error()
{
}

int
ICMP6Error::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("SRC", _src_ip)
	.read_mp("TYPE", _type)
	.read_mp("CODE", _code)
	.complete();
}

bool
ICMP6Error::is_error_type(int type)
{
  return (type == ICMP6_UNREACH
	  || type == ICMP6_PKTTOOBIG
	  || type == ICMP6_TIMXCEED
	  || type == ICMP6_PARAMPROB);
}

bool
ICMP6Error::is_redirect_type(int type)
{
  return (type == ICMP6_REDIRECT);
}

int
ICMP6Error::initialize(ErrorHandler *errh)
{
  if (_type < 0 || _code < 0 || (_src_ip == IP6Address()))
    return errh->error("not configured -a");
  if (!is_error_type(_type) && !is_redirect_type(_type))
    return errh->error("ICMP6 type %d is not an error or redirect type", _type);
  return 0;
}

/*
 * Is an IP6 address unicast?
 * check if they are not multicast address (prefix is ff)
 * Can't detect directed broadcast here!
 */

bool
ICMP6Error::unicast(const IP6Address &aa)
{
  const unsigned char *a = aa.data();
  if (a[0]== 0xff)
    return(0);
  return(1);
}


/*
 * Is a source IP6 address valid
 *
 */

bool
ICMP6Error::valid_source(const IP6Address &aa)
{
  //unsigned int a = aa.s_addr;
  //unsigned int ha = ntohl(a);
  //unsigned net = (ha >> 24) & 0xff;

  /* broadcast or multicast */
 if(unicast(aa) == 0)
    return(0);

  /* local net or host: */
// if(net == 0)
//    return(0);

  /* I don't know how to detect directed broadcast. */

  /* ::1 */
 if(aa == IP6Address("::1"))
   return(0);

  return(1);
}

/*
 * Does a packet contain a source route option?
 * currently, let's say no option for this
 */

bool
ICMP6Error::has_route_opt(const click_ip6 *)
{
  return(0);
}


Packet *
ICMP6Error::simple_action(Packet *p)
{
  WritablePacket *q = 0;
  const click_ip6 *ipp = p->ip6_header();
  click_ip6 *nip;
  click_icmp6 *icp;
  unsigned xlen;


  if (!p->has_network_header())
    goto out;


  /* These "don'ts" are from RFC1885 2.4.e: */

  /* Don't reply to ICMP6 error messages. */
  if(ipp->ip6_nxt == IP_PROTO_ICMP6) {
    icp = (click_icmp6 *) ((char *)ipp);
    if( is_error_type(icp->icmp6_type))
      goto out;
  }

  /* Don't respond to packets with IPv6 broadcast destinations. */
  if(unicast(IP6Address(ipp->ip6_dst)) == 0)
    goto out;

  /* Don't respond to e.g. Ethernet broadcasts or multicasts. */
  if (p->packet_type_anno() == Packet::BROADCAST || p->packet_type_anno() == Packet::MULTICAST)
    goto out;


  /* send back as much of invoding packet as will fit without the ICMPv6 packet exceeding 576 octets , ICMP6 header is 8 octets*/

  xlen = p->length();
  if (xlen > 568)
    xlen = 568;

  if (_type != ICMP6_REDIRECT)
    q = Packet::make(sizeof(struct click_ip6) + sizeof(struct click_icmp6) + xlen);
  else
    q = Packet::make(sizeof(struct click_ip6) + sizeof(struct click_icmp6_redirect) + xlen);

  // guaranteed that packet data is aligned
  memset(q->data(), '\0', q->length());

  //set ip6 header
  nip = (click_ip6 *) q->data();
  nip->ip6_flow = 0;	/* must set first: overlaps vfc */
  nip->ip6_v = 6;
  nip->ip6_plen = htons(q->length()-40);
  nip->ip6_nxt = IP_PROTO_ICMP6;  /* next header */
  nip->ip6_hlim = 0xff; //what hop limit shoud I set?
  nip->ip6_src = _src_ip;
  nip->ip6_dst = ipp->ip6_src;

  //set icmp6 Message
  icp = (click_icmp6 *) (nip + 1);
  icp->icmp6_type = _type;
  icp->icmp6_code = _code;


  if(_type == ICMP6_PKTTOOBIG && _code == 0){
    /* Set the mtu value. */
    ((click_icmp6_pkttoobig *)icp)->icmp6_mtusize = 1500;
  }

  if(_type == 4 && _code == 0){
    /* Set the Parameter Problem pointer. */
    ((click_icmp6_paramprob *) icp)->icmp6_pointer = ICMP_PARAMPROB_ANNO(p);
    //the pointer should be 4 bytes, however, there's no space in Anno structure
    //temporarily use the same as the ICMP parameter pointer, will be dealt later
  }

  if (_type == ICMP6_REDIRECT && _code == 0) {
    click_icmp6_redirect *icpr = (click_icmp6_redirect *) (nip + 1);
    icpr->icmp6_target = DST_IP6_ANNO(p);
    icpr->icmp6_dst = ipp->ip6_dst;
    memcpy((void *)(icpr + 1), p->data(), xlen);
  } else
    memcpy((void *)(icp + 1), p->data(), xlen);

  icp->icmp6_cksum = htons(in6_fast_cksum(&nip->ip6_src, &nip->ip6_dst, nip->ip6_plen, nip->ip6_nxt, 0, (unsigned char *)icp, nip->ip6_plen));

  SET_DST_IP6_ANNO(q, IP6Address(nip->ip6_dst));
  SET_FIX_IP_SRC_ANNO(q, 1); // fix_ip_src: shared flag with IPv4
  q->set_ip6_header(nip, sizeof(click_ip6));

 out:
  p->kill();
  return(q);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMP6Error)
