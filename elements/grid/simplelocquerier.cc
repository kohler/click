/*
 * simplelocquerier.{cc,hh} -- Simple Grid location query element
 * Douglas S. J. De Couto
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
#include "simplelocquerier.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "bitvector.hh"
#include "error.hh"
#include "glue.hh"

SimpleLocQuerier::SimpleLocQuerier()
  : Element(1, 1)
{
}

SimpleLocQuerier::~SimpleLocQuerier()
{
  uninitialize();
}

SimpleLocQuerier *
SimpleLocQuerier::clone() const
{
  return new SimpleLocQuerier;
}


int
SimpleLocQuerier::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  for (int i = 0; i < conf.size(); i++) {
    IPAddress ip;
    int ilat, ilon;
    if (cp_va_space_parse(conf[i], this, errh,
			  cpIPAddress, "IP address", &ip,
			  cpReal, "latitude", 7, &ilat,
			  cpReal, "longitude", 7, &ilon,
			  0) < 0)
      return -1;
    grid_location loc;
    loc.set((double) ilat /  1.0e7, (double) ilon /  1.0e7);
    _locs.insert(ip, loc);
  }
  return 0;
}

int
SimpleLocQuerier::initialize(ErrorHandler *)
{
  return 0;
}

void
SimpleLocQuerier::uninitialize()
{
}




void
SimpleLocQuerier::push(int, Packet *p)
{
  WritablePacket *wp = p->uniqueify();
  grid_nbr_encap *nb = (grid_nbr_encap *) (p->data() + sizeof(grid_hdr) + sizeof(click_ether));
  IPAddress dst_ip(nb->dst_ip);
  grid_location *l = _locs.findp(dst_ip);
  if (l == 0) {
    click_chatter("SimpleLocQuerier %s: dropping packet for %s; there is no location information",
		  id().cc(), dst_ip.s().cc());
    wp->kill();
  }
  else {
    nb->dst_loc = *l;
    output(0).push(wp);
  }
}

String
SimpleLocQuerier::read_table(Element *e, void *)
{
  SimpleLocQuerier *slq = (SimpleLocQuerier *) e;
  String s;
  for (locmap::Iterator i = slq->_locs.first(); i; i++)
    s += i.key().s() + " " + i.value().s() + "\n";
  return s;
}

int
SimpleLocQuerier::add_entry(const String &arg, Element *element,
			    void *, ErrorHandler *errh)
{
  SimpleLocQuerier *l = (SimpleLocQuerier *) element;
  
  IPAddress ip;
  int ilat, ilon;
  if (cp_va_space_parse(arg, l, errh,
			cpIPAddress, "IP address", &ip,
			cpReal, "latitude", 7, &ilat,
			cpReal, "longitude", 7, &ilon,
			0) < 0)
    return -1;
  grid_location loc;
  loc.set((double) ilat /  1.0e7, (double) ilon /  1.0e7);
  l->_locs.insert(ip, loc);

  return 0;
}

void
SimpleLocQuerier::add_handlers()
{
  add_read_handler("table", read_table, (void *)0);
  add_write_handler("add", add_entry, (void *)0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SimpleLocQuerier)

#include "bighashmap.cc"
template class BigHashMap<IPAddress, grid_location>;
