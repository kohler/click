/*
 * gridroutetable.{cc,hh} -- Grid local neighbor and route tables element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <click/click_ether.h>
#include <click/click_ip.h>
#include <stddef.h>
#include <click/standard/scheduleinfo.hh>
#include <click/router.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include "gridroutetable.hh"


GridRouteTable::GridRouteTable() : 
  Element(1, 1), 
  _seq_no(0),
  _max_hops(3), 
  _expire_timer(expire_hook, this),
  _hello_timer(hello_hook, this)
{
  MOD_INC_USE_COUNT;
}

GridRouteTable::~GridRouteTable()
{
  MOD_DEC_USE_COUNT;
}



void *
GridRouteTable::cast(const char *n)
{
  if (strcmp(n, "GridRouteTable") == 0)
    return (GridRouteTable *) this;
  else
    return 0;
}



void
GridRouteTable::log_route_table ()
{
  char str[80];
  for (RTIter i = _rtes.first(); i; i++) {
    const RTEntry &f = i.value();
    
    snprintf(str, sizeof(str), 
	    "%s %f %f %s %d %c %u\n", 
	    f.dest_ip.s().cc(),
	    f.loc.lat(),
	    f.loc.lon(),
	    f.next_hop_ip.s().cc(),
	    f.num_hops,
	    (f.is_gateway ? 'y' : 'n'),
	    f.seq_no);
    _extended_logging_errh->message(str);
  }
  _extended_logging_errh->message("\n");
}


int
GridRouteTable::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String chan("routelog");
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "entry timeout (msec)", &_timeout,
			cpInteger, "route broadcast period (msec)", &_period,
			cpInteger, "route broadcast jitter (msec)", &_jitter,
			cpEthernetAddress, "source Ethernet address", &_eth,
			cpIPAddress, "source IP address", &_ip,
			cpElement, "GridGatewayInfo element", &_gw_info,
			cpOptional,
			cpInteger, "max hops", &_max_hops,
			cpString, "log channel name", &chan,
			0);

  // convert msecs to jiffies
  if (_timeout == 0)
    _timeout = -1;
  if (_timeout > 0) {
    _timeout_jiffies = msec_to_jiff(_timeout);
    if (_timeout_jiffies < 1)
      return errh->error("timeout interval is too small");
  }
  else
    click_chatter("%s: not timing out table entries", id().cc());

  if (_period <= 0)
    return errh->error("period must be greater than 0");
  if (_jitter < 0)
    return errh->error("period must be positive");
  if (_jitter > _period)
    return errh->error("jitter is bigger than period");
  if (_max_hops < 0)
    return errh->error("max hops must be greater than 0");

  _extended_logging_errh = router()->chatter_channel(chan);

  return res;
}



int
GridRouteTable::initialize(ErrorHandler *)
{
  _hello_timer.initialize(this);
  _hello_timer.schedule_after_ms(_period); // Send periodically

  _expire_timer.initialize(this);
  if (_timeout > 0)
    _expire_timer.schedule_after_ms(EXPIRE_TIMER_PERIOD);

  return 0;
}


const GridRouteTable::RTEntry *
GridRouteTable::current_gateway()
{
  for (RTIter i = _rtes.first(); i; i++) {
    const RTEntry &f = i.value();

    if (f.is_gateway)
      return &f;
  }

  return NULL;
}


/*
 * expects grid LR packets, with ethernet and grid hdrs
 */
Packet *
GridRouteTable::simple_action(Packet *packet)
{
  assert(packet);
  int jiff = click_jiffies();

  /* 
   * sanity check the packet, get pointers to headers 
   */  
  click_ether *eh = (click_ether *) packet->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("GridRouteTable %s: got non-Grid packet type", id().cc());
    packet->kill();
    return 0;
  }
  grid_hdr *gh = (grid_hdr *) (eh + 1);

  if (gh->type != grid_hdr::GRID_LR_HELLO) {
    click_chatter("GridRouteTable %s: received unknown Grid packet; ignoring it", id().cc());
    packet->kill();
    return 0;
  }
    
  IPAddress ipaddr((unsigned char *) &gh->tx_ip);
  EtherAddress ethaddr((unsigned char *) eh->ether_shost);

  // this should be redundant (see HostEtherFilter in grid.click)
  if (ethaddr == _eth) {
    click_chatter("GridRouteTable %s: received own Grid packet; ignoring it", id().cc());
    packet->kill();
    return 0;
  }

  grid_hello *hlo = (grid_hello *) (gh + 1);
   
  // extended logging
  timeval tv;
  gettimeofday(&tv, NULL);
  _extended_logging_errh->message("recvd %u from %s %ld %ld", ntohl(hlo->seq_no), ipaddr.s().cc(), tv.tv_sec, tv.tv_usec);

  /*
   * add 1-hop route to packet's transmitter; perform some sanity
   * checking if entry already existed 
   */

  RTEntry *r = _rtes.findp(ipaddr);

  if (!r)
    click_chatter("GridRouteTable %s: adding new 1-hop route %s -- %s", 
		  id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
  else if (r->num_hops == 1 && r->next_hop_eth != ethaddr)
    click_chatter("GridRouteTable %s: ethernet address of %s changed from %s to %s", 
		  id().cc(), ipaddr.s().cc(), r->next_hop_eth.s().cc(), ethaddr.s().cc());

  if (ntohl(hlo->ttl) > 0)
    _rtes.insert(ipaddr, RTEntry(ipaddr, ethaddr, gh, hlo, jiff));
  
  /*
   * loop through and process other route entries in hello message 
   */
  Vector<RTEntry> triggered_rtes;
  Vector<IPAddress> broken_dests;

  int entry_sz = hlo->nbr_entry_sz;
  char *entry_ptr = (char *) (hlo + 1);
  
  for (int i = 0; i < hlo->num_nbrs; i++, entry_ptr += entry_sz) {
    
    grid_nbr_entry *curr = (grid_nbr_entry *) entry_ptr;
    RTEntry route(ipaddr, ethaddr, curr, jiff);

    /* ignore route if ttl has run out */
    if (route.ttl <= 0)
      continue;

    /* ignore route to ourself */
    if (route.dest_ip == _ip)
      continue;

    /* pseudo-split-horizon: ignore routes from nbrs that go back
       through us */
    if (curr->next_hop_ip == (unsigned int) _ip)
      continue;

    RTEntry *our_rte = _rtes.findp(curr->ip);
    
    /* 
     * broken route advertisement 
     */
    if (curr->num_hops == 0) {

      assert(route.seq_no & 1); // broken routes have odd seq_no
    
      /* if we don't have a route to this destination, ignore it */
      if (!our_rte)
	continue;

      /* 
       * if our next hop to the destination is this packet's sender,
       * AND if the seq_no is newer than any information we have.
       * remove the broken route. 
       */
      if (our_rte->next_hop_ip == ipaddr &&
	  route.seq_no > our_rte->seq_no) {
	broken_dests.push_back(route.dest_ip);
	
	/* generate triggered broken route advertisement */
	triggered_rtes.push_back(route);
      }
      /*
       * otherwise, triggered advertisement: if we have a good route
       * to the destination with a newer seq_no, advertise our new
       * information. 
       */
      else if (route.seq_no < our_rte->seq_no) {
	assert(!(our_rte->seq_no & 1)); // valid routes have even seq_no
	if (our_rte->ttl > 0)
	  triggered_rtes.push_back(*our_rte);
      }
      continue;
    }

    /* skip routes with too many hops */
    // this would change if using proxies
    if (route.num_hops + 1 > _max_hops)
      continue;

    /* 
     * regular route entry 
     */

    /* ignore old routes and long routes */
    if (our_rte                                  // we already have a route
	&& (our_rte->seq_no > route.seq_no       // which has a newer seq_no
	    || (our_rte->seq_no == route.seq_no  // or the same seq_no
		&& route.num_hops + 1 >= our_rte->num_hops))) // and is as close
      continue;

    /* add the entry */
    _rtes.insert(route.dest_ip, route);
  }

  /* delete broken routes */
  for (int i = 0; i < broken_dests.size(); i++) {
    bool removed = _rtes.remove(broken_dests[i]);
    assert(removed);
  }

  log_route_table();  // extended logging

  /* send triggered updates */
  if (triggered_rtes.size() > 0)
    send_routing_update(triggered_rtes, false); // XXX should seq_no get incremented?

  packet->kill();
  return 0;
}


GridRouteTable *
GridRouteTable::clone() const
{
  return new GridRouteTable;
}


String 
GridRouteTable::print_rtes_v(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;

  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    const RTEntry &f = i.value();
    s += f.dest_ip.s() 
      + " next=" + f.next_hop_ip.s() 
      + " hops=" + String((int) f.num_hops) 
      + " gw=" + (f.is_gateway ? "y" : "n")
      + " loc=" + f.loc.s()
      + " err=" + (f.loc_good ? "" : "-") + String(f.loc_err) // negate loc if invalid
      + " seq=" + String(f.seq_no)
      + "\n";
  }
  
  return s;
}

String 
GridRouteTable::print_rtes(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;

  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    const RTEntry &f = i.value();
    s += f.dest_ip.s() 
      + " next=" + f.next_hop_ip.s() 
      + " hops=" + String((int) f.num_hops) 
      + " gw=" + (f.is_gateway ? "y" : "n")
      //      + " loc=" + f.loc.s()
      //      + " err=" + (f.loc_good ? "" : "-") + String(f.loc_err) // negate loc if invalid
      + " seq=" + String(f.seq_no)
      + "\n";
  }
  
  return s;
}

String
GridRouteTable::print_nbrs(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  
  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    /* only print immediate neighbors */
    if (i.value().num_hops != 1)
      continue;
    s += i.key().s();
    s += " eth=" + i.value().next_hop_eth.s();
    s += "\n";
  }

  return s;
}


String
GridRouteTable::print_ip(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  return n->_ip.s();
}


String
GridRouteTable::print_eth(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  return n->_eth.s();
}


void
GridRouteTable::add_handlers()
{
  add_default_handlers(false);
  add_read_handler("nbrs", print_nbrs, 0);
  add_read_handler("rtes_v", print_rtes_v, 0);
  add_read_handler("rtes", print_rtes, 0);
  add_read_handler("ip", print_ip, 0);
  add_read_handler("eth", print_eth, 0);
}


void
GridRouteTable::expire_hook(Timer *, void *thunk) 
{
  GridRouteTable *n = (GridRouteTable *) thunk;
  n->expire_routes();
  n->_expire_timer.schedule_after_ms(EXPIRE_TIMER_PERIOD);
}


Vector<GridRouteTable::RTEntry>
GridRouteTable::expire_routes()
{
  /*
   * remove expired routes from the routing table.  return a vector of
   * expired routes which is suitable for inclusion in a broken route
   * advertisement. 
   */

  assert(_timeout > 0);
  int jiff = click_jiffies();

  Vector<RTEntry> retval;

  typedef BigHashMap<IPAddress, bool> xip_t; // ``expired ip''
  xip_t expired_rtes;
  xip_t expired_next_hops;

  timeval tv;
  gettimeofday(&tv, NULL);

  bool table_changed = false;

  /* 1. loop through RT once, remembering destinations which have been
     in our RT too long (last_updated_jiffies too old) or have
     exceeded their ttl.  Also note those expired 1-hop entries --
     they may be someone's next hop. */
  for (RTIter i = _rtes.first(); i; i++) {
    if (jiff - i.value().last_updated_jiffies > _timeout_jiffies ||
	decr_ttl(i.value().ttl, jiff_to_msec(jiff - i.value().last_updated_jiffies)) == 0) {
      expired_rtes.insert(i.value().dest_ip, true);

      _extended_logging_errh->message ("expiring %s %ld %ld", i.value().dest_ip.s().cc(), tv.tv_sec, tv.tv_usec);  // extended logging
      table_changed = true;

      if (i.value().num_hops == 1) /* may be another route's next hop */
	expired_next_hops.insert(i.value().dest_ip, true);
    }
  }
  
  /* 2. Loop through RT a second time, picking up any multi-hop
     entries whose next hop is expired, and are not yet expired. */
  for (RTIter i = _rtes.first(); i; i++) {
    // don't re-expire 1-hop routes, they are their own next hop
    if (i.value().num_hops > 1 &&
	expired_next_hops.findp(i.value().next_hop_ip) &&
	!expired_rtes.findp(i.value().dest_ip)) {
      expired_rtes.insert(i.value().dest_ip, true);

      _extended_logging_errh->message("next to %s expired %ld %ld", i.value().dest_ip.s().cc(), tv.tv_sec, tv.tv_usec);  // extended logging
    }
  }
  
  /* 3. Then, push all expired entries onto the return vector and
     erase them from the RT.  */
  for (xip_t::Iterator i = expired_rtes.first(); i; i++) {
    RTEntry *r = _rtes.findp(i.key());
    assert(r);
    r->num_hops = 0;
    r->seq_no++; // odd numbers indicate broken routes
    assert(r->seq_no & 1);
    r->ttl = grid_hello::MAX_TTL_DEFAULT;
    retval.push_back(*r);
  }
  for (xip_t::Iterator i = expired_rtes.first(); i; i++) {
    bool removed = _rtes.remove(i.key());
    assert(removed);
  }

  if (table_changed)
    log_route_table();  // extended logging

  return retval;
}


void
GridRouteTable::hello_hook(Timer *, void *thunk)
{
  GridRouteTable *n = (GridRouteTable *) thunk;

  /* XXX is this a bug?  we expire some routes, but don't advertise
     them as broken anymore... */
  n->expire_routes();

  Vector<RTEntry> rte_entries;
  for (RTIter i = n->_rtes.first(); i; i++) {
    /* because we called expire_routes() at the top of this function,
     * we know we are not propagating any route entries with ttl of 0
     * or that have timed out */
    rte_entries.push_back(i.value());
  }

  // make and send the packet
  n->send_routing_update(rte_entries);

  int r2 = random();
  double r = (double) (r2 >> 1);
  int jitter = (int) (((double) n->_jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  n->_hello_timer.schedule_after_ms(n->_period + (int) jitter);
}


void
GridRouteTable::send_routing_update(Vector<RTEntry> &rtes_to_send,
				    bool update_seq, bool check_ttls)
{
  /*
   * build and send routing update packet advertising the contents of
   * the rte_info vector.  iff update_seq, increment the sequence
   * number before sending.  The calling function must fill in each
   * nbr entry.  If check_ttls, decrement and check ttls before
   * building the packet.
   */

  int jiff = click_jiffies();

  Vector<RTEntry> rte_info = Vector<RTEntry>();

  /* 
   * if requested by caller, calculate the ttls each route entry
   * should be sent with.  Each entry's ttl must be decremeneted by a
   * minimum amount.  Only send the routes with valid ttls (> 0).
   */
  for (int i = 0; i < rtes_to_send.size(); i++) {
    RTEntry &r = rtes_to_send[i];
    if (check_ttls) {
      unsigned int age = jiff_to_msec(jiff - r.last_updated_jiffies);
      unsigned int new_ttl = decr_ttl(r.ttl, (age > grid_hello::MIN_TTL_DECREMENT ? age : grid_hello::MIN_TTL_DECREMENT));
      if (new_ttl > 0) {
	r.ttl = new_ttl;
	rte_info.push_back(r);
      }
    } else {
      rte_info.push_back(r);
    }
  }

  int hdr_sz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_hello);
  int max_rtes = (1500 - hdr_sz) / sizeof(grid_nbr_entry);
  int num_rtes = (max_rtes < rte_info.size() ? max_rtes : rte_info.size()); // min
  int psz = hdr_sz + sizeof(grid_nbr_entry) * num_rtes;

  assert(psz <= 1500);
  if (num_rtes < rte_info.size())
    click_chatter("GridRouteTable %s: too many routes, truncating route advertisement",
		  id().cc());

  /* allocate and align the packet */
  WritablePacket *p = Packet::make(psz + 2); // for alignment
  if (p == 0) {
    click_chatter("in %s: cannot make packet!", id().cc());
    assert(0);
  } 
  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  /* fill in the timestamp */
  struct timeval tv;
  int res = gettimeofday(&tv, 0);
  if (res == 0) 
    p->set_timestamp_anno(tv);

  /* fill in ethernet header */
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _eth.data(), 6);

  /* fill in the grid header */
  grid_hdr *gh = (grid_hdr *) (eh + 1);
  ASSERT_ALIGNED(gh);
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = htons(psz - sizeof(click_ether));
  gh->type = grid_hdr::GRID_LR_HELLO;
  gh->ip = gh->tx_ip = _ip;
  grid_hello *hlo = (grid_hello *) (gh + 1);
  assert(num_rtes <= 255);
  hlo->num_nbrs = (unsigned char) num_rtes;
  hlo->nbr_entry_sz = sizeof(grid_nbr_entry);
  hlo->seq_no = htonl(_seq_no);

  hlo->is_gateway = _gw_info->is_gateway ();

  /* 
   * Update the sequence number for periodic updates, but not for
   * triggered updates.  originating sequence numbers are even,
   * starting at 0.  odd numbers are reserved for other nodes to
   * advertise broken routes 
   */
  assert(!(_seq_no & 1));
  if (update_seq) 
    _seq_no += 2;
  
  /* extended logging */
  gettimeofday(&tv, NULL);
  _extended_logging_errh->message("sending %u %ld %ld", _seq_no, tv.tv_sec, tv.tv_usec);

  hlo->ttl = htonl(grid_hello::MAX_TTL_DEFAULT);

  grid_nbr_entry *curr = (grid_nbr_entry *) (hlo + 1);

  char str[80];
  for (int i = 0; i < num_rtes; i++, curr++) {

    const RTEntry &f = rte_info[i];
    snprintf(str, sizeof(str), 
	     "%s %f %f %s %d %c %u\n", 
	     f.dest_ip.s().cc(),
	     f.loc.lat(),
	     f.loc.lon(),
	     f.next_hop_ip.s().cc(),
	     f.num_hops,
	     (f.is_gateway ? 'y' : 'n'),
	     f.seq_no);
    _extended_logging_errh->message(str);

    rte_info[i].fill_in(curr);
  }
  
  _extended_logging_errh->message("\n");

  output(0).push(p);
}


void
GridRouteTable::RTEntry::fill_in(grid_nbr_entry *nb)
{
  nb->ip = dest_ip;
  nb->next_hop_ip = next_hop_ip;
  nb->num_hops = num_hops;
  nb->loc = loc;
  nb->loc_err = htons(loc_err);
  nb->loc_good = loc_good;
  nb->seq_no = htonl(seq_no);

  nb->is_gateway = is_gateway;
  nb->ttl = htonl(ttl);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridRouteTable)

#include <click/bighashmap.cc>
template class BigHashMap<IPAddress, GridRouteTable::RTEntry>;
template class BigHashMap<IPAddress, bool>;
#include <click/vector.cc>
template class Vector<IPAddress>;
template class Vector<GridRouteTable::RTEntry>;
