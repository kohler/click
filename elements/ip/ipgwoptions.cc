/*
 * ipgwoptions.{cc,hh} -- element processes IP Timestamp and Record Route
 * options
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
#include "ipgwoptions.hh"
#include <click/click_ip.h>
#include <click/ipaddressset.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>

IPGWOptions::IPGWOptions()
{
  MOD_INC_USE_COUNT;
  _drops = 0;
  _other_ips = 0;
  add_input();
  add_output();
}

IPGWOptions::~IPGWOptions()
{
  MOD_DEC_USE_COUNT;
  delete[] _other_ips;
}

int
IPGWOptions::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  IPAddress my_ip;
  IPAddressSet ips;
  if (cp_va_parse(conf, this, errh,
                  cpIPAddress, "local IP address", &my_ip,
		  cpOptional,
		  cpIPAddressSet, "other interface IP addresses", &ips,
		  0) < 0)
    return -1;
  
  _my_ip = my_ip.in_addr();
  
  ips.insert(my_ip);
  delete[] _other_ips;
  _n_other_ips = ips.size();
  _other_ips = ips.list_copy();
  
  return 0;
}

void
IPGWOptions::notify_noutputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  set_noutputs(n < 2 ? 1 : 2);
}

IPGWOptions *
IPGWOptions::clone() const
{
  return new IPGWOptions();
}

Packet *
IPGWOptions::handle_options(Packet *p_in)
{
  /* This is lame: should be lazier. */
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  unsigned hlen = ip->ip_hl << 2;

  u_char *oa = (u_char *) (ip + 1);
  int olen = hlen - sizeof(click_ip);
  int oi;
  int do_cksum = 0;
  int problem_offset = -1;

  for(oi = 0; oi < olen; ){
    unsigned type = oa[oi];
    int xlen;
    if(type <= 1)
      xlen = 1;
    else
      xlen = oa[oi+1];
    if(oi + xlen > olen)
      break;
    if(type == IPOPT_EOL){
      /* end of option list */
      break;
    } else if(type == IPOPT_RR){
      /*
       * Record Route.
       * Apparently the pointer (oa[oi+2]) is 1-origin.
       */
      int p = oa[oi+2] - 1;
      if(p >= 3 && p+4 <= xlen){
        memcpy(oa + oi + p, &_my_ip, 4);
        oa[oi+2] += 4;
        do_cksum = 1;
      } else if(p != xlen){
        problem_offset = 20 + oi + 2;
        goto send_error;
      }
    } else if(type == IPOPT_TS){
      /*
       * Timestamp Option.
       * We can't do a good job with the pre-specified mode (flg=3),
       * since we don't know all our i/f addresses.
       */
      int p = oa[oi+2] - 1;
      int oflw = oa[oi+3] >> 4;
      int flg = oa[oi+3] & 0xf;
      int overflowed = 0;

      struct timeval tv;
      click_gettimeofday(&tv);
      int ms = htonl((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);

      if(p < 4){
        problem_offset = 20 + oi + 2;
        goto send_error;
      } else if(flg == 0){
        /* 32-bit timestamps only */
        if(p+4 <= xlen){
          memcpy(oa + oi + p, &ms, 4);
          oa[oi+2] += 4;
          do_cksum = 1;
        } else {
          overflowed = 1;
        }
      } else if(flg == 1){
        /* ip address followed by timestamp */
        if(p+8 <= xlen){
          memcpy(oa + oi + p, &_my_ip, 4);
          memcpy(oa + oi + p + 4, &ms, 4);
          oa[oi+2] += 8;
          do_cksum = 1;
        } else {
          overflowed = 1;
        }
      } else if (flg == 3 && p + 8 <= xlen) {
	unsigned addr, doit = 0;
	memcpy(&addr, oa + oi + p, 4);
	for (int i = 0; !doit && i < _n_other_ips; i++)
	  doit = (addr == _other_ips[i]);
        /* only if it's my address */
	if (doit) {
          memcpy(oa + oi + p + 4, &ms, 4);
          oa[oi+2] += 8;
          do_cksum = 1;
        }
      }
      if(overflowed){
        if(oflw < 15){
          oa[oi+3] = ((oflw + 1) << 4) | flg;
          do_cksum = 1;
        } else {
          problem_offset = 20 + oi + 3;
          goto send_error;
        }
      }
    }
    oi += xlen;
  }

  if(do_cksum){
    ip->ip_sum = 0;
    ip->ip_sum = in_cksum(p->data(), hlen);
  }

  return(p);

 send_error:
  _drops++;
  if (noutputs() == 2){
    SET_ICMP_PARAM_PROB_ANNO(p, problem_offset);
    output(1).push(p);
  } else {
    p->kill();
  }
  return 0;
}

Packet *
IPGWOptions::simple_action(Packet *p)
{
  const click_ip *ip = p->ip_header();
  assert(ip);
  unsigned hlen = ip->ip_hl << 2;
  if (hlen <= sizeof(click_ip) || (p = handle_options(p)))
    return(p);
  return(0);
}

static String
IPGWOptions_read_drops(Element *xf, void *)
{
  IPGWOptions *f = (IPGWOptions *)xf;
  return String(f->drops()) + "\n";
}

void
IPGWOptions::add_handlers()
{
  add_read_handler("drops", IPGWOptions_read_drops, 0);
}

EXPORT_ELEMENT(IPGWOptions)
ELEMENT_MT_SAFE(IPGWOptions)
