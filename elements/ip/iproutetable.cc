/*
 * iproutetable.{cc,hh} -- looks up next-hop address in route table
 * Benjie Chen
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, subject to the conditions listed in the Click LICENSE
 * file. These conditions include: you must preserve this copyright
 * notice, and you cannot mention the copyright holders in advertising
 * related to the Software without their permission.  The Software is
 * provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This notice is a
 * summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "iproutetable.hh"

IPRouteTable::IPRouteTable()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}


IPRouteTable::~IPRouteTable()
{
  MOD_DEC_USE_COUNT;
}

int
IPRouteTable::ctrl_handler
(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  unsigned int dst, mask, gw=0;
  int output_num;
  bool ok = false;
  IPRouteTable *r = reinterpret_cast<IPRouteTable*>(e);

  Vector<String> words;
  cp_spacevec(conf, words);

  if (words[0] == "add") {
    if ((words.size() == 3 || words.size() == 4)
	&& cp_ip_prefix(words[1], (unsigned char *)&dst, 
	                          (unsigned char *)&mask, true, r)
	&& cp_integer(words.back(), &output_num)) {
      if (words.size() == 4)
	ok = cp_ip_address(words[2], (unsigned char *)&gw, r);
      else
	ok = true;
    }

    if (ok && output_num >= 0 && output_num < r->noutputs())
      r->add_route(dst, mask, gw, output_num);
    else if (output_num < 0 || output_num >= r->noutputs())
      return 
	errh->error("output number must be between 0 and %d", r->noutputs());
    else
      return 
	errh->error("add arguments should be `daddr/mask [gateway] output'");
  }
  else if (words[0] == "remove") {
    if (words.size() == 2 && 
	cp_ip_prefix
	(words[1], (unsigned char *)&dst, (unsigned char *)&mask, true, r))
      r->remove_route(dst, mask);
  }
  else
    return errh->error("command must be add or remove");
  return 0;
}

String
IPRouteTable::look_handler(Element *e, void *)
{
  IPRouteTable *r = reinterpret_cast<IPRouteTable*>(e);
  return r->dump_routes();
}

void
IPRouteTable::push(int, Packet *p)
{
  IPAddress a = p->dst_ip_anno();
  IPAddress gw;
  int port = -1;

  if ((port = lookup_route(a, gw)) >= 0) {
    if (gw)
      p->set_dst_ip_anno(gw);
    output(port).push(p);
  } else {
    click_chatter("IPRouteTable: no route for %x", a.addr());
    p->kill();
  }
}

void
IPRouteTable::add_handlers()
{
  add_write_handler("ctrl", ctrl_handler, 0);
  add_read_handler("look", look_handler, 0);
}

EXPORT_ELEMENT(IPRouteTable)

