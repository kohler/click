/*
 * gridroutetable.{cc,hh} -- Grid local neighbor and route tables element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/package.hh>
#include "gridroutetable.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_ether.h>
#include <click/click_ip.h>
#include <stddef.h>
#include "elements/standard/scheduleinfo.hh"
#include <click/router.hh>
#include "grid.hh"


GridRouteTable::GridRouteTable() : Element(1, 2), _max_hops(3), 
  _hello_timer(hello_hook, this), 
  _expire_timer(expire_hook, this),
  _seq_no(0)
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



int
GridRouteTable::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "entry timeout (msec)", &_timeout,
			cpInteger, "Hello broadcast period (msec)", &_period,
			cpInteger, "Hello broadcast jitter (msec)", &_jitter,
			cpEthernetAddress, "source Ethernet address", &_ethaddr,
			cpIPAddress, "source IP address", &_ipaddr,
			cpOptional,
			cpInteger, "max hops", &_max_hops,
			0);

  // convert msecs to jiffies
  if (_timeout == 0)
    _timeout = -1;
  if (_timeout > 0) {
    _timeout_jiffies = (CLICK_HZ * _timeout) / 1000;
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


void
GridRouteTable::push(Packet *packet)
{
  /*
   * expects grid LR packets, with ethernet and grid hdrs
   */
  assert(packet);
  int jiff = click_jiffies();


  /* 
   * sanity check the packet, get pointers to headers 
   */  
  click_ether *eh = (click_ether *) packet->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("GridRouteTable %s: got non-Grid packet type", id().cc());
    goto done;
  }
  grid_hdr *gh = (grid_hdr *) (eh + 1);

  if (gh->type != GRID_LR_HELLO) {
    click_chatter("GridRouteTable %s: received unknown Grid packet; ignoring it", id().cc());
    goto done;
  }
    
  IPAddress ipaddr((unsigned char *) &gh->tx_ip);
  EtherAddress ethaddr((unsigned char *) eh->ether_shost);

  if (ethaddr == _ethaddr) {
    click_chatter("GridRouteTable %s: received own Grid packet; ignoring it", id().cc());
    goto done;
  }
  
  grid_hello *hlo = (grid_hello *) (gh + 1);
   
   
  /*
   * add 1-hop route to packet's transmitter; perform some sanity
   * checking if entry already existed 
   */
  RTEntry *r = _rtes.findp(ipaddr);
  if (r && r->num_hops == 1 && r->next_hop_eth != ethaddr)
    click_chatter("GridRouteTable %s: ethernet address of %s changed from %s to %s", id().cc(), ipaddr.s().cc(), r->next_hop_eth.s().cc(), ethaddr.s().cc());
  else 
    click_chatter("GridRouteTable %s: adding new 1-hop route %s -- %s", 
		  id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
  
  _rtes.insert(ipaddr, RTEntry(ipaddr, ethaddr, gh, hlo, jiff));
  

  /*
   * loop through and process other route entries in hello message 
   */
  int entry_sz = hlo->nbr_entry_sz;
  Vector<RTEntry> triggered_rtes;
  Vector<IPAddress> broken_dests;
  
  grid_nbr_entry *curr = (grid_nbr_entry *) (hlo + 1);
  for (int i = 0; i < hlo->num_nbrs; i++, curr++) {

    /* ignore route to ourself */
    if (curr->ip == _ip)
      continue; 

    /* pseudo-split-horizon: ignore routes from nbrs that go back
       through us */
    if (curr->next_hop_ip == _ip)
      continue;

    RTEntry *our_rte = _rtes.findp(curr->ip);
    
    /* 
     * broken route advertisement 
     */
    if (curr->num_hops == 0) {
      assert(ntohl(curr->seq_no) & 1); // broken routes have odd seq_no
    
      /* if we don't have a route to this destination, ignore it */
      if (!our_rte)
	continue; 

      /* 
       * if our next hop to the destination is this packet's sender,
       * AND if the seq_no is newer than any information we have.
       * remove the broken route. 
       */
      if (our_rte->next_hop_ip == gh->ip &&
	  ntohl(curr->seq_no) > our_rte->seq_no) {
	broken_dests.push_back(curr->ip);
	
	/* generate triggered broken route advertisement */
	RTEntry broken_entry(ipaddr, ethaddr, curr, jiff);
	if (broken_entry.age > 0)
	  triggered_rtes.push_back(broken_entry);
      }
      /*
       * triggered advertisement: if we have a good route to the
       * destination with a newer seq_no, advertise our new
       * information.  
       */
      else if (ntohl(curr->seq_no) < our_rte->seq_no) {
	assert(!(our_rte->seq_no & 1)); // valid routes have even seq_no
	if (our_rte->age > 0)
	  triggered_rtes.push_back(*our_rte);
      }
    }

    /* skip routes with too many hops */
    // this would change if using proxies
    if (curr->num_hops + 1 > _max_hops)
	  continue; 


    /* 
     * regular route entry 
     */

    /* ignore old routes and long routes */
    if (our_rte                                        // we already have a route
	&& (our_rte->seq_no > ntohl(curr->seq_no)      // which has a newer seq_no
	    || (our_rte->seq_no == ntohl(curr->seq_no) // or the same seq_no
		&& curr->num_hops + 1 >= our_rte->num_hops))) // and is as close
      continue;
    
    
    /* add the entry */
    RTEntry new_entry(ipaddr, ethaddr, curr, jiff);
    _rtes.insert(curr->ip, new_entry);
  }

  /* delete broken routes */
  for (int i = 0; i < broken_rtes.size(); i++) {
    bool removed = _rtes.remove(broken_rtes[i]);
    assert(removed);
  }

  /* send triggered updates */
  if (triggered_rtes.size() > 0)
    send_routing_update(triggered_rtes);

 done:
  packet->kill();
}


Packet *
GridRouteTable::simple_action(Packet *packet)
{
  /*
   * expects grid packets, with MAC hdrs
   */
  assert(packet);
  int jiff = click_jiffies();
  
  /*
   * Update immediate neighbor table with this packet's transmitter's
   * info.
   */
  click_ether *eh = (click_ether *) packet->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("%s: got non-Grid packet type", id().cc());
    return packet;
  }
  grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
  IPAddress ipaddr((unsigned char *) &gh->tx_ip);
  EtherAddress ethaddr((unsigned char *) eh->ether_shost);

  if (ethaddr == _ethaddr) {
    click_chatter("%s: received own Grid packet; ignoring it", id().cc());
    return packet;
  }

  NbrEntry *nbr = _addresses.findp(ipaddr);
  if (nbr == 0) {
    // this src addr not already in map, so add it
    NbrEntry new_nbr(ethaddr, ipaddr, jiff);
    _addresses.insert(ipaddr, new_nbr);
    click_chatter("%s: adding %s -- %s", id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
  }
  else {
    // update jiffies and MAC for existing entry
    nbr->last_updated_jiffies = jiff;
    if (nbr->eth != ethaddr) 
      click_chatter("%s: updating %s -- %s", id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
    nbr->eth = ethaddr;
  }
  
  /* XXX need to update (or add) routing entry in _rtes to be the
     correct one-hop entry for destination of this sender, ipaddr. */
  
  /*
   * perform further packet processing, extrace routing information from DSDV packets
   */
  switch (gh->type) {
  case grid_hdr::GRID_LR_HELLO:
    {   
      grid_hello *hlo = (grid_hello *) (packet->data() + sizeof(click_ether) + sizeof(grid_hdr));

      /*
       * update far nbr info with this hello sender info -- list sender as
       * its own next hop.
       */
      far_entry *fe = _rtes.findp(ipaddr);
      if (fe == 0) {
	// we don't already know about it, so add it
	/* XXX not using HashMap2 very efficiently --- fix later */

	/* since DSDV update packets on travel one hop, all the tx_ip
           and tx_loc transmitter info in the grid_hdr is the same as
           the sender ip and loc info */

	_rtes.insert(ipaddr, far_entry(jiff, grid_nbr_entry(gh->ip, gh->ip, 1, ntohl(hlo->seq_no))));
	fe = _rtes.findp(ipaddr);
	fe->nbr.loc = gh->loc;
	fe->nbr.loc_err = ntohs(gh->loc_err);
	fe->nbr.loc_good = gh->loc_good;
	fe->nbr.age = decr_age(ntohl(hlo->age), grid_hello::MIN_AGE_DECREMENT);
      } else { 
	/* i guess we always overwrite existing info, because we heard
           this info directly from the node... */
	// update pre-existing information
	fe->last_updated_jiffies = jiff;
	fe->nbr.num_hops = 1;
	fe->nbr.next_hop_ip = gh->ip;
	fe->nbr.loc = gh->loc;
	fe->nbr.loc_err = ntohs(gh->loc_err);
	fe->nbr.loc_good = gh->loc_good;
	fe->nbr.seq_no = ntohl(hlo->seq_no);
	fe->nbr.age = decr_age(ntohl(hlo->age), grid_hello::MIN_AGE_DECREMENT);
      }

      
      /* 
       * add this sender's nbrs to our far neighbor list.  
       */
      int entry_sz = hlo->nbr_entry_sz;
      Vector<grid_nbr_entry> triggered_rtes;
      Vector<IPAddress> broken_rtes; 

      // loop through all the route entries in the packet
      for (int i = 0; i < hlo->num_nbrs; i++) {
	grid_nbr_entry *curr = (grid_nbr_entry *) (packet->data() + sizeof(click_ether) + 
						   sizeof(grid_hdr) + sizeof(grid_hello) +
						   i * entry_sz);

	if (IPAddress(curr->ip) == _ipaddr)
	  continue; // we already know how to get to ourself -- don't want to advertise some other strange route to us!

	if (IPAddress(curr->next_hop_ip) == _ipaddr)
	  continue; // pseduo-split-horizon: ignore routes from nbrs that go back through us

	if (curr->num_hops == 0) {
	  /* this entry indicates a broken route.  if the seq_no is
             newer than any information we have, AND we route to the
             specified address with this packet's sender as next hop,
             remove the broken route.  propagate broken route info.
             if the seq_no is older than some good route information
             we have, advertise our new information to overthrow the
             old broken route info we received */

	  IPAddress broken_ip(curr->ip);
	  fe = _rtes.findp(broken_ip);
	  if (fe != 0) {
	    if (ntohl(curr->seq_no) > fe->nbr.seq_no && fe->nbr.next_hop_ip == gh->ip) {
	      // invalidate a route we have through this next hop
	      grid_nbr_entry new_entry = fe->nbr;
	      new_entry.num_hops = 0;
	      // who told us about the broken route, so the
	      // pseudo-split-horizon will ignore this entry
	      new_entry.next_hop_ip = curr->ip; 
	      // broken route info should be odd seq_no's
	      new_entry.seq_no = ntohl(curr->seq_no);
	      assert((new_entry.seq_no & 1) == 1); // XXX convert this to more robust check
	      new_entry.age = decr_age(ntohl(curr->age), grid_hello::MIN_AGE_DECREMENT);
	      if (new_entry.age > 0) // don't propagate expired info
		triggered_rtes.push_back(new_entry);
	      broken_rtes.push_back(fe->nbr.ip);
	    }
	    else if (ntohl(curr->seq_no) < fe->nbr.seq_no) {
	      // we know more recent info about a route that this
	      // entry is trying to invalidate
	      grid_nbr_entry new_entry = fe->nbr;
	      assert((new_entry.seq_no & 1) == 0);
	      if (new_entry.age > 0)
		triggered_rtes.push_back(new_entry);
	    }
	  }
	  else
	    ; // else we never had a route to this broken destination anyway

	  continue;
	}

	if (curr->num_hops + 1 > _max_hops)
	  continue; // skip this one, we don't care about nbrs too many hops away

	IPAddress curr_ip(curr->ip);
	fe = _rtes.findp(curr_ip);
	if (fe == 0) {
	  // we don't already know about this nbr
	  _rtes.insert(curr_ip, far_entry(jiff, grid_nbr_entry(curr->ip, gh->ip, curr->num_hops + 1, ntohl(curr->seq_no))));
	  fe =_rtes.findp(curr_ip);
	  fe->nbr.loc = curr->loc;
	  fe->nbr.loc_err = ntohs(curr->loc_err);
	  fe->nbr.loc_good = curr->loc_good;
	  fe->nbr.age = decr_age(ntohl(curr->age), grid_hello::MIN_AGE_DECREMENT);
	}
	else { 
	  // replace iff seq_no is newer, or if seq_no is same and hops are less
	  unsigned int curr_seq = ntohl(curr->seq_no);
	  if (curr_seq > fe->nbr.seq_no ||
	      (curr_seq == fe->nbr.seq_no && (curr->num_hops + 1) < fe->nbr.num_hops)) {
	    fe->nbr.num_hops = curr->num_hops + 1;
	    fe->nbr.next_hop_ip = gh->ip;
	    fe->nbr.loc = curr->loc;
	    fe->nbr.loc_err = ntohs(curr->loc_err);
	    fe->nbr.loc_good = curr->loc_good;
	    fe->nbr.seq_no = curr_seq;
	    fe->last_updated_jiffies = jiff;
	    fe->nbr.age = decr_age(ntohl(curr->age), grid_hello::MIN_AGE_DECREMENT);
	    // if new entry is more than one hop away, remove from nbrs table also
	    if (fe->nbr.num_hops > 1)
	      _addresses.remove(fe->nbr.ip); // may fail, e.g. wasn't in nbr addresses table anyway
	  }
	}
      }
    
      if (triggered_rtes.size() > 0) {
        // send the triggered update
        send_routing_update(triggered_rtes, false);
      }

      // remove the broken routes
      for (int i = 0; i < broken_rtes.size(); i++)
	assert(_rtes.remove(broken_rtes[i]));
    }
    break;
    
  default:
    break;
  }
  return packet;
}



GridRouteTable *
GridRouteTable::clone() const
{
  return new GridRouteTable;
}


static String 
print_rtes(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  
  String s;
  return s;
}

static String
print_nbrs(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  
  String s;
  return s;
}

static String
print_ip(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  return n->_ipaddr.s();
}


static String
print_eth(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  return n->_ethaddr.s();
}


void
GridRouteTable::add_handlers()
{
  add_default_handlers(false);
  add_read_handler("nbrs", print_nbrs, 0);
  add_read_handler("rtes", print_rtes, 0);
  add_read_handler("ip", print_ip, 0);
  add_read_handler("eth", print_eth, 0);
}


void
GridRouteTable::expire_hook(Timer *, void *thunk) 
{
  GridRouteTable *n = (GridRouteTable *) thunk;

  expire_routes();

  n->_expire_timer.schedule_after_ms(EXPIRE_TIMER_PERIOD);
}


Vector<RTEntry>
UpdateGridRoutes::expire_routes()
{
  /* remove expired routes from the routing table.  return a vector of
     expired routes which is suitable for inclusion in a broken route
     advertisement. */

  assert(_timeout > 0);
  int jiff = click_jiffies();

  Vector<RTEntry> retval;

  typedef BigHashMap<IPAddress, bool> xip_t; // ``expired ip''
  typedef xip_t:Iterator xipi_t;
  xip_t expired_rtes;
  xip_t expired_next_hops;

  /* 1. loop through RT once, remembering destinations which have been
     in our RT too long (last_updated_jiffies too old) or have
     exceeded their ttl.  Also note those expired 1-hop entries --
     they may be someone's next hop. */
  for (RTIter i = _rtes.first(); i; i++) {
    if (jiff - i.value().last_updated_jiffies > _timeout_jiffies ||
	i.value().ttl == 0) {
      expired_rtes.insert(i.value().dest_ip, true);
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
	!expired_rtes.findp(i.value().dest_ip))
      expired_rtes.insert(i.value().dest_ip, true);
  }
  
  /* 3. Then, push all expired entries onto the return vector and
     erase them from the RT.  */
  for (xip_t i = expired_rtes.first(); i; i++) {
    RTEntry *r = _rtes.findp(i.key());
    assert(r);
    r->num_hops = 0;
    r->seq_no++; // odd numbers indicate broken routes
    r->ttl = grid_hello::MAX_AGE_DEFAULT;
    retval.push_back(*r);
  }
  for (xip_t i = expired_rtes.first(); i; i++) {
    bool removed = _rtes.remove(i.key());
    assert(removed);
  }

  return retval;
}

Vector<RTEntry>
UpdateGridRoutes::expire_routes()
{
  /* removes expired routes and immediate neighbor entries from the
     tables.  returns a vector of expired routes which is suitable for
     inclusion in a broken route advertisement. */

  assert(_timeout > 0);
  int jiff = click_jiffies();

  // XXX not sure if we are allowed to iterate while modifying map
  // (i.e. erasing entries), so figure out what to expire first.
  typedef BigHashMap<IPAddress, bool> xa_t;
  xa_t expired_addresses;
  Vector<grid_nbr_entry> expired_nbrs;

  // find the expired immediate entries
  for (UpdateGridRoutes::Table::Iterator iter = _addresses.first(); iter; iter++) 
    if (jiff - iter.value().last_updated_jiffies > _timeout_jiffies)
      expired_addresses.insert(iter.key(), true);

  // find expired routing entries -- either we have had the entry for
  // too long, or it has been flying around the whole network for too
  // long, or we have expired the next hop from our immediate neighbor
  for (UpdateGridRoutes::FarTable::Iterator iter = _rtes.first(); iter; iter++) {
    assert(iter.value().nbr.num_hops > 0);
    if (jiff - iter.value().last_updated_jiffies > _timeout_jiffies ||
	iter.value().nbr.age == 0 ||
	expired_addresses.findp(iter.value().nbr.next_hop_ip)) {
      grid_nbr_entry nbr = iter.value().nbr;
      nbr.num_hops = 0;
      nbr.seq_no++; // odd numbers indicate broken routes
      nbr.age = grid_hello::MAX_AGE_DEFAULT;
      expired_nbrs.push_back(nbr);
    }
  }

  // remove expired immediate nbr entries
  for (xa_t::Iterator iter = expired_addresses.first(); iter; iter++) {
    click_chatter("%s: expiring address for %s",
                  id().cc(), iter.key().s().cc());
    assert(_addresses.remove(iter.key()));
  }

  // remove expired route table entry
  for (int i = 0; i < expired_nbrs.size(); i++) {
    click_chatter("%s: expiring route entry for %s", id().cc(), IPAddress(expired_nbrs[i].ip).s().cc());
    assert(_rtes.remove(expired_nbrs[i].ip));
  }

  return expired_nbrs;
}


void
UpdateGridRoutes::hello_hook(Timer *, void *thunk)
{
  UpdateGridRoutes *n = (UpdateGridRoutes *) thunk;

  /* XXX is this a bug?  we expire some routes, but don't advertise
     them as broken anymore... */
  n->expire_routes();

  Vector<grid_nbr_entry> rte_entries;
  for (UpdateGridRoutes::FarTable::Iterator iter = n->_rtes.first(); iter; iter++) {
    /* XXX if everyone is using the same max-hops parameter, we could
       leave out all of our entries that are exactly max-hops hops
       away, because we know those entries will be greater than
       max-hops at any neighbor.  but, let's leave it in case we have
       different max-hops across the network */
    /* because we called expire_routes() at the top of this function,
       we know we are not propagating any route entries with age of 0
       or that have timed out */
    rte_entries.push_back(iter.value().nbr);
  }

  // make and send the packet
  n->send_routing_update(rte_entries, true);

  // XXX this random stuff is not right i think... wouldn't it be nice
  // if click had a phat RNG like ns?
  int r2 = random();
  double r = (double) (r2 >> 1);
  int jitter = (int) (((double) n->_jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  n->_hello_timer.schedule_after_ms(n->_period + (int) jitter);
}


void
UpdateGridRoutes::send_routing_update(Vector<grid_nbr_entry> &rte_info,
				      bool update_seq)
{
  /* build and send routing update packet advertising the contents of
     the rte_info vector.  calling function must fill in each nbr
     entry */

  _num_updates_sent++;

  int num_rtes = rte_info.size();
  int psz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_hello);
  psz += sizeof(grid_nbr_entry) * num_rtes;

  WritablePacket *p = Packet::make(psz + 2); // for alignment
  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  struct timeval tv;
  int res = gettimeofday(&tv, 0);
  if (res == 0) 
    p->set_timestamp_anno(tv);

  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _ethaddr.data(), 6);

  grid_hdr *gh = (grid_hdr *) (eh + 1);
  ASSERT_ALIGNED(gh);
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = psz - sizeof(click_ether);
  gh->total_len = htons(gh->total_len);
  gh->type = grid_hdr::GRID_LR_HELLO;
  gh->ip = gh->tx_ip = _ipaddr.addr();
  grid_hello *hlo = (grid_hello *) (gh + 1);
  assert(num_rtes <= 255);
  hlo->num_nbrs = (unsigned char) num_rtes;
  hlo->nbr_entry_sz = sizeof(grid_nbr_entry);
  hlo->seq_no = htonl(_seq_no);

  // Update the sequence number for periodic updates, but not
  // for triggered updates.
  if (update_seq) {
    /* originating sequence numbers are even, starting at 0.  odd
       numbers are reserved for other nodes to advertise a broken route
       to us.  from DSDV paper. */
    _seq_no += 2;
  }
  
  hlo->age = htonl(grid_hello::MAX_AGE_DEFAULT);

  grid_nbr_entry *curr = (grid_nbr_entry *) (hlo + 1);
  for (int i = 0; i < num_rtes; i++) {
    *curr = rte_info[i];
    curr->seq_no = htonl(curr->seq_no);
    curr->age = htonl(curr->age);
    curr++;
  }

  output(1).push(p);
}



ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridRouteTable)

#include <click/bighashmap.cc>
template class BigHashMap<IPAddress, UpdateGridRoutes::NbrEntry>;
template class BigHashMap<IPAddress, UpdateGridRoutes::far_entry>;
#include <click/vector.cc>
template class Vector<IPAddress>;
template class Vector<grid_nbr_entry>;
