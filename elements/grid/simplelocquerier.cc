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
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

SimpleLocQuerier::SimpleLocQuerier()
{
}

SimpleLocQuerier::~SimpleLocQuerier()
{
}


int
SimpleLocQuerier::configure(Vector<String> &conf, ErrorHandler *errh)
{
  for (int i = 0; i < conf.size(); i++) {
    IPAddress ip;
    int ilat, ilon;
    if (Args(this, errh).push_back_words(conf[i])
	.read_mp("IP", ip)
	.read_mp("LATITUDE", DecimalFixedPointArg(7), ilat)
	.read_mp("LONGITUDE", DecimalFixedPointArg(7), ilon)
	.complete() < 0)
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
SimpleLocQuerier::push(int, Packet *p)
{
  WritablePacket *wp = p->uniqueify();
  grid_nbr_encap *nb = (grid_nbr_encap *) (p->data() + sizeof(grid_hdr) + sizeof(click_ether));
  IPAddress dst_ip(nb->dst_ip);
  grid_location *l = _locs.findp(dst_ip);
  if (l == 0) {
    click_chatter("SimpleLocQuerier %s: dropping packet for %s; there is no location information",
		  name().c_str(), dst_ip.unparse().c_str());
    wp->kill();
  }
  else {
#ifndef SMALL_GRID_HEADERS
    nb->dst_loc = *l;
#endif
    output(0).push(wp);
  }
}

String
SimpleLocQuerier::read_table(Element *e, void *)
{
  SimpleLocQuerier *slq = (SimpleLocQuerier *) e;
  String s;
  for (locmap::iterator i = slq->_locs.begin(); i.live(); i++)
    s += i.key().unparse() + " " + i.value().s() + "\n";
  return s;
}

int
SimpleLocQuerier::add_entry(const String &arg, Element *element,
			    void *, ErrorHandler *errh)
{
  SimpleLocQuerier *l = (SimpleLocQuerier *) element;

  IPAddress ip;
  int ilat, ilon;
  if (Args(l, errh).push_back_words(arg)
      .read_mp("IP", ip)
      .read_mp("LATITUDE", DecimalFixedPointArg(7), ilat)
      .read_mp("LONGITUDE", DecimalFixedPointArg(7), ilon)
      .complete() < 0)
    return -1;
  grid_location loc((double) ilat /  1.0e7, (double) ilon /  1.0e7);
  l->_locs.insert(ip, loc);

  return 0;
}

void
SimpleLocQuerier::add_handlers()
{
  add_read_handler("table", read_table, 0);
  add_write_handler("add", add_entry, 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SimpleLocQuerier)
CLICK_ENDDECLS
