/*
 * floodinglocquerier.{cc,hh} -- Flooding protocol for finding Grid locations
 * Douglas S. J. De Couto
 * based on arpquerier.{cc,hh} by Robert Morris and Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "floodinglocquerier.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

#define NOISY 1

typedef GridRouteActionCallback GRCB;

FloodingLocQuerier::FloodingLocQuerier()
  : _expire_timer(expire_hook, this)
{
    // input 0: GRID_NBR_ENCAP packets
    // input 1: flooding queries and responses
    // output 0: GRID_NBR_ENCAP packets
    // output 1: flooding queries
}

FloodingLocQuerier::~FloodingLocQuerier()
{
}


int
FloodingLocQuerier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("ETH", _my_en)
	.read_mp("IP", _my_ip)
	.complete();
}

int
FloodingLocQuerier::initialize(ErrorHandler *)
{
  _expire_timer.initialize(this);
  _expire_timer.schedule_after_msec(EXPIRE_TIMEOUT_MS);
  _loc_queries = 0;
  _pkts_killed = 0;
  _timeout_jiffies = (CLICK_HZ * EXPIRE_TIMEOUT_MS) / 1000;

  return 0;
}


void
FloodingLocQuerier::expire_hook(Timer *, void *thunk)
{
  /* yes, this function won't expire entries exactly on the dot */
  FloodingLocQuerier *locq = (FloodingLocQuerier *)thunk;
  unsigned int jiff = click_jiffies();

  // flush old ``last sequence numbers''
  typedef seq_map::iterator smi_t;
  Vector<IPAddress> old_seqs;
  for (smi_t i = locq->_query_seqs.begin(); i.live(); i++) {
    if (i.key() == locq->_my_ip) // don't expire own last seq
      continue;
    if (jiff - i.value().last_response_jiffies > locq->_timeout_jiffies)
      old_seqs.push_back(i.key());
  }
  for (int i = 0; i < old_seqs.size(); i++)
    locq->_query_seqs.remove(old_seqs[i]);

  // expire stale outstanding queries and query responses
  /* XXX differentiate between stale outstanding queries, and stale
     cached query responses */
  typedef qmap::iterator qmi_t;
  Vector<IPAddress> old_entries;
  for (qmi_t i = locq-> _queries.begin(); i.live(); i++) {
    if (jiff - i.value().last_response_jiffies > locq->_timeout_jiffies) {
      old_entries.push_back(i.key());
      if (i.value().p != 0)
	i.value().p->kill();
    }
  }
  for (int i = 0; i < old_entries.size(); i++)
    locq->_queries.remove(old_entries[i]);

  locq->_expire_timer.schedule_after_msec(EXPIRE_TIMEOUT_MS);
}

void
FloodingLocQuerier::send_query_for(const IPAddress &want_ip)
{
  click_ether *e;
  grid_hdr *gh;
  grid_loc_query *fq;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*gh) + sizeof(*fq) + 2);
  if (q == 0) {
    click_chatter("in %s: cannot make packet!", name().c_str());
    assert(0);
  }
  ASSERT_4ALIGNED(q->data());
  q->pull(2);

  q->set_timestamp_anno(Timestamp::now());

  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  gh = (grid_hdr *) (e + 1);
  fq = (grid_loc_query *) (gh + 1);

  memcpy(e->ether_dhost, "\xff\xff\xff\xff\xff\xff", 6);
  memcpy(e->ether_shost, _my_en.data(), 6);
  e->ether_type = htons(ETHERTYPE_GRID);

  gh->hdr_len = sizeof(grid_hdr);
  gh->type = grid_hdr::GRID_LOC_QUERY;
  gh->ip = gh->tx_ip = _my_ip;
  gh->total_len = htons(q->length() - sizeof(click_ether));

  fq->dst_ip = want_ip;
  fq->seq_no = htonl(_loc_queries);

  // make sure we never propagate our own queries!
  _query_seqs.insert(_my_ip, seq_t(_loc_queries, click_jiffies()));

  _loc_queries++;
  output(1).push(q);
}

/* if the packet has location information already in it, just send it
 * out, ignoring the state of our location table (e.g. don't update
 * our table with the packet info, and don't update the packet with
 * any info we might have).
 *
 * otherwise....
 * If the packet's location is in the table, fill in the
 * grid_nbr_encap header and push it out.  Otherwise push out a query
 * packet.  May save the packet in the ARP table for later sending.
 * May call p->kill().  */
void
FloodingLocQuerier::handle_nbr_encap(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (eh + 1);
  grid_nbr_encap *nb = (grid_nbr_encap *) (gh + 1);

#if NOISY
  click_chatter("FloodingLocQuerier %s: got packet for %p{ip_ptr}", name().c_str(), &nb->dst_ip);
#endif

#ifdef SMALL_GRID_HEADERS
  click_chatter("FloodingLocQuerier %s: not enough info in small grid headers, dropping packet", name().c_str());
  p->kill();
  return;
#else
  // see if packet has location info in it already
  if (nb->dst_loc_good) {
#if NOISY
    click_chatter("FloodingLocQuerier %s: packet has loc info, sending immediately", name().c_str());
#endif
    // memcpy(&eh->ether_shost, _my_en.data(), 6); // LookupGeographicGridRoute does this for us
    notify_route_cbs(p, nb->dst_ip, GRCB::NoLocQueryNeeded, 0, 0);
    output(0).push(p);
    return;
  }

  // oops, no loc info, let's look it up!
  LocEntry *ae = _queries.findp(nb->dst_ip);

  if (ae != 0) {
    // we have a query entry for this destination
    if (ae->p == 0) {
#if NOISY
      click_chatter("FloodingLocQuerier %s: dest %p{ip_ptr} has good cached info, sending immediately", name().c_str(), &nb->dst_ip);
#endif
      // ae data is cached from sending last p
      WritablePacket *q = p->uniqueify();
      click_ether *eh2 = (click_ether *) q->data();
      grid_hdr *gh2 = (grid_hdr *) (eh2 + 1);
      gh2->tx_ip = _my_ip;
      grid_nbr_encap *nb2 = (grid_nbr_encap *) (gh2 + 1);
      nb2->dst_loc = ae->loc;
      nb2->dst_loc_err = htons(ae->loc_err);
      nb2->dst_loc_good = ae->loc_good;
      if (!ae->loc_good)
	click_chatter("FloodingLocQuerier %s: ``bad'' location information in table!  sending packet anyway...", name().c_str()); // XXX lame, should cache the packet and wait for some new info that is good.
      // memcpy(&eh2->ether_shost, _my_en.data(), 6);
      notify_route_cbs(p, nb->dst_ip, GRCB::CachedLocFound, 0, 0);
      output(0).push(q);
    }
    else {
      // there is a packet waiting for response; kill waiting packet,
      // then reissue query
#if NOISY
      click_chatter("FloodingLocQuerier %s: dest %s has oustanding query, killing cached packet, reissuing loc query", name().c_str(), IPAddress(nb->dst_ip).unparse().c_str());
#endif
      ae->p->kill();
      _pkts_killed++;
      ae->p = p;
      ae->last_response_jiffies = click_jiffies();
      notify_route_cbs(p, nb->dst_ip, GRCB::QueuedForLocQuery, 0, 0);
      send_query_for(nb->dst_ip);
    }
  }
  else {
    // no entry, create new entry and issue query
#if NOISY
      click_chatter("FloodingLocQuerier %s: dest %s has NO cached info, issuing loc query", name().c_str(), IPAddress(nb->dst_ip).unparse().c_str());
#endif
    LocEntry ae2;
    ae2.ip = nb->dst_ip;
    ae2.p = p;
    ae2.last_response_jiffies = click_jiffies();
    _queries.insert(nb->dst_ip, ae2);
    notify_route_cbs(p, nb->dst_ip, GRCB::QueuedForLocQuery, 0, 0);
    send_query_for(nb->dst_ip);
  }
#endif // #ifndef SMALL_GRID_HEADERS
}

/*
 * Got a loc query response.
 * Update our loc table.
 * If there was a packet waiting to be sent, send it.
 */
void
FloodingLocQuerier::handle_reply(Packet *p)
{
  if (p->length() < sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap))
    return;

  click_ether *ethh = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (ethh + 1);
  grid_nbr_encap *nb = (grid_nbr_encap *) (gh + 1);
  IPAddress ip_repl(gh->ip); // the sender of the reply
  IPAddress ip_dst(nb->dst_ip);

#if NOISY
  click_chatter("FloodingLocQuerier %s: got reply for %s from %s", name().c_str(), ip_dst.unparse().c_str(), ip_repl.unparse().c_str());
#endif

  if (ip_dst != _my_ip) {
    click_chatter("FloodingLocQuerier %s: got location query reply for %s (not for us!); check the configuration", name().c_str(), ip_repl.unparse().c_str());
    p->kill();
    return;
  }

  LocEntry *ae = _queries.findp(ip_repl);
  if (!ae) {
    click_chatter("FloodingLocQuerier %s: got location query reply from %s, but there is no entry; ignoring it", name().c_str(), ip_repl.unparse().c_str());
    p->kill();
    return;
  }

  unsigned int loc_seq_no = ntohl(gh->loc_seq_no);
  if (loc_seq_no < ae->loc_seq_no && ae->p == 0) {
    click_chatter("FloodingLocQuerier %s: got location query reply from %s with old information, ignoring it", name().c_str(), ip_repl.unparse().c_str());
    p->kill();
    return;
  }

  // fill in info (it's either newer than what we have, or we don't
  // have any yet...
  ae->loc = gh->loc;
  ae->loc_err = ntohs(gh->loc_err);
  ae->loc_good = gh->loc_good;
  ae->loc_seq_no = loc_seq_no;
  ae->last_response_jiffies = click_jiffies();

  if (!ae->loc_good)
    click_chatter("FloodingLocQuerier %s: caching bad location info from %s", name().c_str(), ip_repl.unparse().c_str());

  Packet *cached_packet = ae->p;
  ae->p = 0;

  if (cached_packet) {
#if NOISY
    click_chatter("FloodingLocQuerier %s: after reply, re-sending packet for %s", name().c_str(), ip_repl.unparse().c_str());
#endif
    handle_nbr_encap(cached_packet);
  }

  p->kill();
}

void
FloodingLocQuerier::handle_query(Packet *p)
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  grid_loc_query *lq = (grid_loc_query *) (gh + 1);

#if NOISY
  click_chatter("FloodingLocQuerier %s: got query for %s from %s (%u)", name().c_str(), IPAddress(lq->dst_ip).unparse().c_str(), IPAddress(gh->ip).unparse().c_str(), ntohl(lq->seq_no));
#endif

  if (lq->dst_ip == (unsigned int) _my_ip) {
    click_chatter("FloodingLocQuerier %s: got location query for us, but it should go to the LocQueryResponder.  Check the configuration.", name().c_str());
    p->kill();
    return;
  }
  else {
    // (possibly) propagate the query
    seq_t *seq_rec = _query_seqs.findp(gh->ip);
    unsigned int q_seq_no = ntohl(lq->seq_no);
    if (seq_rec && seq_rec->seq_no >= q_seq_no) {
#if NOISY
  click_chatter("FloodingLocQuerier %s: killing query for %s from %s (%u)", name().c_str(), IPAddress(lq->dst_ip).unparse().c_str(), IPAddress(gh->ip).unparse().c_str(), ntohl(lq->seq_no));
#endif
      // already handled this query
      p->kill();
      return;
    }
#if NOISY
    click_chatter("FloodingLocQuerier %s: propagating query for %s from %s (%u)", name().c_str(), IPAddress(lq->dst_ip).unparse().c_str(), IPAddress(gh->ip).unparse().c_str(), ntohl(lq->seq_no));
#endif
    _query_seqs.insert(gh->ip, seq_t(q_seq_no, click_jiffies()));
    WritablePacket *wp = p->uniqueify();
    click_ether *eh = (click_ether *) wp->data();
    memcpy(&eh->ether_shost, _my_en.data(), 6);
    gh = (grid_hdr *) (eh + 1);
    gh->tx_ip = _my_ip;
    // FixSrcLoc will handle the rest of the tx_* fields
    output(1).push(wp);
  }
}

void
FloodingLocQuerier::push(int port, Packet *p)
{
  if (port == 0)
    handle_nbr_encap(p);
  else {
    grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
    if (gh->type == grid_hdr::GRID_LOC_QUERY)
      handle_query(p);
    else if (gh->type == grid_hdr::GRID_LOC_REPLY) {
      handle_reply(p);
    }
    else {
      click_chatter("FloodingLocQuerier %s: got an unexpected packet type", name().c_str());
      assert(0);
    }
  }
}

String
FloodingLocQuerier::read_seqs(Element *e, void *)
{
  FloodingLocQuerier *q = (FloodingLocQuerier *)e;
  String s;

  unsigned int jiff = click_jiffies();

  typedef seq_map::iterator si_t;
  for (si_t i = q->_query_seqs.begin(); i.live(); i++) {
    const seq_t &e = i.value();
    unsigned int age = (1000 * (jiff - e.last_response_jiffies)) / CLICK_HZ;
    s += i.key().unparse() + " seq=" + String(e.seq_no) + " age=" + String(age) + "\n";
  }

  return s;
}

String
FloodingLocQuerier::read_table(Element *e, void *)
{
  FloodingLocQuerier *q = (FloodingLocQuerier *)e;
  String s;

  unsigned int jiff = click_jiffies();

  typedef qmap::iterator smi_t;
  for (smi_t i = q->_queries.begin(); i.live(); i++) {
    const LocEntry &e = i.value();
    unsigned int age = (1000 * (jiff - e.last_response_jiffies)) / CLICK_HZ;
    if (e.p == 0) {
      char locbuf[255];
      snprintf(locbuf, sizeof(locbuf), " lat_ms=%d lon_ms=%d h_mm=%d", (int) e.loc.lat_ms(), (int) e.loc.lon_ms(), (int) e.loc.h_mm());
      s += e.ip.unparse() + String(locbuf)
	+ " seq=" + String(e.loc_seq_no) + " age=" + String(age) + "\n";
    }
    else
      s += e.ip.unparse() + " <no loc yet> age=" + String(age) + "\n";
  }
  return s;
}

static String
FloodingLocQuerier_read_stats(Element *e, void *)
{
  FloodingLocQuerier *q = (FloodingLocQuerier *)e;
  return
    String(q->_pkts_killed) + " packets killed\n" +
    String(q->_loc_queries) + " loc queries sent\n";
}

void
FloodingLocQuerier::add_handlers()
{
  add_read_handler("table", read_table, 0);
  add_read_handler("queries", read_seqs, 0);
  add_read_handler("stats", FloodingLocQuerier_read_stats, 0);
}

EXPORT_ELEMENT(FloodingLocQuerier)
CLICK_ENDDECLS
