/*
 * ripsend.{cc,hh} -- element advertises routes using RIP protocol
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ripsend.hh"
#include "confparse.hh"
#include <string.h>
#include "click_ip.h"
#include "click_udp.h"

RIPSend::RIPSend()
  : _timer(this)
{
  add_output();
}

int
RIPSend::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int ret = cp_va_parse(conf, this, errh,
                        cpIPAddress, "source addr", &_src,
                        cpIPAddress, "dst addr", &_dst,
                        cpIPAddressMask, "advertised address", &_what, &_mask,
                        cpInteger, "metric", &_metric,
                        0);
  if(ret < 0)
    return(ret);

  return(0);
}

int
RIPSend::initialize(ErrorHandler *)
{
  _timer.attach(this);
  _timer.schedule_after_ms(3 * 1000);
  return 0;
}

void
RIPSend::run_scheduled()
{
  click_ip *ipp;
  click_udp *udpp;
  Packet *p = Packet::make(sizeof(*ipp) + sizeof(*udpp) + 24);

  memset(p->data(), '\0', p->length());
  
  /* for now just pseudo-header fields for UDP checksum */
  ipp = (click_ip *) p->data();
  ipp->ip_len = htons(p->length() - sizeof(*ipp));
  ipp->ip_p = IPPROTO_UDP;
  ipp->ip_src = _src.in_addr();
  ipp->ip_dst = _dst.in_addr();

  /* RIP payload */
  udpp = (click_udp *) (ipp + 1);
  unsigned int *r = (unsigned int *) (udpp + 1);
  r[0] = htonl((2 << 24) | (2 << 16) | 0);
  r[1] = htonl((2 << 16) | 0);
  r[2] = _what.addr();
  r[3] = _mask.addr();
  r[4] = _src.addr();
  r[5] = htonl(_metric);

  /* UDP header */
  udpp->uh_sport = htons(520);
  udpp->uh_dport = htons(520);
  udpp->uh_ulen = htons(p->length() - sizeof(*ipp));
  udpp->uh_sum = in_cksum(p->data(), p->length());

  /* the remaining IP header fields */
  ipp->ip_len = htons(p->length());
  ipp->ip_hl = sizeof(click_ip) >> 2;
  ipp->ip_v = IPVERSION;
  ipp->ip_ttl = 200;
  ipp->ip_sum = in_cksum((unsigned char *) ipp, sizeof(*ipp));
  
  p->set_ip_header(ipp, sizeof(click_ip));
  
  output(0).push(p);

  _timer.schedule_after_ms(30 * 1000);
}

EXPORT_ELEMENT(RIPSend)
