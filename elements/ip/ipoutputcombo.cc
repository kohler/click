/*
 * ipoutputcombo.{cc,hh} -- IP router output combination element
 * Eddie Kohler
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
#include "ipoutputcombo.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

IPOutputCombo::IPOutputCombo()
{
}

IPOutputCombo::~IPOutputCombo()
{
}

int
IPOutputCombo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("COLOR", _color)
	.read_mp("IPADDR", _my_ip)
	.read_mp("MTU", _mtu).complete();
}

void
IPOutputCombo::push(int, Packet *p_in)
{
  int do_cksum = 0;
  int problem_offset = -1;

  // DropBroadcasts
  if (p_in->packet_type_anno() == Packet::BROADCAST || p_in->packet_type_anno() == Packet::MULTICAST) {
    p_in->kill();
    return;
  }

  // PaintTee
  if (PAINT_ANNO(p_in) == _color)
    output(1).push(p_in->clone());

  // IPGWOptions
  WritablePacket *p = p_in->uniqueify();
  assert(p->has_network_header());
  click_ip *ip = p->ip_header();
  unsigned hlen = (ip->ip_hl << 2);

  if (hlen > sizeof(click_ip)) {
    uint8_t *woa = p->network_header();
    int hlen = p->network_header_length();

    int oi;
    for (oi = sizeof(click_ip); oi < hlen; ) {
      // handle one-byte options
      unsigned type = woa[oi];
      if (type == IPOPT_NOP) {
	oi++;
	continue;
      } else if (type == IPOPT_EOL)
	/* end of option list */
	break;

      // otherwise, get option length
      int xlen = woa[oi + 1];
      if (xlen < 2 || oi + xlen > hlen) {
	// bad length
	problem_offset = oi + 1; // to point at length
	goto ipgw_send_error;
      } else if (type != IPOPT_RR && type != IPOPT_TS) {
	// not for us to process
	oi += xlen;
	continue;
      }

      // have a writable packet already

      if(type == IPOPT_RR){
	/*
	 * Record Route.
	 * Apparently the pointer (oa[oi+2]) is 1-origin.
	 */
	int p = woa[oi+2] - 1;
	if (p >= 3 && p + 4 <= xlen) {
	  memcpy(woa + oi + p, &_my_ip, 4);
	  woa[oi+2] += 4;
	  do_cksum = 1;
	} else if (p != xlen) {
	  problem_offset = oi + 2;
	  goto ipgw_send_error;
	}
      } else if(type == IPOPT_TS){
	/*
	 * Timestamp Option.
	 * We can't do a good job with the pre-specified mode (flg=3),
	 * since we don't know all our i/f addresses.
	 */
	int p = woa[oi+2] - 1;
	int oflw = woa[oi+3] >> 4;
	int flg = woa[oi+3] & 0xf;
	bool overflowed = 0;

	Timestamp now = Timestamp::now();
	int ms = htonl((now.sec() % 86400)*1000 + now.msec());

	if(p < 4){
	  problem_offset = oi + 2;
	  goto ipgw_send_error;
	} else if(flg == 0){
	  /* 32-bit timestamps only */
	  if(p+4 <= xlen){
	    memcpy(woa + oi + p, &ms, 4);
	    woa[oi+2] += 4;
	    do_cksum = 1;
	  } else
	    overflowed = 1;
	} else if(flg == 1){
	  /* ip address followed by timestamp */
	  if(p+8 <= xlen){
	    memcpy(woa + oi + p, &_my_ip, 4);
	    memcpy(woa + oi + p + 4, &ms, 4);
	    woa[oi+2] += 8;
	    do_cksum = 1;
	  } else
	    overflowed = 1;
	} else if (flg == 3 && p + 8 <= xlen) {
	  /* only if it's my address */
	  if (memcmp(woa + oi + p, &_my_ip, 4) == 0) {
	    memcpy(woa + oi + p + 4, &ms, 4);
	    woa[oi+2] += 8;
	    do_cksum = 1;
	  }
	} else {
	  problem_offset = oi + 3;
	  goto ipgw_send_error;
	}
	if (overflowed) {
	  if (oflw < 15) {
	    woa[oi+3] = ((oflw + 1) << 4) | flg;
	    do_cksum = 1;
	  } else {
	    problem_offset = oi + 3;
	    goto ipgw_send_error;
	  }
	}
      }

      oi += xlen;
    }
  }

  // FixIPSrc
  if (FIX_IP_SRC_ANNO(p)) {
    SET_FIX_IP_SRC_ANNO(p, 0);
    ip->ip_src = _my_ip;
    do_cksum = 1;
  }

  // IPGWOptions / FixIPSrc
  if (do_cksum) {
    ip->ip_sum = 0;
    ip->ip_sum = click_in_cksum(p->data(), hlen);
  }

  // DecIPTTL
  if (ip->ip_ttl <= 1) {
    output(3).push(p);
    return;
  } else {
    ip->ip_ttl--;
    // 19.Aug.1999 - incrementally update IP checksum as suggested by SOSP
    // reviewers, according to RFC1141 and RFC1624
    unsigned long sum = (~ntohs(ip->ip_sum) & 0xFFFF) + 0xFEFF;
    ip->ip_sum = ~htons(sum + (sum >> 16));
  }

  // Fragmenter
  if (p->length() > _mtu) {
    output(4).push(p);		// expect a Fragmenter there
    return;
  }

  output(0).push(p);
  return;

 ipgw_send_error:
  SET_ICMP_PARAMPROB_ANNO(p, problem_offset);
  output(2).push(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPOutputCombo)
ELEMENT_MT_SAFE(IPOutputCombo)
