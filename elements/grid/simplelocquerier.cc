/*
 * simplelocquerier.{cc,hh} -- Simple Grid location query element
 * Douglas S. J. De Couto
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
#include "simplelocquerier.hh"
#include <click/click_ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>

SimpleLocQuerier::SimpleLocQuerier()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SimpleLocQuerier::~SimpleLocQuerier()
{
  MOD_DEC_USE_COUNT;
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
			  cpReal10, "latitude", 7, &ilat,
			  cpReal10, "longitude", 7, &ilon,
			  0) < 0)
      return -1;
    grid_location loc((double) ilat /  1.0e7, (double) ilon /  1.0e7);
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
			cpReal10, "latitude", 7, &ilat,
			cpReal10, "longitude", 7, &ilon,
			0) < 0)
    return -1;
  grid_location loc((double) ilat /  1.0e7, (double) ilon /  1.0e7);
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

#include <click/bighashmap.cc>
template class BigHashMap<IPAddress, grid_location>;
