/*
 * ipencap.{cc,hh} -- element encapsulates packet in IP header
 * Robert Morris, Eddie Kohler, Alex Snoeren
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
#include "ipencap.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

IPEncap::IPEncap()
{
}

IPEncap::~IPEncap()
{
}

int
IPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  click_ip iph;
  memset(&iph, 0, sizeof(click_ip));
  iph.ip_v = 4;
  iph.ip_hl = sizeof(click_ip) >> 2;
  iph.ip_ttl = 250;
  int proto, tos = -1, dscp = -1;
  bool ce = false, df = false, use_dst_anno;
  String ect_str;

  use_dst_anno = (conf.size() >= 3 && conf[2] == "DST_ANNO");
  if (use_dst_anno)
      conf[2] = "0.0.0.0";
  
  if (cp_va_parse(conf, this, errh,
		  cpNamedInteger, "protocol", NameInfo::T_IP_PROTO, &proto,
		  cpIPAddress, "source address", &iph.ip_src,
		  cpIPAddress, "destination address", &iph.ip_dst,
		  cpKeywords,
		  "TOS", cpUnsigned, "TOS", &tos,
		  "TTL", cpByte, "time-to-live", &iph.ip_ttl,
		  "DSCP", cpUnsigned, "DSCP", &dscp,
		  "ECT", cpKeyword, "ECN capable transport", &ect_str,
		  "CE", cpBool, "ECN congestion experienced", &ce,
		  "DF", cpBool, "don't fragment", &df,
		  cpEnd) < 0)
    return -1;

  if (proto < 0 || proto > 255)
      return errh->error("bad IP protocol");
  iph.ip_p = proto;
  
  int ect = 0;
  if (ect_str) {
    bool x;
    if (cp_bool(ect_str, &x))
      ect = x;
    else if (ect_str == "2")
      ect = 2;
    else
      return errh->error("bad ECT value '%s'", ect_str.c_str());
  }
  
  if (tos >= 0 && dscp >= 0)
    return errh->error("cannot set both TOS and DSCP");
  else if (tos >= 0 && (ect || ce))
    return errh->error("cannot set both TOS and ECN bits");
  if (tos >= 256 || tos < -1)
    return errh->error("TOS too large; max 255");
  else if (dscp >= 63 || dscp < -1)
    return errh->error("DSCP too large; max 63");
  if (ect && ce)
    return errh->error("can set at most one ECN option");

  if (tos >= 0)
    iph.ip_tos = tos;
  else if (dscp >= 0)
    iph.ip_tos = (dscp << 2);
  if (ect)
    iph.ip_tos |= (ect == 1 ? IP_ECN_ECT1 : IP_ECN_ECT2);
  if (ce)
    iph.ip_tos |= IP_ECN_CE;
  if (df)
    iph.ip_off |= htons(IP_DF);
  _iph = iph;

  // set the checksum field so we can calculate the checksum incrementally
#if HAVE_FAST_CHECKSUM
  _iph.ip_sum = ip_fast_csum((unsigned char *) &_iph, sizeof(click_ip) >> 2);
#else
  _iph.ip_sum = click_in_cksum((unsigned char *) &_iph, sizeof(click_ip));
#endif
  
  // store information about use_dst_anno in the otherwise useless
  // _iph.ip_len field
  _iph.ip_len = (use_dst_anno ? 1 : 0);
  
  return 0;
}

int
IPEncap::initialize(ErrorHandler *)
{
  _id = 0;
  return 0;
}

inline void
IPEncap::update_cksum(click_ip *ip, int off) const
{
#if HAVE_INDIFFERENT_ALIGNMENT
    click_update_in_cksum(&ip->ip_sum, 0, ((uint16_t *) ip)[off/2]);
#else
    const uint8_t *u = (const uint8_t *) ip;
# if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    click_update_in_cksum(&ip->ip_sum, 0, u[off]*256 + u[off+1]);
# else
    click_update_in_cksum(&ip->ip_sum, 0, u[off] + u[off+1]*256);
# endif
#endif
}

Packet *
IPEncap::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->push(sizeof(click_ip));
  if (!p) return 0;
  
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  memcpy(ip, &_iph, sizeof(click_ip));
  if (ip->ip_len) {		// use_dst_anno
      ip->ip_dst = p->dst_ip_anno();
      update_cksum(ip, 16);
      update_cksum(ip, 18);
  } else
      p->set_dst_ip_anno(IPAddress(ip->ip_dst));
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.fetch_and_add(1));
  update_cksum(ip, 2);
  update_cksum(ip, 4);
  
  p->set_ip_header(ip, sizeof(click_ip));
  
  return p;
}

String
IPEncap::read_handler(Element *e, void *thunk)
{
  IPEncap *ipe = static_cast<IPEncap *>(e);
  switch ((intptr_t)thunk) {
    case 0:
      return IPAddress(ipe->_iph.ip_src).unparse();
    case 1:
      if (ipe->_iph.ip_len == 1)
	  return "DST_ANNO";
      else
	  return IPAddress(ipe->_iph.ip_dst).unparse();
    default:
      return "<error>";
  }
}

void
IPEncap::add_handlers()
{
  add_read_handler("src", read_handler, (void *)0);  
  add_write_handler("src", reconfigure_positional_handler, (void *)1);
  add_read_handler("dst", read_handler, (void *)1);  
  add_write_handler("dst", reconfigure_positional_handler, (void *)2);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPEncap)
ELEMENT_MT_SAFE(IPEncap)
