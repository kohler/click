/*
 * checkgridheader.{cc,hh} -- element checks Grid header for correctness
 * (checksums, lengths)
 * Douglas S. J. De Couto
 * from checkipheader.{cc,hh} by Robert Morris
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
#include "checkgridheader.hh"
#include "glue.hh"
#include "grid.hh"
#include "click_ether.h"
#include "click_ip.h"

CheckGridHeader::CheckGridHeader()
  : _drops(0)
{
  add_input();
  add_output();
}

CheckGridHeader::~CheckGridHeader()
{
}

CheckGridHeader *
CheckGridHeader::clone() const
{
  return new CheckGridHeader();
}

void
CheckGridHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}


void
CheckGridHeader::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("Grid checksum failed");
  _drops++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

Packet *
CheckGridHeader::simple_action(Packet *p)
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));

  if(p->length() < sizeof(click_ether) + sizeof(grid_hdr))
    goto bad;
  
  unsigned int hlen, tlen;
  hlen = gh->hdr_len;
  tlen = ntohs(gh->total_len);

  /* grid header size keeps changing
  if(hlen < sizeof(grid_hdr))
    goto bad;
  */
  
  if (tlen + sizeof(click_ether) > p->length()) { 
    /* can only check inequality, as short packets are padded to a
       minimum frame size for wavelan and ethernet */
#if 0
    click_chatter("%s: bad packet size", id().cc());
#endif
    goto bad;
  }

  if (in_cksum((unsigned char *) gh, tlen) != 0) {
#if 0
    click_chatter("%s: bad Grid checksum", id().cc());
#endif
    goto bad;
  }
  return(p);
  
 bad:
  drop_it(p);
  return 0;
}

static String
CheckGridHeader_read_drops(Element *xf, void *)
{
  CheckGridHeader *f = (CheckGridHeader *)xf;
  return String(f->drops()) + "\n";
}

void
CheckGridHeader::add_handlers()
{
  add_read_handler("drops", CheckGridHeader_read_drops, 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(CheckGridHeader)
