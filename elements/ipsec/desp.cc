/*
 * desp.{cc,hh} -- element implements IPsec unencapsulation (RFC 2406)
 * Alex Snoeren
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
#ifndef HAVE_IPSEC
# error "Must #define HAVE_IPSEC in config.h"
#endif
#include "esp.hh"
#include "desp.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

DeEsp::DeEsp()
  : Element(1, 1)
{
}

DeEsp::~DeEsp()
{
}

DeEsp *
DeEsp::clone() const
{
  return new DeEsp();
}

int
DeEsp::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  0) < 0)
    return -1;
  return 0;
}


Packet *
DeEsp::simple_action(Packet *p)
{

  int i, blks;
  const unsigned char * blk;

  // Rip off ESP header
  p->pull(sizeof(esp_new));

  // Verify padding
  blks = p->length();
  click_chatter("got %d left", blks);
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

  // Chop off padding
  return Packet::make(p->data(), p->length() - (blks + 2));
}

ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(DeEsp)
