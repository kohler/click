/*
 * gridproxy.{cc,hh}linkstat.{cc,hh}
 * John Bicket
 *
 * Copyright (c) 1999-2002 Massachusetts Institute of Technology
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
#include "gridproxy.hh"
#include <click/confparse.hh>
CLICK_DECLS

GridProxy::GridProxy()
  : _map(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_input();
  add_output();
  add_output();
  
}

GridProxy::~GridProxy()
{
  MOD_DEC_USE_COUNT;
}

void 
GridProxy::cleanup(CleanupStage) 
{

}
void *
GridProxy::cast(const char *n)
{
  if (strcmp(n, "GridProxy") == 0)
    return (GridProxy *)this;
  else
    return 0;
}

int
GridProxy::configure(Vector<String> &conf, ErrorHandler *errh)
{
  click_ip iph;
  memset(&iph, 0, sizeof(click_ip));
  iph.ip_v = 4;
  iph.ip_hl = sizeof(click_ip) >> 2;
  iph.ip_tos = 0;
  iph.ip_off = 0;
  iph.ip_ttl = 250;
  iph.ip_sum = 0;
  iph.ip_p = 4; /* IP in IP */

  int res = cp_va_parse(conf, this, errh,
			cpIPAddress, "proxy address", &iph.ip_src,
			cpKeywords,
			0);

  if (res < 0)
    return res;
  

  _iph = iph;
  return res;
}


int
GridProxy::initialize(ErrorHandler *)
{
  _id = 0;
  return 0;
}



void GridProxy::push(int port, Packet *p_in)
{
  if (0 == port) {
    forward_mapping(p_in);
  } else {
    reverse_mapping(p_in);
  }
}


void 
GridProxy::reverse_mapping(Packet *p_in) {

  /* decide where to send it */
  IPAddress dst;
  IPAddress gateway;
  dst = IPAddress(p_in->ip_header()->ip_dst);
  gateway = IPAddress(_map[dst]);

  if (!gateway) {
    //click_chatter("couldn't find a gateway for %s!\n", dst.s().cc());
    p_in->kill();
    return;
  }
  /* do ip-to-ip encapsulation */
  WritablePacket *p = p_in->push(sizeof(click_ip));
  if (!p) {
    p_in->kill();
    return;
  }

  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  memcpy(ip, &_iph, sizeof(click_ip));
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.read_and_add(1));

  p->set_dst_ip_anno(gateway);
  p->set_ip_header(ip, sizeof(click_ip));
  p->ip_header()->ip_dst = gateway.in_addr();


#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  if (_aligned)
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
  else
    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
  ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif

  output(1).push(p);  
}

void
GridProxy::forward_mapping(Packet *p_in) {

  IPAddress gateway;
  IPAddress src;
  
  gateway = IPAddress(p_in->ip_header()->ip_src);

  /* strip the ip header to get the actual ip packet */
  p_in->pull((int)p_in->ip_header_offset() + p_in->ip_header_length());

  /* set the new ip header*/ 
  const click_ip *ip = reinterpret_cast<const click_ip *>(p_in->data());
  p_in->set_ip_header(ip, ip->ip_hl << 2);

  /* record the gateway that the src picked */
  src = IPAddress(p_in->ip_header()->ip_src);

  _map.insert(src, gateway);

  output(0).push(p_in);  
  return;
}
void
GridProxy::add_handlers()
{

}


CLICK_ENDDECLS
EXPORT_ELEMENT(GridProxy)


