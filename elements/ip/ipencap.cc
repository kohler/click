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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "elements/standard/alignmentinfo.hh"

IPEncap::IPEncap()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

IPEncap::~IPEncap()
{
  MOD_DEC_USE_COUNT;
}

IPEncap *
IPEncap::clone() const
{
  return new IPEncap;
}

int
IPEncap::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  click_ip iph;
  memset(&iph, 0, sizeof(click_ip));
  iph.ip_v = 4;
  iph.ip_hl = sizeof(click_ip) >> 2;
  iph.ip_tos = 0;
  iph.ip_off = 0;
  iph.ip_ttl = 250;
  iph.ip_sum = 0;
  int tos = -1, dscp = -1;
  bool ce = false, df = false;
  String ect_str;
  
  if (cp_va_parse(conf, this, errh,
		  cpByte, "protocol", &iph.ip_p,
		  cpIPAddress, "source address", &iph.ip_src,
		  cpIPAddress, "destination address", &iph.ip_dst,
		  cpKeywords,
		  "TOS", cpUnsigned, "TOS", &tos,
		  "TTL", cpByte, "time-to-live", &iph.ip_ttl,
		  "DSCP", cpUnsigned, "DSCP", &dscp,
		  "ECT", cpKeyword, "ECN capable transport", &ect_str,
		  "CE", cpBool, "ECN congestion experienced", &ce,
		  "DF", cpBool, "don't fragment", &df,
		  0) < 0)
    return -1;

  int ect = 0;
  if (ect_str) {
    bool x;
    if (cp_bool(ect_str, &x))
      ect = x;
    else if (ect_str == "2")
      ect = 2;
    else
      return errh->error("bad ECT value `%s'", ect_str.cc());
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
  
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  // check alignment
  {
    int ans, c, o;
    ans = AlignmentInfo::query(this, 0, c, o);
    _aligned = (ans && c == 4 && o == 0);
    if (!_aligned)
      errh->warning("IP header unaligned, cannot use fast IP checksum");
    if (!ans)
      errh->message("(Try passing the configuration through `click-align'.)");
  }
#endif
  
  return 0;
}

int
IPEncap::initialize(ErrorHandler *)
{
  _id = 0;
  return 0;
}

Packet *
IPEncap::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->push(sizeof(click_ip));
  if (!p) return 0;
  
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  memcpy(ip, &_iph, sizeof(click_ip));
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.read_and_add(1));

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  if (_aligned)
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
  else
    ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
  ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif
  
  p->set_dst_ip_anno(IPAddress(ip->ip_dst));
  p->set_ip_header(ip, sizeof(click_ip));
  
  return p;
}

String
IPEncap::read_handler(Element *e, void *thunk)
{
  IPEncap *ipe = static_cast<IPEncap *>(e);
  switch ((int)thunk) {
   case 0:	return IPAddress(ipe->_iph.ip_src).s() + "\n";
   case 1:	return IPAddress(ipe->_iph.ip_dst).s() + "\n";
   default:	return "<error>\n";
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

EXPORT_ELEMENT(IPEncap)
ELEMENT_MT_SAFE(IPEncap)
