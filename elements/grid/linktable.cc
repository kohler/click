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
  
  click_chatter("looking up route for %s", dst.s().cc());
  while (nfo && nfo->_metric != 9999 && nfo->_metric != 0) {
    click_chatter("pushing back %s", _ip.s().cc());
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
LinkTable::take_string() 
{
  StringAccum sa;

  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();
    sa << n._ip << " m="<< n._metric << " next=" << n._prev <<"\n";
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

    click_chatter("Selected current min: %s", current_min_ip.s().cc());

    for (IPTable::iterator iter = current_min->_neighbors.begin(); iter; iter++) {
      HostInfo *neighbor = _hosts.findp(iter.value());
      lt_assert(neighbor);
      if (!neighbor->_marked) {
	LinkInfo *lnfo = _links.findp(IPPair(current_min->_ip, neighbor->_ip));
	lt_assert(lnfo);
	click_chatter("neighbor metric = %d, current_min->metroc =%d, link metric = %d", 
		      neighbor->_metric, current_min->_metric, lnfo->_metric);

	if (neighbor->_metric > current_min->_metric + lnfo->_metric) {
	  click_chatter("LinkTable %s: updating metric on pair (%s,%s)", _ip.s().cc(), 
			current_min->_ip.s().cc(), neighbor->_ip.s().cc());
	  neighbor->_metric = current_min->_metric + lnfo->_metric;
	  neighbor->_prev = current_min_ip;
	} else {
	  click_chatter("LinkTable %s: didn't update pair (%s,%s)", _ip.s().cc(), 
			current_min->_ip.s().cc(), neighbor->_ip.s().cc());
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
