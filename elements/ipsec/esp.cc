/*
 * esp.{cc,hh} -- element implements IPsec encapsulation (RFC 2406)
 * Alex Snoeren, Benjie Chen
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
#ifndef HAVE_IPSEC
# error "Must #define HAVE_IPSEC in config.h"
#endif
#include "esp.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/click_ip.h>
#include <click/error.hh>
#include <click/glue.hh>

IPsecESPEncap::IPsecESPEncap()
  : Element(1, 1), _spi(-1)
{
}

IPsecESPEncap::IPsecESPEncap(int spi)
{
  add_input();
  add_output();
  _spi = spi;
}

IPsecESPEncap::~IPsecESPEncap()
{
}

IPsecESPEncap *
IPsecESPEncap::clone() const
{
  return new IPsecESPEncap(_spi);
}

int
IPsecESPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  unsigned int spi_uc;

  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "Security Parameter Index", &spi_uc, 0) < 0)
    return -1;
  _spi = spi_uc;
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
  int padding = ((BLKS - ((plen + 2) % BLKS)) % BLKS) + 2;
  
  WritablePacket *q = p->push(sizeof(esp_new));
  q = q->put(padding);

  struct esp_new *esp = (struct esp_new *) q->data();  
  u_char *pad = ((u_char *) q->data()) + sizeof(esp_new) + plen;

  // copy in ESP header
  esp->esp_spi = htonl(_spi);
  _rpl++;
  int rpl = _rpl;
  esp->esp_rpl = htonl(rpl);
  i = random() >> 2;
  memmove(&esp->esp_iv[0], &i, 4);
  i = random() >> 2;
  memmove(&esp->esp_iv[4], &i, 4);
  memmove(q->data(), esp, sizeof(struct esp_new));

  // default padding specified by RFC 2406
  for (i = 0; i < padding - 2; i++)
    pad[i] = i + 1;
  pad[padding - 2] = padding - 2;
  
  // next header = ip protocol number
  pad[padding - 1] = ip_p;

  return(q);
}

EXPORT_ELEMENT(IPsecESPEncap)
ELEMENT_MT_SAFE(IPsecESPEncap)
