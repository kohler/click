/*
 * esp.{cc,hh} -- element implements IPsec encapsulation (RFC 2406)
 * Alex Snoeren
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef HAVE_IPSEC
# error "Must #define HAVE_IPSEC in config.h"
#endif
#include "esp.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

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
Esp::configure(const String &conf, ErrorHandler *errh)
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
  click_ip *ip = (click_ip *) p->data();
  u_char ip_p = ip->ip_p;
  
  // Make room for ESP header and padding
  int plen = p->length();
  int padding = ((_blks - ((plen + 2) % _blks)) % _blks) + 2;
  Packet *q = Packet::make(sizeof(esp_new) + plen + padding);
  
  // Copy data and packet annotations
  memcpy((q->data() + sizeof(esp_new)), p->data(), plen);
  (void) q->set_ip_ttl_anno(p->ip_ttl_anno());
  (void) q->set_ip_tos_anno(p->ip_tos_anno());
  (void) q->set_ip_off_anno(p->ip_off_anno());
  p->kill();
  
  struct esp_new *esp = (struct esp_new *) q->data();  
  u_char *pad = ((u_char *) q->data()) + sizeof(esp_new) + plen;

  // Copy in ESP header
  esp->esp_spi = htonl(_spi);
  esp->esp_rpl = htonl(++_rpl);
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
