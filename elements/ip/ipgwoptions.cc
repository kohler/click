/*
 * ipgwoptions.{cc,hh} -- element processes IP Timestamp and Record Route
 * options
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
#include "ipgwoptions.hh"
#include <clicknet/ip.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

IPGWOptions::IPGWOptions()
{
  MOD_INC_USE_COUNT;
  _drops = 0;
  add_input();
  add_output();
}

IPGWOptions::~IPGWOptions()
{
  MOD_DEC_USE_COUNT;
}

int
IPGWOptions::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpIPAddress, "local IP address", &_preferred_addr,
		  cpOptional,
		  cpIPAddressList, "other interface IP addresses", &_my_addrs,
		  0) < 0)
    return -1;
  _my_addrs.insert(_preferred_addr);
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
IPGWOptions::handle_options(Packet *p)
{
  WritablePacket *wp = 0;
  const uint8_t *oa = p->network_header();
  int hlen = p->network_header_length();

  int oi;
  for (oi = sizeof(click_ip); oi < hlen; ) {
    // handle one-byte options
    unsigned type = oa[oi];
    if (type == IPOPT_NOP) {
      oi++;
      continue;
    } else if (type == IPOPT_EOL)
      /* end of option list */
      break;

    // otherwise, get option length
    int xlen = oa[oi + 1];
    if (xlen < 2 || oi + xlen > hlen) {
      // bad length
      oi++;			// to point at length
      goto send_error;
    } else if (type != IPOPT_RR && type != IPOPT_TS) {
      // not for us to process
      oi += xlen;
      continue;
    }

    // need a writable packet
    if (!wp) {
      if (!(wp = p->uniqueify()))
	return 0;
      oa = wp->network_header(); // may have changed due to packet copy
    }
    uint8_t *woa = wp->network_header();
    
    if(type == IPOPT_RR){
      /*
       * Record Route.
       * Apparently the pointer (oa[oi+2]) is 1-origin.
       */
      int p = oa[oi+2] - 1;
      if (p >= 3 && p + 4 <= xlen) {
        memcpy(woa + oi + p, &_preferred_addr, 4);
        woa[oi+2] += 4;
      } else if (p != xlen) {
	oi += 2;
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
      bool overflowed = 0;

      struct timeval tv;
      click_gettimeofday(&tv);
      int ms = htonl((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);

      if(p < 4){
	oi += 2;
        goto send_error;
      } else if(flg == 0){
        /* 32-bit timestamps only */
        if(p+4 <= xlen){
          memcpy(woa + oi + p, &ms, 4);
          woa[oi+2] += 4;
        } else
          overflowed = 1;
      } else if(flg == 1){
        /* ip address followed by timestamp */
        if(p+8 <= xlen){
          memcpy(woa + oi + p, &_preferred_addr, 4);
          memcpy(woa + oi + p + 4, &ms, 4);
          woa[oi+2] += 8;
        } else
          overflowed = 1;
      } else if (flg == 3 && p + 8 <= xlen) {
	unsigned addr;
	memcpy(&addr, oa + oi + p, 4);
        /* only if it's my address */
	if (_my_addrs.contains(addr)) {
          memcpy(woa + oi + p + 4, &ms, 4);
          woa[oi+2] += 8;
        }
      } else {
	oi += 3;
	goto send_error;
      }
      if (overflowed) {
        if (oflw < 15)
          woa[oi+3] = ((oflw + 1) << 4) | flg;
	else {
	  oi += 3;
          goto send_error;
        }
      }
    }
    
    oi += xlen;
  }

  if (wp) {
    click_ip *iph = wp->ip_header();
    iph->ip_sum = 0;
    iph->ip_sum = click_in_cksum(p->network_header(), hlen);
  }

  return (wp ? wp : p);

 send_error:
  _drops++;
  SET_ICMP_PARAMPROB_ANNO(p, oi);
  checked_output_push(1, p);
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

CLICK_ENDDECLS
EXPORT_ELEMENT(IPGWOptions)
ELEMENT_MT_SAFE(IPGWOptions)
