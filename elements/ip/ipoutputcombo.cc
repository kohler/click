/*
 * ipoutputcombo.{cc,hh} -- IP router output combination element
 * Eddie Kohler
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
#include <click/package.hh>
#include "ipoutputcombo.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>

IPOutputCombo::IPOutputCombo()
  : Element(1, 5)
{
  MOD_INC_USE_COUNT;
}

IPOutputCombo::~IPOutputCombo()
{
  MOD_DEC_USE_COUNT;
}

IPOutputCombo *
IPOutputCombo::clone() const
{
  return new IPOutputCombo();
}

int
IPOutputCombo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "color (PaintTee)", &_color,
		  cpIPAddress, "dest IP address", &_my_ip,
		  cpUnsigned, "MTU (IPFragmenter)", &_mtu,
		  0) < 0)
    return -1;
  return 0;
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
  click_ip *ip = p->ip_header();
  assert(ip);
  unsigned hlen = (ip->ip_hl << 2);

  if(hlen > sizeof(click_ip)){
    u_char *oa = (u_char *) (ip + 1);
    int olen = hlen - sizeof(click_ip);
    int oi;
  
    for (oi = 0; oi < olen; ) {
      u_int type = oa[oi];
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
          goto ipgw_send_error;
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
          goto ipgw_send_error;
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
        } else if(flg == 3){
          /* only if it's my address */
          if(p+8 <= xlen && memcmp(oa + oi + p, &_my_ip, 4) == 0){
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
    ip->ip_sum = in_cksum(p->data(), hlen);
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
  SET_ICMP_PARAM_PROB_ANNO(p, problem_offset);
  output(2).push(p);
}

EXPORT_ELEMENT(IPOutputCombo)
ELEMENT_MT_SAFE(IPOutputCombo)
