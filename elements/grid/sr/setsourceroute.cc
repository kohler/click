/*
 * SetSourceRoute.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussetsourceroutes Institute of Technology
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <elements/grid/linktable.hh>
#include <elements/grid/arptable.hh>
#include <elements/grid/sr/path.hh>
#include <elements/grid/sr/srcrstat.hh>
#include "setsourceroute.hh"
#include "srforwarder.hh"

CLICK_DECLS

#ifndef setsourceroute_assert
#define setsourceroute_assert(e) ((e) ? (void) 0 : setsourceroute_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


SetSourceRoute::SetSourceRoute()
  :  Element(1,1),
     _sr_forwarder(0)
{
  MOD_INC_USE_COUNT;
}

SetSourceRoute::~SetSourceRoute()
{
  MOD_DEC_USE_COUNT;
}

int
SetSourceRoute::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "SR", cpElement, "SRForwarder element", &_sr_forwarder,
                    0);

  if (!_sr_forwarder || _sr_forwarder->cast("SRForwarder") == 0) 
    return errh->error("SRForwarder element is not a SRForwarder or not specified");
  if (!_ip) 
    return errh->error("IP Address must be specified");

  return ret;
}

SetSourceRoute *
SetSourceRoute::clone () const
{
  return new SetSourceRoute;
}

int
SetSourceRoute::initialize (ErrorHandler *)
{
  return 0;
}

Packet *
SetSourceRoute::simple_action(Packet *p_in)
{

  
  IPAddress dst = p_in->dst_ip_anno();

  if (!dst) {
    click_chatter("SetSourceRoute %s: got invalid dst %s\n",
		  id().cc(),
		  dst.s().cc());
    p_in->kill();
    return 0;
  }

  Path *p = _routes.findp(dst);
  if (!p) {
    click_chatter("SetSourceRoute %s: couldn't find path for dst %s\n",
		  id().cc(),
		  dst.s().cc());
    p_in->kill();
    return 0;
  }

  Packet *p_out = _sr_forwarder->encap(p_in->data(), p_in->length(), *p, 0);
  p_in->kill();
  return p_out;
}


int
SetSourceRoute::static_set_route(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  SetSourceRoute *n = (SetSourceRoute *) e;
  Vector<String> args;
  Path p;

  cp_spacevec(arg, args);
  for (int x = 0; x < args.size(); x++) {
    IPAddress ip;
    if (!cp_ip_address(args[x], &ip)) {
      return errh->error("Couldn't read arg %d to ip: %s",
			 x,
			 args[x].cc());
    }
    p.push_back(ip);
  }
  if (p[0] != n->_ip) {
    return errh->error("First hop %s doesn't match my ip %s",
		       p[0].s().cc(),
		       n->_ip.s().cc());
  }
  n->set_route(p);
  return 0;
}

void
SetSourceRoute::set_route(Path p) 
{
  if (p.size() < 1) {
    click_chatter("SetSourceRoute %s: Path must be longer than 0\n",
		  id().cc());
  }
  if (p[0] != _ip) {
    click_chatter("SetSourceRoute %s: First node must be me (%s) not %s!\n",
		  id().cc(),
		  _ip.s().cc(),
		  p[0].s().cc());
  }

  _routes.insert(p[p.size()-1], p);
  
}
String
SetSourceRoute::static_print_routes(Element *f, void *)
{
  SetSourceRoute *d = (SetSourceRoute *) f;
  return d->print_routes();
}

String
SetSourceRoute::print_routes()
{
  StringAccum sa;
  for (RouteTable::iterator iter = _routes.begin(); iter; iter++) {
    IPAddress dst = iter.key();
    Path p = iter.value();
    sa << dst << " : " << path_to_string(p) << "\n";
  }
  return sa.take_string();
}

int
SetSourceRoute::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  SetSourceRoute *n = (SetSourceRoute *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
SetSourceRoute::clear() 
{
  _routes.clear();
}

void
SetSourceRoute::add_handlers()
{
  add_read_handler("print_routes", static_print_routes, 0);
  add_write_handler("clear", static_clear, 0);
  add_write_handler("set_route", static_set_route, 0);
}

void
SetSourceRoute::setsourceroute_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("SetSourceRoute %s assertion \"%s\" failed: file %s, line %d",
		id().cc(), expr, file, line);
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif

}

// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<IPAddress, Path>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SetSourceRoute)
