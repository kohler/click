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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/wifi/path.hh>
#include <click/straccum.hh>
CLICK_DECLS

LinkTable::LinkTable()
  : _timer(this)
{
}



LinkTable::~LinkTable()
{
}


int
LinkTable::initialize (ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}

void
LinkTable::run_timer(Timer *)
{
  clear_stale();
  dijkstra(true);
  dijkstra(false);
  _timer.schedule_after_msec(5000);
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
  int stale_period = 120;
  ret = Args(conf, this, errh)
      .read("IP", _ip)
      .read("STALE", stale_period)
      .complete();

  if (!_ip)
    return errh->error("IP not specified");

  _stale_timeout.assign(stale_period, 0);

  _hosts.insert(_ip, HostInfo(_ip));
  return ret;
}


void
LinkTable::take_state(Element *e, ErrorHandler *) {
  LinkTable *q = (LinkTable *)e->cast("LinkTable");
  if (!q) return;

  _hosts = q->_hosts;
  _links = q->_links;
  dijkstra(true);
  dijkstra(false);
}

int
LinkTable::static_update_link(const String &arg, Element *e,
			      void *, ErrorHandler *errh)
{
  LinkTable *n = (LinkTable *) e;
  Vector<String> args;
  IPAddress from;
  IPAddress to;
  uint32_t seq;
  uint32_t age;
  uint32_t metric;
  cp_spacevec(arg, args);

  if (args.size() != 5) {
    return errh->error("Must have three arguments: currently has %d: %s", args.size(), args[0].c_str());
  }


  if (!IPAddressArg().parse(args[0], from)) {
    return errh->error("Couldn't read IPAddress out of from");
  }
  if (!IPAddressArg().parse(args[1], to)) {
    return errh->error("Couldn't read IPAddress out of to");
  }
  if (!IntArg().parse(args[2], metric)) {
    return errh->error("Couldn't read metric");
  }

  if (!IntArg().parse(args[3], seq)) {
    return errh->error("Couldn't read seq");
  }

  if (!IntArg().parse(args[4], age)) {
    return errh->error("Couldn't read age");
  }

  n->update_link(from, to, seq, age, metric);
  return 0;

}
void
LinkTable::clear()
{
  _hosts.clear();
  _links.clear();

}
bool
LinkTable::update_link(IPAddress from, IPAddress to,
		       uint32_t seq, uint32_t age, uint32_t metric)
{
  if (!from || !to || !metric) {
    return false;
  }
  if (_stale_timeout.sec() < (int) age) {
    return true;
  }

  /* make sure both the hosts exist */
  HostInfo *nfrom = _hosts.findp(from);
  if (!nfrom) {
    HostInfo foo = HostInfo(from);
    _hosts.insert(from, foo);
    nfrom = _hosts.findp(from);
  }
  HostInfo *nto = _hosts.findp(to);
  if (!nto) {
    _hosts.insert(to, HostInfo(to));
    nto = _hosts.findp(to);
  }

  assert(nfrom);
  assert(nto);

  IPPair p = IPPair(from, to);
  LinkInfo *lnfo = _links.findp(p);
  if (!lnfo) {
    _links.insert(p, LinkInfo(from, to, seq, age, metric));
  } else {
    lnfo->update(seq, age, metric);
  }
  return true;
}


LinkTable::Link
LinkTable::random_link()
{
  int ndx = click_random(0, _links.size() - 1);
  int current_ndx = 0;
  for (LTIter iter = _links.begin(); iter.live(); iter++, current_ndx++) {
    if (current_ndx == ndx) {
      LinkInfo n = iter.value();
      return Link(n._from, n._to, n._seq, n._metric);
    }
  }
  click_chatter("LinkTable %s: random_link overestimated number of elements\n",
		name().c_str());
  return Link();

}
Vector<IPAddress>
LinkTable::get_hosts()
{
  Vector<IPAddress> v;
  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    HostInfo n = iter.value();
    v.push_back(n._ip);
  }
  return v;
}

uint32_t
LinkTable::get_host_metric_to_me(IPAddress s)
{
  if (!s) {
    return 0;
  }
  HostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    return 0;
  }
  return nfo->_metric_to_me;
}

uint32_t
LinkTable::get_host_metric_from_me(IPAddress s)
{
  if (!s) {
    return 0;
  }
  HostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    return 0;
  }
  return nfo->_metric_from_me;
}

uint32_t
LinkTable::get_link_metric(IPAddress from, IPAddress to)
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  IPPair p = IPPair(from, to);
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  return nfo->_metric;
}
uint32_t
LinkTable::get_link_seq(IPAddress from, IPAddress to)
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  IPPair p = IPPair(from, to);
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  return nfo->_seq;
}
uint32_t
LinkTable::get_link_age(IPAddress from, IPAddress to)
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  IPPair p = IPPair(from, to);
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  return nfo->age();
}



unsigned
LinkTable::get_route_metric(const Vector<IPAddress> &route)
{
  unsigned metric = 0;
  for (int i = 0; i < route.size() - 1; i++) {
    unsigned m = get_link_metric(route[i], route[i+1]);
    if (m == 0) {
      return 0;
    }
    metric += m;
  }
  return metric;

}

String
LinkTable::route_to_string(Path p) {
	StringAccum sa;
	int hops = p.size()-1;
	int metric = 0;
	StringAccum sa2;
	for (int i = 0; i < p.size(); i++) {
		sa2 << p[i];
		if (i != p.size()-1) {
			int m = get_link_metric(p[i], p[i+1]);
			sa2 << " (" << m << ") ";
			metric += m;
		}
	}
	sa << p[p.size()-1] << " hops " << hops << " metric " << metric << " " << sa2;
	return sa.take_string();
}
bool
LinkTable::valid_route(const Vector<IPAddress> &route)
{
  if (route.size() < 1) {
    return false;
  }
  /* ensure the metrics are all valid */
  unsigned metric = get_route_metric(route);
  if (metric  == 0 ||
      metric >= 777777){
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
Vector<IPAddress>
LinkTable::best_route(IPAddress dst, bool from_me)
{
  Vector<IPAddress> reverse_route;
  if (!dst) {
    return reverse_route;
  }
  HostInfo *nfo = _hosts.findp(dst);

  if (from_me) {
    while (nfo && nfo->_metric_from_me != 0) {
      reverse_route.push_back(nfo->_ip);
      nfo = _hosts.findp(nfo->_prev_from_me);
    }
    if (nfo && nfo->_metric_from_me == 0) {
    reverse_route.push_back(nfo->_ip);
    }
  } else {
    while (nfo && nfo->_metric_to_me != 0) {
      reverse_route.push_back(nfo->_ip);
      nfo = _hosts.findp(nfo->_prev_to_me);
    }
    if (nfo && nfo->_metric_to_me == 0) {
      reverse_route.push_back(nfo->_ip);
    }
  }


  if (from_me) {
	  Vector<IPAddress> route;
	  /* why isn't there just push? */
	  for (int i=reverse_route.size() - 1; i >= 0; i--) {
		  route.push_back(reverse_route[i]);
	  }
	  return route;
  }

  return reverse_route;
}

String
LinkTable::print_links()
{
  StringAccum sa;
  for (LTIter iter = _links.begin(); iter.live(); iter++) {
    LinkInfo n = iter.value();
    sa << n._from.unparse() << " " << n._to.unparse();
    sa << " " << n._metric;
    sa << " " << n._seq << " " << n.age() << "\n";
  }
  return sa.take_string();
}

static int ipaddr_sorter(const void *va, const void *vb, void *) {
    IPAddress *a = (IPAddress *)va, *b = (IPAddress *)vb;
    if (a->addr() == b->addr()) {
	return 0;
    }
    return (ntohl(a->addr()) < ntohl(b->addr())) ? -1 : 1;
}


String
LinkTable::print_routes(bool from_me, bool pretty)
{
  StringAccum sa;

  Vector<IPAddress> ip_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++)
    ip_addrs.push_back(iter.key());

  click_qsort(ip_addrs.begin(), ip_addrs.size(), sizeof(IPAddress), ipaddr_sorter);

  for (int x = 0; x < ip_addrs.size(); x++) {
    IPAddress ip = ip_addrs[x];
    Vector <IPAddress> r = best_route(ip, from_me);
    if (valid_route(r)) {
	    if (pretty) {
		    sa << route_to_string(r) << "\n";
	    } else {
		    sa << r[r.size()-1] << "  ";
		    for (int a = 0; a < r.size(); a++) {
			    sa << r[a];
			    if (a < r.size() - 1) {
				    sa << " " << get_link_metric(r[a],r[a+1]);
				    sa << " (" << get_link_seq(r[a],r[a+1])
				       << "," << get_link_age(r[a],r[a+1])
				       << ") ";
			    }
		    }
		    sa << "\n";
	    }
    }
  }
  return sa.take_string();
}


String
LinkTable::print_hosts()
{
  StringAccum sa;
  Vector<IPAddress> ip_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++)
    ip_addrs.push_back(iter.key());

  click_qsort(ip_addrs.begin(), ip_addrs.size(), sizeof(IPAddress), ipaddr_sorter);

  for (int x = 0; x < ip_addrs.size(); x++)
    sa << ip_addrs[x] << "\n";

  return sa.take_string();
}



void
LinkTable::clear_stale() {

  LTable links;
  for (LTIter iter = _links.begin(); iter.live(); iter++) {
    LinkInfo nfo = iter.value();
    if ((unsigned) _stale_timeout.sec() >= nfo.age()) {
      links.insert(IPPair(nfo._from, nfo._to), nfo);
    } else {
      if (0) {
	click_chatter("%p{element} :: %s removing link %s -> %s metric %d seq %d age %d\n",
		      this,
		      __func__,
		      nfo._from.unparse().c_str(),
		      nfo._to.unparse().c_str(),
		      nfo._metric,
		      nfo._seq,
		      nfo.age());
      }
    }
  }
  _links.clear();

  for (LTIter iter = links.begin(); iter.live(); iter++) {
    LinkInfo nfo = iter.value();
    _links.insert(IPPair(nfo._from, nfo._to), nfo);
  }

}

Vector<IPAddress>
LinkTable::get_neighbors(IPAddress ip)
{
  Vector<IPAddress> neighbors;

  typedef HashMap<IPAddress, bool> IPMap;
  IPMap ip_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    ip_addrs.insert(iter.value()._ip, true);
  }

  for (IPMap::const_iterator i = ip_addrs.begin(); i.live(); i++) {
    HostInfo *neighbor = _hosts.findp(i.key());
    assert(neighbor);
    if (ip != neighbor->_ip) {
      LinkInfo *lnfo = _links.findp(IPPair(ip, neighbor->_ip));
      if (lnfo) {
	neighbors.push_back(neighbor->_ip);
      }
    }

  }

  return neighbors;
}
void
LinkTable::dijkstra(bool from_me)
{
  Timestamp start = Timestamp::now();
  IPAddress src = _ip;

  typedef HashMap<IPAddress, bool> IPMap;
  IPMap ip_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    ip_addrs.insert(iter.value()._ip, true);
  }

  for (IPMap::const_iterator i = ip_addrs.begin(); i.live(); i++) {
    /* clear them all initially */
    HostInfo *n = _hosts.findp(i.key());
    n->clear(from_me);
  }
  HostInfo *root_info = _hosts.findp(src);


  assert(root_info);

  if (from_me) {
    root_info->_prev_from_me = root_info->_ip;
    root_info->_metric_from_me = 0;
  } else {
    root_info->_prev_to_me = root_info->_ip;
    root_info->_metric_to_me = 0;
  }

  IPAddress current_min_ip = root_info->_ip;

  while (current_min_ip) {
    HostInfo *current_min = _hosts.findp(current_min_ip);
    assert(current_min);
    if (from_me) {
      current_min->_marked_from_me = true;
    } else {
      current_min->_marked_to_me = true;
    }


    for (IPMap::const_iterator i = ip_addrs.begin(); i.live(); i++) {
      HostInfo *neighbor = _hosts.findp(i.key());
      assert(neighbor);
      bool marked = neighbor->_marked_to_me;
      if (from_me) {
	marked = neighbor->_marked_from_me;
      }

      if (marked) {
	continue;
      }

      IPPair pair = IPPair(neighbor->_ip, current_min_ip);
      if (from_me) {
	pair = IPPair(current_min_ip, neighbor->_ip);
      }
      LinkInfo *lnfo = _links.findp(pair);
      if (!lnfo || !lnfo->_metric) {
	continue;
      }
      uint32_t neighbor_metric = neighbor->_metric_to_me;
      uint32_t current_metric = current_min->_metric_to_me;

      if (from_me) {
	neighbor_metric = neighbor->_metric_from_me;
	current_metric = current_min->_metric_from_me;
      }


      uint32_t adjusted_metric = current_metric + lnfo->_metric;
      if (!neighbor_metric ||
	  adjusted_metric < neighbor_metric) {
	if (from_me) {
	  neighbor->_metric_from_me = adjusted_metric;
	  neighbor->_prev_from_me = current_min_ip;
	} else {
	  neighbor->_metric_to_me = adjusted_metric;
	  neighbor->_prev_to_me = current_min_ip;
	}

      }
    }

    current_min_ip = IPAddress();
    uint32_t  min_metric = ~0;
    for (IPMap::const_iterator i = ip_addrs.begin(); i.live(); i++) {
      HostInfo *nfo = _hosts.findp(i.key());
      uint32_t metric = nfo->_metric_to_me;
      bool marked = nfo->_marked_to_me;
      if (from_me) {
	metric = nfo->_metric_from_me;
	marked = nfo->_marked_from_me;
      }
      if (!marked && metric &&
	  metric < min_metric) {
        current_min_ip = nfo->_ip;
        min_metric = metric;
      }
    }


  }

  dijkstra_time = Timestamp::now() - start;
  //StringAccum sa;
  //sa << "dijstra took " << finish - start;
  //click_chatter("%s: %s\n", name().c_str(), sa.take_string().c_str());
}


enum {H_BLACKLIST,
      H_BLACKLIST_CLEAR,
      H_BLACKLIST_ADD,
      H_BLACKLIST_REMOVE,
      H_LINKS,
      H_ROUTES_OLD,
      H_ROUTES_FROM,
      H_ROUTES_TO,
      H_HOSTS,
      H_CLEAR,
      H_DIJKSTRA,
      H_DIJKSTRA_TIME};

static String
LinkTable_read_param(Element *e, void *thunk)
{
  LinkTable *td = (LinkTable *)e;
    switch ((uintptr_t) thunk) {
    case H_BLACKLIST: {
      StringAccum sa;
      typedef HashMap<IPAddress, IPAddress> IPTable;
      typedef IPTable::const_iterator IPIter;


      for (IPIter iter = td->_blacklist.begin(); iter.live(); iter++) {
	sa << iter.value() << " ";
      }
      return sa.take_string() + "\n";
    }
    case H_LINKS:  return td->print_links();
    case H_ROUTES_TO: return td->print_routes(false, true);
    case H_ROUTES_FROM: return td->print_routes(true, true);
    case H_ROUTES_OLD: return td->print_routes(true, false);
    case H_HOSTS:  return td->print_hosts();
    case H_DIJKSTRA_TIME: {
      StringAccum sa;
      sa << td->dijkstra_time << "\n";
      return sa.take_string();
    }
    default:
      return String();
    }
}
static int
LinkTable_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  LinkTable *f = (LinkTable *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_BLACKLIST_CLEAR: {
    f->_blacklist.clear();
    break;
  }
  case H_BLACKLIST_ADD: {
    IPAddress m;
    if (!IPAddressArg().parse(s, m))
      return errh->error("blacklist_add parameter must be ipaddress");
    f->_blacklist.insert(m, m);
    break;
  }
  case H_BLACKLIST_REMOVE: {
    IPAddress m;
    if (!IPAddressArg().parse(s, m))
      return errh->error("blacklist_add parameter must be ipaddress");
    f->_blacklist.erase(m);
    break;
  }
  case H_CLEAR: f->clear(); break;
  case H_DIJKSTRA: f->dijkstra(true); f->dijkstra(false); break;
  }
  return 0;
}


void
LinkTable::add_handlers() {
  add_read_handler("routes", LinkTable_read_param, H_ROUTES_FROM);
  add_read_handler("routes_old", LinkTable_read_param, H_ROUTES_OLD);
  add_read_handler("routes_from", LinkTable_read_param, H_ROUTES_FROM);
  add_read_handler("routes_to", LinkTable_read_param, H_ROUTES_TO);
  add_read_handler("links", LinkTable_read_param, H_LINKS);
  add_read_handler("hosts", LinkTable_read_param, H_HOSTS);
  add_read_handler("blacklist", LinkTable_read_param, H_BLACKLIST);
  add_read_handler("dijkstra_time", LinkTable_read_param, H_DIJKSTRA_TIME);

  add_write_handler("clear", LinkTable_write_param, H_CLEAR);
  add_write_handler("blacklist_clear", LinkTable_write_param, H_BLACKLIST_CLEAR);
  add_write_handler("blacklist_add", LinkTable_write_param, H_BLACKLIST_ADD);
  add_write_handler("blacklist_remove", LinkTable_write_param, H_BLACKLIST_REMOVE);
  add_write_handler("dijkstra", LinkTable_write_param, H_DIJKSTRA);


  add_write_handler("update_link", static_update_link, 0);


}

EXPORT_ELEMENT(LinkTable)
CLICK_ENDDECLS
