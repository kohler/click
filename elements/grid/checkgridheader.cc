/*
 * checkgridheader.{cc,hh} -- element checks Grid header for correctness
 * (checksums, lengths)
 * Douglas S. J. De Couto
 * from checkipheader.{cc,hh} by Robert Morris
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
  tlen = gh->total_len;
  /* grid header size keeps changing
  if(hlen < sizeof(grid_hdr))
    goto bad;
  */
  
  if (tlen > p->length())
    goto bad;

  if (in_cksum((unsigned char *) gh, tlen) != 0)
    goto bad;
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

EXPORT_ELEMENT(CheckGridHeader)
