/*
 * LinkTable.{cc,hh} -- Routing Table in terms of links
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "linktable.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
CLICK_DECLS

#ifndef lt_assert
#define lt_assert(e) ((e) ? (void) 0 : lt_assert_(__FILE__, __LINE__, #e))
#endif /* dsr_assert */

LinkTable::LinkTable() 
{
  MOD_INC_USE_COUNT;
}



LinkTable::~LinkTable() 
{
  MOD_DEC_USE_COUNT;
}

int
LinkTable::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
                    cpIPAddress, "IP address", &_ip,
                    cpKeywords,
                    0);
  return ret;
}



void
LinkTable::add_handlers() {
  add_default_handlers(false);
  add_read_handler("routes", static_print_routes, 0);
  add_read_handler("links", static_print_links, 0);
  add_read_handler("hosts", static_print_hosts, 0);
  add_write_handler("clear", static_clear, 0);
}


int
LinkTable::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  LinkTable *n = (LinkTable *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`frozen' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
LinkTable::clear() 
{
  _hosts.clear();
  _links.clear();

}
void 
LinkTable::update_link(IPPair p, u_short metric, timeval now) 
{
  /* make sure both the hosts exist */
  HostInfo *hnfo_a = _hosts.findp(p._a);
  if (!hnfo_a) {
    HostInfo foo = HostInfo(p._a);
    _hosts.insert(p._a, foo);
    hnfo_a = _hosts.findp(p._a);
  }
  HostInfo *hnfo_b = _hosts.findp(p._b);
  if (!hnfo_b) {
    _hosts.insert(p._b, HostInfo(p._b));
    hnfo_b = _hosts.findp(p._b);
  }
  
  hnfo_a->_neighbors.insert(p._b, p._b);
  hnfo_b->_neighbors.insert(p._a, p._a);

  LinkInfo *lnfo = _links.findp(p);
  if (!lnfo) {
    _links.insert(p, LinkInfo(p, metric, now));
  } else {
    lnfo->update(metric, now);
  }
  
}



u_short 
LinkTable::get_hop_metric(IPPair p) 
{
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 9999;
  }
  return nfo->_metric;
}
  
  
  
u_short 
LinkTable::get_route_metric(Vector<IPAddress> route, int size) 
{
  u_short metric = 0;
  for (int i = 0; i < size - 1; i++) {
    metric += get_hop_metric(IPPair(route[i], route[i+1]));
  }
  return metric;
  
}


Vector<IPAddress> 
LinkTable::best_route(IPAddress dst)
{
  Vector<IPAddress> reverse_route;
  Vector<IPAddress> route;
  HostInfo *nfo = _hosts.findp(dst);
  
  while (nfo && nfo->_metric != 9999 && nfo->_metric != 0) {
    reverse_route.push_back(nfo->_ip);
    nfo = _hosts.findp(nfo->_prev);
  }
  if (nfo && nfo->_metric == 0) {
    reverse_route.push_back(nfo->_ip);
  }

  /* why isn't there just push? */
  for (int i=reverse_route.size() - 1; i >= 0; i--) {
    route.push_back(reverse_route[i]);
  }

  return route;
}

IPAddress
LinkTable::extract_min()
{

  IPAddress min = IPAddress(0);
  u_short min_metric = 9999;
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo nfo = iter.value();
    if (!nfo._marked && nfo._metric < min_metric) {
      min = nfo._ip;
      min_metric = nfo._metric;
    }
  }
  return min;
}

String
LinkTable::static_print_links(Element *e, void *)
{
  LinkTable *n = (LinkTable *) e;
  return n->print_links();
}

String 
LinkTable::print_links() 
{
  StringAccum sa;
  for (LTIter iter = _links.begin(); iter; iter++) {
    LinkInfo n = iter.value();
    sa << n._p._a.s().cc() << " " << n._p._b.s().cc() << " : ";
    sa << n._metric << " " << " " << n._last_updated << "\n";
  }
  return sa.take_string();
}



String
LinkTable::static_print_routes(Element *e, void *)
{
  LinkTable *n = (LinkTable *) e;
  return n->print_routes();
}

String 
LinkTable::print_routes() 
{
  StringAccum sa;
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();
    Vector <IPAddress> r = best_route(n._ip);
    sa << n._ip.s().cc() << " : ";
    for (int i = 0; i < r.size(); i++) {
      sa << " " << r[i] << " ";
      if (i != r.size()-1) {
	LinkInfo *l = _links.findp(IPPair(r[i], r[i+1]));
	lt_assert(l);
	sa << "<" << l->_metric << ">";
      }
    }
    sa << "\n";
  }
  return sa.take_string();
}


String
LinkTable::static_print_hosts(Element *e, void *)
{
  LinkTable *n = (LinkTable *) e;
  return n->print_links();
}

String 
LinkTable::print_hosts() 
{
  StringAccum sa;
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();
    sa << n._ip.s().cc() << "\n";
  }
  return sa.take_string();
}

void
LinkTable::dijkstra(IPAddress src) 
{
  /* clear them all initially */
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();
    n.clear();
    n._metric = 9999;
    n._prev = IPAddress(0);
  }
  
  HostInfo *root_info = _hosts.findp(src);


  lt_assert(root_info);
  
  root_info->_prev = root_info->_ip;
  root_info->_metric = 0;
  IPAddress current_min_ip = root_info->_ip;

  while (current_min_ip != IPAddress(0)) {
    HostInfo *current_min = _hosts.findp(current_min_ip);

    lt_assert(current_min);

    current_min->_marked = true;


    for (IPTable::iterator iter = current_min->_neighbors.begin(); iter; iter++) {
      HostInfo *neighbor = _hosts.findp(iter.value());
      lt_assert(neighbor);
      if (!neighbor->_marked) {
	LinkInfo *lnfo = _links.findp(IPPair(current_min->_ip, neighbor->_ip));
	lt_assert(lnfo);

	if (neighbor->_metric > current_min->_metric + lnfo->_metric) {
	  neighbor->_metric = current_min->_metric + lnfo->_metric;
	  neighbor->_prev = current_min_ip;
	} 
      }
    }

    current_min_ip = extract_min();
  }
  
}


void
LinkTable::lt_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("LinkTable assertion \"%s\" failed: file %s, line %d",
		expr, file, line);
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif

}

#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<IPAddress, IPAddress>;
template class BigHashMap<IPPair, LinkTable::LinkInfo>;
template class BigHashMap<IPAddress, LinkTable::HostInfo>;
#endif
EXPORT_ELEMENT(LinkTable)
CLICK_ENDDECLS
