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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef HAVE_IPSEC
# error "Must #define HAVE_IPSEC in config.h"
#endif
#include "esp.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/click_ip.h>
#include <click/error.hh>
#include <click/glue.hh>

Esp::Esp()
  : Element(1, 1), _spi(-1)
{
}

Esp::Esp(int spi, int blks)
{
  add_input();
  add_output();
  _spi = spi;
  _blks = blks;

}

Esp::~Esp()
{
}

Esp *
Esp::clone() const
{
  return new Esp(_spi, _blks);
}

int
Esp::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned int spi_uc;
  int blk_int;

  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "Security Parameter Index", &spi_uc,
		  cpInteger, "Block size", &blk_int,
		  0) < 0)
    return -1;
  _spi = spi_uc;
  _blks = blk_int;
  return 0;
}

int
Esp::initialize(ErrorHandler *errh)
{
  if (_spi < 0)
    return errh->error("not configured");
  _rpl = 0;
  return 0;
}


Packet *
Esp::simple_action(Packet *p)
{
  int i;

  // Extract Protocol Header 
  const click_ip *ip = p->ip_header();
  u_char ip_p = ip->ip_p;
  
  // Make room for ESP header and padding
  int plen = p->length();
  int padding = ((_blks - ((plen + 2) % _blks)) % _blks) + 2;
  
  // WritablePacket *q = Packet::make(sizeof(esp_new) + plen + padding);
  // // Copy data
  // memcpy((q->data() + sizeof(esp_new)), p->data(), plen);
  // p->kill();

  WritablePacket *q = p->push(sizeof(esp_new));
  q = q->put(padding);
  
  struct esp_new *esp = (struct esp_new *) q->data();  
  u_char *pad = ((u_char *) q->data()) + sizeof(esp_new) + plen;

  // Copy in ESP header
  esp->esp_spi = htonl(_spi);
  _rpl++;
  int rpl = _rpl;
  esp->esp_rpl = htonl(rpl);
  memcpy(q->data(), esp, sizeof(struct esp_new));

  // Self describing padding
  for (i = 0; i < padding - 2; i++)
    pad[i] = i + 1;
  pad[padding - 2] = padding - 2;
  // Next header 
  pad[padding - 1] = ip_p;

  return(q);
}

EXPORT_ELEMENT(Esp)
ELEMENT_MT_SAFE(Esp)

