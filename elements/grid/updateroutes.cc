/*
 * updateroutes.{cc,hh} -- Grid local neighbor and route tables element
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
#include "updateroutes.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/standard/scheduleinfo.hh>
#include <click/router.hh>
#include "grid.hh"
CLICK_DECLS

UpdateGridRoutes::UpdateGridRoutes() : _max_hops(3),
  _hello_timer(hello_hook, this),
  _expire_timer(expire_hook, this),
  _sanity_timer(sanity_hook, this),
  _num_updates_sent(0), _seq_no(0)
{
}

UpdateGridRoutes::~UpdateGridRoutes()
{
}



void *
UpdateGridRoutes::cast(const char *n)
{
  if (strcmp(n, "UpdateGridRoutes") == 0)
    return (UpdateGridRoutes *)this;
  else
    return 0;
}



int
UpdateGridRoutes::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int res = Args(conf, this, errh)
	.read_mp("TIMEOUT", _timeout)
	.read_mp("PERIOD", _period)
	.read_mp("JITTER", _jitter)
	.read_mp("ETH", _ethaddr)
	.read_mp("IP", _ipaddr)
	.read_p("MAXHOPS", _max_hops)
	.complete();
  if (res < 0)
    return res;

  // convert msecs to jiffies
  if (_timeout == 0)
    _timeout = -1;
  if (_timeout > 0) {
    _timeout_jiffies = (CLICK_HZ * _timeout) / 1000;
    if (_timeout_jiffies < 1)
      return errh->error("timeout interval is too small");
  }
  else
    click_chatter("%s: not timing out table entries", name().c_str());

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
UpdateGridRoutes::initialize(ErrorHandler *)
{
  //  ScheduleInfo::join_scheduler(this, errh);
  _hello_timer.initialize(this);
  _hello_timer.schedule_after_msec(_period); // Send periodically

  _expire_timer.initialize(this);
  if (_timeout > 0)
    _expire_timer.schedule_after_msec(EXPIRE_TIMER_PERIOD);

  _sanity_timer.initialize(this);
  _sanity_timer.schedule_after_msec(SANITY_CHECK_PERIOD); // Send periodically

  return 0;
}



Packet *
UpdateGridRoutes::simple_action(Packet *packet)
{
  /*
   * expects grid packets, with MAC hdrs
   */
  assert(packet);
  unsigned int jiff = click_jiffies();

  /*
   * Update immediate neighbor table with this packet's transmitter's
   * info.
   */
  click_ether *eh = (click_ether *) packet->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("%s: got non-Grid packet type", name().c_str());
    return packet;
  }
  grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
  IPAddress ipaddr((unsigned char *) &gh->tx_ip);
  EtherAddress ethaddr((unsigned char *) eh->ether_shost);

  if (ethaddr == _ethaddr) {
    click_chatter("%s: received own Grid packet; ignoring it", name().c_str());
    return packet;
  }

  NbrEntry *nbr = _addresses.findp(ipaddr);
  if (nbr == 0) {
    // this src addr not already in map, so add it
    NbrEntry new_nbr(ethaddr, ipaddr, jiff);
    _addresses.insert(ipaddr, new_nbr);
    click_chatter("%s: adding %s -- %s", name().c_str(), ipaddr.unparse().c_str(), ethaddr.unparse().c_str());
  }
  else {
    // update jiffies and MAC for existing entry
    nbr->last_updated_jiffies = jiff;
    if (nbr->eth != ethaddr)
      click_chatter("%s: updating %s -- %s", name().c_str(), ipaddr.unparse().c_str(), ethaddr.unparse().c_str());
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
	  else {
	    // we never had a route to this broken destination anyway
	  }

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



static String
print_rtes(Element *e, void *)
{
  UpdateGridRoutes *n = (UpdateGridRoutes *) e;

  String s;
  for (UpdateGridRoutes::FarTable::iterator iter = n->_rtes.begin(); iter.live(); iter++) {
    UpdateGridRoutes::far_entry f = iter.value();
    s += IPAddress(f.nbr.ip).unparse()
      + " next_hop=" + IPAddress(f.nbr.next_hop_ip).unparse()
      + " num_hops=" + String((int) f.nbr.num_hops)
      + " loc=" + f.nbr.loc.s()
      + " err=" + (f.nbr.loc_good ? "" : "-") + String(f.nbr.loc_err) // negate loc if invalid
      + " seq_no=" + String(f.nbr.seq_no)
      + "\n";
  }
  return s;
}

static String
print_nbrs(Element *e, void *)
{
  UpdateGridRoutes *n = (UpdateGridRoutes *) e;

  String s;
  for (UpdateGridRoutes::Table::iterator iter = n->_addresses.begin(); iter.live(); iter++) {
    s += iter.key().unparse();
    s += " eth=" + iter.value().eth.unparse();
    s += "\n";
  }
  return s;
}

static String
print_ip(Element *e, void *)
{
  UpdateGridRoutes *n = (UpdateGridRoutes *) e;
  return n->_ipaddr.unparse();
}


static String
print_eth(Element *e, void *)
{
  UpdateGridRoutes *n = (UpdateGridRoutes *) e;
  return n->_ethaddr.unparse();
}


void
UpdateGridRoutes::add_handlers()
{
  add_read_handler("nbrs", print_nbrs, 0);
  add_read_handler("rtes", print_rtes, 0);
  add_read_handler("ip", print_ip, 0);
  add_read_handler("eth", print_eth, 0);
}




void
UpdateGridRoutes::get_rtes(Vector<grid_nbr_entry> *retval)
{
  assert(retval != 0);
  expire_routes();
  for (UpdateGridRoutes::FarTable::iterator iter = _rtes.begin(); iter.live(); iter++)
    retval->push_back(iter.value().nbr);
}


void
UpdateGridRoutes::sanity_hook(Timer *, void *thunk)
{
  UpdateGridRoutes *n = (UpdateGridRoutes *) thunk;

  if (n->_num_updates_sent > SANITY_CHECK_MAX_PACKETS)
    click_chatter("%s: sent more than %d routing updates in %d milliseconds!",
		  n->name().c_str(), SANITY_CHECK_MAX_PACKETS, SANITY_CHECK_PERIOD);
  n->_num_updates_sent = 0;

  n->_expire_timer.schedule_after_msec(SANITY_CHECK_PERIOD);
}

void
UpdateGridRoutes::expire_hook(Timer *, void *thunk)
{
  UpdateGridRoutes *n = (UpdateGridRoutes *) thunk;

  // decrement the ages of the route entries
  /* why don't we do this is in expire_routes also?  i think i
     remember: because expire_routes gets called all the time,
     vs. this hook which is actually called on a time basis. */
  /* decrement age separately from expiring routes; we expire the
     routes often in private functions to clean up, and we may not
     want to adjust the age by a whole _period.  yes, basically the
     time handling is junky here */
  for (UpdateGridRoutes::FarTable::iterator iter = n->_rtes.begin(); iter.live(); iter++) {
    // XXX yucky
    *((unsigned int *) &(iter.value().nbr.age)) = decr_age(iter.value().nbr.age, EXPIRE_TIMER_PERIOD);
  }

  Vector<grid_nbr_entry> expired_info = n->expire_routes();

  if (expired_info.size() > 0) {
    // make and send the packet advertising any broken routes
    n->send_routing_update(expired_info, false);
  }

  n->_expire_timer.schedule_after_msec(EXPIRE_TIMER_PERIOD);
}


Vector<grid_nbr_entry>
UpdateGridRoutes::expire_routes()
{
  /* removes expired routes and immediate neighbor entries from the
     tables.  returns a vector of expired routes which is suitable for
     inclusion in a broken route advertisement. */

  assert(_timeout > 0);
  unsigned int jiff = click_jiffies();

  // XXX not sure if we are allowed to iterate while modifying map
  // (i.e. erasing entries), so figure out what to expire first.
  typedef HashMap<IPAddress, bool> xa_t;
  xa_t expired_addresses;
  Vector<grid_nbr_entry> expired_nbrs;

  // find the expired immediate entries
  for (UpdateGridRoutes::Table::iterator iter = _addresses.begin(); iter.live(); iter++)
    if (jiff - iter.value().last_updated_jiffies > _timeout_jiffies)
      expired_addresses.insert(iter.key(), true);

  // find expired routing entries -- either we have had the entry for
  // too long, or it has been flying around the whole network for too
  // long, or we have expired the next hop from our immediate neighbor
  for (UpdateGridRoutes::FarTable::iterator iter = _rtes.begin(); iter.live(); iter++) {
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
  for (xa_t::iterator iter = expired_addresses.begin(); iter.live(); iter++) {
    click_chatter("%s: expiring address for %s",
                  name().c_str(), iter.key().unparse().c_str());
    assert(_addresses.remove(iter.key()));
  }

  // remove expired route table entry
  for (int i = 0; i < expired_nbrs.size(); i++) {
    click_chatter("%s: expiring route entry for %s", name().c_str(), IPAddress(expired_nbrs[i].ip).unparse().c_str());
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
  for (UpdateGridRoutes::FarTable::iterator iter = n->_rtes.begin(); iter.live(); iter++) {
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
  uint32_t r2 = click_random();
  double r = (double) (r2 >> 1);
  int jitter = (int) (((double) n->_jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  n->_hello_timer.schedule_after_msec(n->_period + (int) jitter);
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
  if (p == 0) {
    click_chatter("in %s: cannot make packet!", name().c_str());
    assert(0);
  }
  ASSERT_4ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  p->set_timestamp_anno(Timestamp::now());

  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _ethaddr.data(), 6);

  grid_hdr *gh = (grid_hdr *) (eh + 1);
  ASSERT_4ALIGNED(gh);
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
EXPORT_ELEMENT(UpdateGridRoutes)
CLICK_ENDDECLS
