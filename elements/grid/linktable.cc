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
#define lt_assert(e) ((e) ? (void) 0 : _lt_assert_(__FILE__, __LINE__, #e))
#endif /* dsr_assert */

LinkTable::LinkTable() 
{
  MOD_INC_USE_COUNT;
}



LinkTable::~LinkTable() 
{
  MOD_DEC_USE_COUNT;
}

void *
LinkTable::cast(const char *n)
{
  if (strcmp(n, "LinkTable") == 0)
    return (LinkTable *) this;
  else
    return 0;
}
int
LinkTable::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
                    cpIP6Address, "IP address", &_ip,
                    cpKeywords,
                    0);
  
  _hosts.insert(_ip, HostInfo(_ip));
  return ret;
}



void
LinkTable::add_handlers() {
  add_default_handlers(false);
  add_read_handler("routes", static_print_routes, 0);
  add_read_handler("links", static_print_links, 0);
  add_read_handler("hosts", static_print_hosts, 0);
  add_read_handler("neighbors", static_print_neighbors, 0);
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
LinkTable::update_link(IP6Address from, IP6Address to, u_short metric, unsigned int now)
{
  /* make sure both the hosts exist */
  HostInfo *nfrom = _hosts.findp(from);
  if (!nfrom) {
    HostInfo foo = HostInfo(to);
    _hosts.insert(from, foo);
    nfrom = _hosts.findp(from);
  }
  HostInfo *nto = _hosts.findp(to);
  if (!nto) {
    _hosts.insert(to, HostInfo(to));
    nto = _hosts.findp(to);
  }
  
  lt_assert(nfrom);
  lt_assert(nto);

  nfrom->_neighbors.insert(from, to);

  IP6Pair p = IP6Pair(from, to);
  LinkInfo *lnfo = _links.findp(p);
  if (!lnfo) {
    _links.insert(p, LinkInfo(from, to, metric, now));
  } else {
    lnfo->update(metric, now);
  }
  
}

Vector<IP6Address> 
LinkTable::get_hosts()
{
  Vector<IP6Address> v;
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();
    v.push_back(n._ip);
  }
  return v;
}
u_short 
LinkTable::get_host_metric(IP6Address s)
{
  HostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    return 9999;
  }
  return nfo->_metric;
}

u_short 
LinkTable::get_hop_metric(IP6Address from, IP6Address to) 
{
  IP6Pair p = IP6Pair(from, to);
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 9999;
  }
  return nfo->_metric;
}
  
  
  
u_short 
LinkTable::get_route_metric(Vector<IP6Address> route) 
{
  u_short metric = 0;
  for (int i = 0; i < route.size() - 1; i++) {
    metric += get_hop_metric(route[i], route[i+1]);
  }
  return metric;
  
}


bool
LinkTable::valid_route(Vector<IP6Address> route) 
{
  if (route.size() < 1) {
    return false;
  }
  /* ensure the metrics are all valid */
  if (get_route_metric(route) >= 9999){
    return false;
  }

  /* ensure that a node appears no more than once */
  for (int x = 0; x < route.size(); x++) {
    for (int y = x + 1; y < route.size(); y++) {
      if (route[x] == route[y]) {
	return false;
      }
    }
  }

  return true;
}
Vector<IP6Address> 
LinkTable::best_route(IP6Address dst)
{
  Vector<IP6Address> reverse_route;
  Vector<IP6Address> route;
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
Vector<Vector<IP6Address> > 
LinkTable::update_routes(Vector<Vector<IP6Address> > routes, int size, Vector<IP6Address> route)
{
  int x = 0;
  int y = 0;
  if (!valid_route(route)) {
    return routes;
  }

  u_short route_m = get_route_metric(route);
  
  for (x = 0; x < size; x++) {
    if (!valid_route(routes[x])) {
      routes[x] = route;
      return routes;
    }
    u_short m = get_route_metric(routes[x]);
    if (route_m < m) {
      break;
    }
  }
  if (x == size) {
    /* we're not good enough */
    return routes;
  }
  for(y = size - 1; y > x; y--) {
    routes[y] = routes[y-1];
  }
  routes[x] = route;
  return routes;
}
Vector <Vector <IP6Address> >
LinkTable::top_n_routes(IP6Address dst, int n)
{
  Vector<Vector<IP6Address> > routes;
  {
    Vector<IP6Address> route;
    route.push_back(_ip);
    route.push_back(dst);
    update_routes(routes, n, route);
  }

  /* two hop */
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    Vector<IP6Address> route;

    HostInfo h = iter.value();
    route.push_back(_ip);
    route.push_back(h._ip);
    route.push_back(dst);
    update_routes(routes, n, route);
  }
  
  /* three hop */
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    for (HTIter iter2 = _hosts.begin(); iter; iter++) {
      Vector<IP6Address> route;
      
      HostInfo h = iter.value();
      HostInfo h2 = iter2.value();
      route.push_back(_ip);
      route.push_back(h._ip);
      route.push_back(h2._ip);
      route.push_back(dst);
      update_routes(routes, n, route);
    }
  }

  /* four hop */
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    for (HTIter iter2 = _hosts.begin(); iter; iter++) {
      for (HTIter iter3 = _hosts.begin(); iter; iter++) {
      Vector<IP6Address> route;
      
      HostInfo h = iter.value();
      HostInfo h2 = iter2.value();
      HostInfo h3 = iter3.value();
      route.push_back(_ip);
      route.push_back(h._ip);
      route.push_back(h2._ip);
      route.push_back(h3._ip);
      route.push_back(dst);
      update_routes(routes, n, route);
      }
    }
  }

  return routes;
}

String
LinkTable::static_print_neighbors(Element *e, void *)
{
  LinkTable *n = (LinkTable *) e;
  return n->print_neighbors();
}
String 
LinkTable::print_neighbors() 
{
  StringAccum sa;
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();

    sa << "neighbors: " << n._ip.s().cc() << ":";
    for (IPTable::iterator iter = n._neighbors.begin(); iter; iter++) {
      sa << " " << iter.value().s().cc() << " ";
    }
    sa << "\n";
  }
  return sa.take_string();
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
  unsigned int now = click_jiffies();
  for (LTIter iter = _links.begin(); iter; iter++) {
    LinkInfo n = iter.value();
    sa << "link: ";
    sa << n._from.s().cc() << " " << n._to.s().cc() << " : ";
    sa << n._metric << " " << " " << (now - n._last_updated)/100 << "\n";
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
    Vector <IP6Address> r = best_route(n._ip);
    sa << "route: " << n._ip.s().cc() << " : ";
    for (int i = 0; i < r.size(); i++) {
      sa << " " << r[i] << " ";
      if (i != r.size()-1) {
	LinkInfo *l = _links.findp(IP6Pair(r[i], r[i+1]));
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
  return n->print_hosts();
}

String 
LinkTable::print_hosts() 
{
  StringAccum sa;
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();
    sa << "host: " << n._ip.s().cc() << "\n";
  }
  return sa.take_string();
}

IP6Address
LinkTable::extract_min()
{

  IP6Address min = IP6Address(0);
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



void
LinkTable::dijkstra() 
{
  IP6Address src = _ip;

  /* clear them all initially */
  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();
    n.clear();
    _hosts.insert(n._ip, n);
  }
  
  HostInfo *root_info = _hosts.findp(src);


  lt_assert(root_info);
  
  root_info->_prev = root_info->_ip;
  root_info->_metric = 0;
  IP6Address current_min_ip = root_info->_ip;

  while (current_min_ip != IP6Address(0)) {
    HostInfo *current_min = _hosts.findp(current_min_ip);
    lt_assert(current_min);

    current_min->_marked = true;


    for (IPTable::iterator iter = current_min->_neighbors.begin(); iter; iter++) {
      HostInfo *neighbor = _hosts.findp(iter.value());
      lt_assert(neighbor);
      if (!neighbor->_marked) {
	LinkInfo *lnfo = _links.findp(IP6Pair(current_min->_ip, neighbor->_ip));
	lt_assert(lnfo);

	if (neighbor->_metric > current_min->_metric + lnfo->_metric) {
	  neighbor->_metric = current_min->_metric + lnfo->_metric;
	  neighbor->_prev = current_min_ip;
	} 
      } else {
      }
    }

    current_min_ip = extract_min();
  }
  
}


void
LinkTable::_lt_assert_(const char *file, int line, const char *expr)
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
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<IP6Address, IP6Address>;
template class BigHashMap<IP6Pair, LinkTable::LinkInfo>;
template class BigHashMap<IP6Address, LinkTable::HostInfo>;
#endif
EXPORT_ELEMENT(LinkTable)
CLICK_ENDDECLS
