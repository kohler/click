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
#include <elements/wifi/sr/path.hh>
#include <click/straccum.hh>
CLICK_DECLS

LinkTable::LinkTable() 
  : _timer(this)
{
  MOD_INC_USE_COUNT;
}



LinkTable::~LinkTable() 
{
  MOD_DEC_USE_COUNT;
}


int
LinkTable::initialize (ErrorHandler *) 
{
  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}

void
LinkTable::run_timer() 
{
  clear_stale();
  dijkstra(true);
  dijkstra(false);
  _timer.schedule_after_ms(5000);
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
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
                    "IP", cpIPAddress, "IP address", &_ip,
		    "STALE", cpUnsigned, "Stale info timeout", &stale_period,
                    cpEnd);
  
  if (!_ip) 
    return errh->error("IP not specified");

  _stale_timeout.tv_sec = stale_period;
  _stale_timeout.tv_usec = 0;

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
    return errh->error("Must have three arguments: currently has %d: %s", args.size(), args[0].cc());
  }


  if (!cp_ip_address(args[0], &from)) {
    return errh->error("Couldn't read IPAddress out of from");
  }

  if (!cp_ip_address(args[1], &to)) {
    return errh->error("Couldn't read IPAddress out of to");
  }
  if (!cp_unsigned(args[2], &metric)) {
    return errh->error("Couldn't read metric");
  }

  if (!cp_unsigned(args[3], &seq)) {
    return errh->error("Couldn't read seq");
  }

  if (!cp_unsigned(args[4], &age)) {
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
  int ndx = random() % _links.size();
  int current_ndx = 0;
  for (LTIter iter = _links.begin(); iter; iter++, current_ndx++) {
    if (current_ndx == ndx) {
      LinkInfo n = iter.value();
      return Link(n._from, n._to, n._seq, n._metric);
    }
  }
  click_chatter("LinkTable %s: random_link overestimated number of elements\n",
		id().cc());
  return Link();

}
Vector<IPAddress> 
LinkTable::get_hosts()
{
  Vector<IPAddress> v;
  for (HTIter iter = _hosts.begin(); iter; iter++) {
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
  struct timeval now;
  click_gettimeofday(&now);
  return nfo->age();
}
  
  
  
unsigned 
LinkTable::get_route_metric(Vector<IPAddress> route) 
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
LinkTable::routes_to_string(Vector< Vector<IPAddress> > routes) {
  StringAccum sa;
  for (int x = 0; x < routes.size(); x++) {
    Vector <IPAddress> r = routes[x];
    for (int i = 0; i < r.size(); i++) {
      if (i != 0) {
	sa << " ";
      }      
      sa << r[i] << " ";
      if (i != r.size()-1) {
	sa << get_link_metric(r[i], r[i+1]);
      }
      
    }
    if (r.size() > 0) {
      sa << "\n";
    }
  }
  return sa.take_string();
}
bool
LinkTable::valid_route(Vector<IPAddress> route) 
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
  Vector<IPAddress> route;
  if (!dst) {
    return route;
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
    /* why isn't there just push? */
    for (int i=reverse_route.size() - 1; i >= 0; i--) {
      route.push_back(reverse_route[i]);
    }
    return route;
  }

  return reverse_route;
}

String routes_to_string(Vector<Path> routes) {
  StringAccum sa;
  for (int x = 1; x < routes.size(); x++) {
    sa << path_to_string(routes[x]).cc() << "\n";
  }
  return sa.take_string();
}


String 
LinkTable::print_links() 
{
  StringAccum sa;
  struct timeval now;
  click_gettimeofday(&now);
  for (LTIter iter = _links.begin(); iter; iter++) {
    LinkInfo n = iter.value();
    sa << n._from.s().cc() << " " << n._to.s().cc();
    sa << " " << n._metric;
    sa << " " << n._seq << " " << n.age() << "\n";
  }
  return sa.take_string();
}



String 
LinkTable::print_routes(bool from_me) 
{
  StringAccum sa;

  typedef HashMap<IPAddress, bool> IPMap;
  IPMap ip_addrs;

  for (HTIter iter = _hosts.begin(); iter; iter++) {
    HostInfo n = iter.value();
    ip_addrs.insert(n._ip, true);
  }
  
  for (IPMap::const_iterator i = ip_addrs.begin(); i; i++) {
    const IPAddress &ip = i.key();
    Vector <IPAddress> r = best_route(ip, from_me);
    if (valid_route(r)) {
      sa << ip.s().cc() << " ";
      for (int i = 0; i < r.size(); i++) {
	sa << " " << r[i] << " ";
	if (i != r.size()-1) {
	  IPPair pair = IPPair(r[i], r[i+1]);
	  LinkInfo *l = _links.findp(pair);
	  assert(l);
	  sa << l->_metric;
	  sa << " (" << l->_seq << "," << l->age() << ")";
	}
      }
      sa << "\n";
    }
  }
  return sa.take_string();
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
LinkTable::clear_stale() {

  class LTable links;
  for (LTIter iter = _links.begin(); iter; iter++) {
    LinkInfo nfo = iter.value();
    if ((unsigned) _stale_timeout.tv_sec >= nfo.age()) {
      links.insert(IPPair(nfo._from, nfo._to), nfo);
    } else {
      click_chatter("%{element} :: %s removing link %s -> %s metric %d seq %d age %d\n",
		    this,
		    __func__,
		    nfo._from.s().cc(),
		    nfo._to.s().cc(),
		    nfo._metric,
		    nfo._seq, 
		    nfo.age());
    }
  }
  _links.clear();

  for (LTIter iter = links.begin(); iter; iter++) {
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

  for (HTIter iter = _hosts.begin(); iter; iter++) {
    ip_addrs.insert(iter.value()._ip, true);
  }

  for (IPMap::const_iterator i = ip_addrs.begin(); i; i++) {
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
  struct timeval start;
  struct timeval finish;
  click_gettimeofday(&start);
  IPAddress src = _ip;

  typedef HashMap<IPAddress, bool> IPMap;
  IPMap ip_addrs;

  for (HTIter iter = _hosts.begin(); iter; iter++) {
    ip_addrs.insert(iter.value()._ip, true);
  }
  
  for (IPMap::const_iterator i = ip_addrs.begin(); i; i++) {
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


    for (IPMap::const_iterator i = ip_addrs.begin(); i; i++) {
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
    for (IPMap::const_iterator i = ip_addrs.begin(); i; i++) {
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
  
  click_gettimeofday(&finish);
  timersub(&finish, &start, &dijkstra_time);
  //StringAccum sa;
  //sa << "dijstra took " << finish - start;
  //click_chatter("%s: %s\n", id().cc(), sa.take_string().cc());
}


enum {H_BLACKLIST, 
      H_BLACKLIST_CLEAR, 
      H_BLACKLIST_ADD, 
      H_BLACKLIST_REMOVE,
      H_LINKS,
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
  

      for (IPIter iter = td->_blacklist.begin(); iter; iter++) {
	sa << iter.value() << " ";
      }
      return sa.take_string() + "\n";
    }
    case H_LINKS:  return td->print_links();
    case H_ROUTES_TO: return td->print_routes(false);
    case H_ROUTES_FROM: return td->print_routes(true);
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
  switch((int)vparam) {
  case H_BLACKLIST_CLEAR: {
    f->_blacklist.clear();
    break;
  }
  case H_BLACKLIST_ADD: {
    IPAddress m;
    if (!cp_ip_address(s, &m)) 
      return errh->error("blacklist_add parameter must be ipaddress");
    f->_blacklist.insert(m, m);
    break;
  }
  case H_BLACKLIST_REMOVE: {
    IPAddress m;
    if (!cp_ip_address(s, &m)) 
      return errh->error("blacklist_add parameter must be ipaddress");
    f->_blacklist.remove(m);
    break;
  }
  case H_CLEAR: f->clear(); break;
  case H_DIJKSTRA: f->dijkstra(true); f->dijkstra(false); break;
  }
  return 0;
}


void
LinkTable::add_handlers() {
  add_default_handlers(false);
  add_read_handler("routes", LinkTable_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_from", LinkTable_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_to", LinkTable_read_param, (void *)H_ROUTES_TO);
  add_read_handler("links", LinkTable_read_param, (void *)H_LINKS);
  add_read_handler("hosts", LinkTable_read_param, (void *)H_HOSTS);
  add_read_handler("blacklist", LinkTable_read_param, (void *)H_BLACKLIST);
  add_read_handler("dijkstra_time", LinkTable_read_param, (void *)H_DIJKSTRA_TIME);

  add_write_handler("clear", LinkTable_write_param, (void *)H_CLEAR);
  add_write_handler("blacklist_clear", LinkTable_write_param, (void *)H_BLACKLIST_CLEAR);
  add_write_handler("blacklist_add", LinkTable_write_param, (void *)H_BLACKLIST_ADD);
  add_write_handler("blacklist_remove", LinkTable_write_param, (void *)H_BLACKLIST_REMOVE);
  add_write_handler("dijkstra", LinkTable_write_param, (void *)H_DIJKSTRA);


  add_write_handler("update_link", static_update_link, 0);


}



#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<IPAddress, IPAddress>;
template class HashMap<IPPair, LinkTable::LinkInfo>;
template class HashMap<IPAddress, LinkTable::HostInfo>;
#endif
EXPORT_ELEMENT(LinkTable)
CLICK_ENDDECLS
