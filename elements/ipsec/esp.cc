/*
 * esp.{cc,hh} -- element implements IPsec encapsulation (RFC 2406)
 * Alex Snoeren
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
#ifndef HAVE_IPSEC
# error "Must #define HAVE_IPSEC in config.h"
#endif
#include "esp.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/click_ip.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/sha1.hh>

IPsecESPEncap::IPsecESPEncap()
  : Element(1, 1), _spi(-1)
{
}

IPsecESPEncap::IPsecESPEncap(int spi, int blks)
{
  add_input();
  add_output();
  _spi = spi;
  _blks = blks;

}

IPsecESPEncap::~IPsecESPEncap()
{
}

IPsecESPEncap *
IPsecESPEncap::clone() const
{
  return new IPsecESPEncap(_spi, _blks);
}

int
IPsecESPEncap::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _sha1 = false;
  unsigned int spi_uc;
  int blk_int;

  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "Security Parameter Index", &spi_uc,
		  cpInteger, "Block size", &blk_int,
		  cpOptional,
		  cpBool, "Compute SHA1 hash?", &_sha1,
		  0) < 0)
    return -1;
  _spi = spi_uc;
  _blks = blk_int;
  return 0;
}

int
IPsecESPEncap::initialize(ErrorHandler *errh)
{
  if (_spi < 0)
    return errh->error("not configured");
  _rpl = 0;
  return 0;
}


Packet *
IPsecESPEncap::simple_action(Packet *p)
{
  int i;

  // extract protocol header
  const click_ip *ip = p->ip_header();
  u_char ip_p = ip->ip_p;
  
  // make room for ESP header and padding
  int plen = p->length();
  int padding = ((_blks - ((plen + 2) % _blks)) % _blks) + 2;
  
  WritablePacket *q = p->push(sizeof(esp_new));
  q = q->put(padding);

  struct esp_new *esp = (struct esp_new *) q->data();  
  u_char *pad = ((u_char *) q->data()) + sizeof(esp_new) + plen;

  // copy in ESP header
  esp->esp_spi = htonl(_spi);
  _rpl++;
  int rpl = _rpl;
  esp->esp_rpl = htonl(rpl);
  memcpy(q->data(), esp, sizeof(struct esp_new));

  // default padding specified by RFC 2406
  for (i = 0; i < padding - 2; i++)
    pad[i] = i + 1;
  pad[padding - 2] = padding - 2;
  
  // next header = ip protocol number
  pad[padding - 1] = ip_p;

  // compute sha1
  if (_sha1) {
    u_char *ah = ((u_char*) q->data()) + q->length(); 
    q = q->put(12);
    SHA1_ctx ctx;
    SHA1_init (&ctx);
    SHA1_update (&ctx, 
	         ((u_char*) q->data())+sizeof(esp_new), 
		 q->length()-12-sizeof(esp_new));
    SHA1_final (&ctx);
    const unsigned char *digest = SHA1_digest(&ctx);
    memmove(ah, digest, 12);
  }

  return(q);
}

EXPORT_ELEMENT(IPsecESPEncap)
ELEMENT_MT_SAFE(IPsecESPEncap)
