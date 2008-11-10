/*
 * esp.{cc,hh} -- element implements IPsec encapsulation (RFC 2406)
 * Alex Snoeren, Benjie Chen
 *
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Added Security Association Database support. Dimitris Syrivelis <jsyr@inf.uth.gr>, University of Thessaly, *
 * Hellas
 *
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
#include <clicknet/ip.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include "satable.hh"
#include "sadatatuple.hh"
CLICK_DECLS

IPsecESPEncap::IPsecESPEncap()
{
}

IPsecESPEncap::~IPsecESPEncap()
{
}

int
IPsecESPEncap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

Packet *
IPsecESPEncap::simple_action(Packet *p)
{
  int i;
  SADataTuple * sa_data;
  u_char ip_p=0;

  // extract protocol header
  if (p->has_network_header())
      ip_p = p->ip_header()->ip_p;
  sa_data=(SADataTuple *)IPSEC_SA_DATA_REFERENCE_ANNO(p);

  // make room for ESP header and padding
  int plen = p->length();
  int padding = ((BLKS - ((plen + 2) % BLKS)) % BLKS) + 2;

  WritablePacket *q = p->push(sizeof(esp_new));
  q = q->put(padding);

  struct esp_new *esp = (struct esp_new *) q->data();
  u_char *pad = ((u_char *) q->data()) + sizeof(esp_new) + plen;

  // copy in ESP header
  // Get SPI from packet user annotation. This is the fourth user integer.
  esp->esp_spi = htonl((uint32_t)IPSEC_SPI_ANNO(p));
  esp->esp_rpl = htonl(sa_data->cur_rpl);

  if((sa_data->cur_rpl++) == 0) {
	//if the replay counter rolls over...set it to the agreed start value
        sa_data->cur_rpl = sa_data->replay_start_counter;
  }
  i = click_random() >> 2;
  memmove(&esp->esp_iv[0], &i, 4);
  i = click_random() >> 2;
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



CLICK_ENDDECLS
EXPORT_ELEMENT(IPsecESPEncap)
ELEMENT_MT_SAFE(IPsecESPEncap)
