/*
 * printgrid.{cc,hh} -- print Grid packets, for debugging.
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

  char *buf = 0;

  if(gh->type == GRID_HELLO){
    grid_hello *h = (grid_hello *) (gh + 1);
    grid_nbr_entry *na = (grid_nbr_entry *) (h + 1);
    buf = new char [h->num_nbrs * 100 + 1];
    assert(buf);
    buf[0] = '\0';
    int i;
    for(i = 0; i < h->num_nbrs; i++){
      char tmp[100];
      sprintf(tmp, "%s %s %d %f %f ",
              IPAddress(na[i].ip).s().cc(),
              IPAddress(na[i].next_hop_ip).s().cc(),
              na[i].num_hops,
              na[i].loc.lat(),
              na[i].loc.lon());
      strcat(buf, tmp);
    }
  }

  click_chatter("PrintGrid%s%s : %s %s %s %.4f %.4f %s",
                _label.cc()[0] ? " " : "",
                _label.cc(),
                seth.s().cc(),
                type,
                xip.s().cc(),
                gh->loc.lat(),
                gh->loc.lon(),
                buf ? buf : "");
  
  if(buf)
    delete buf;

  return p;
}

EXPORT_ELEMENT(PrintGrid)
