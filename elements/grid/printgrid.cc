/*
 * print.{cc,hh} -- print Grid packets, for debugging.
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
#include "printgrid.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"

#include "etheraddress.hh"
#include "ipaddress.hh"
#include "click_ether.h"
#include "grid.hh"

PrintGrid::PrintGrid()
  : Element(1, 1)
{
  _label = "";
}

PrintGrid::~PrintGrid()
{
}

PrintGrid *
PrintGrid::clone() const
{
  return new PrintGrid;
}

int
PrintGrid::configure(const Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpOptional,
		  cpString, "label", &_label,
		  cpEnd) < 0)
    return -1;
  return(0);
}

Packet *
PrintGrid::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    return p;
  }
  EtherAddress deth = EtherAddress(eh->ether_dhost);
  EtherAddress seth = EtherAddress(eh->ether_shost);

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  IPAddress xip = IPAddress((unsigned) gh->ip);

  char *type = "?";
  if(gh->type == GRID_HELLO)
    type = "HELLO";
  else if(gh->type == GRID_NBR_ENCAP)
    type = "ENCAP";

  click_chatter("PrintGrid%s%s : %s %s %s %s %.4f %.4f",
                _label.cc()[0] ? " " : "",
                _label.cc(),
                seth.s().cc(),
                deth.s().cc(),
                type,
                xip.s().cc(),
                gh->loc.lat(),
                gh->loc.lon());

  return p;
}

EXPORT_ELEMENT(PrintGrid)
