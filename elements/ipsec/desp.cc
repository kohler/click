/*
 * desp.{cc,hh} -- element implements IPsec unencapsulation (RFC 2406)
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
#include "desp.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

IPsecESPUnencap::IPsecESPUnencap()
  : Element(1, 1)
{
}

IPsecESPUnencap::~IPsecESPUnencap()
{
}

IPsecESPUnencap *
IPsecESPUnencap::clone() const
{
  return new IPsecESPUnencap();
}

int
IPsecESPUnencap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh, 0) < 0)
    return -1;
  return 0;
}


Packet *
IPsecESPUnencap::simple_action(Packet *p)
{

  int i, blks;
  const unsigned char * blk;

  // rip off ESP header
  p->pull(sizeof(esp_new));

  // verify padding
  blks = p->length();
  blk = p->data();
  if((blk[blks - 2] != blk[blks - 3]) && (blk[blks -2] != 0)) {
    click_chatter("Invalid padding length");
    p->kill();
    return(0);
  }
  blks = blk[blks - 2];
  blk = p->data() + p->length() - (blks + 2);
  for(i = 0; (i < blks) && (blk[i] == ++i););    
  if(i<blks) {
    click_chatter("Corrupt padding");
    p->kill();
    return(0);
  }
  
  // chop off padding
  p->take(blks+2);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPsecESPUnencap)
ELEMENT_MT_SAFE(IPsecESPUnencap)
