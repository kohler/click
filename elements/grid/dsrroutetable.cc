/*
 * dsrroutetable.{cc,hh}
 * Shweta Bhandare, Sagar Sanghani, Sheetalkumar Doshi, Timothy X Brown
 * Daniel Aguayo
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 * Copyright (c) 2003 University of Colorado at Boulder
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
#include <click/package.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include "dsrroutetable.hh"
#include "dsr.hh"
#include "gridgenericmetric.hh"

CLICK_DECLS

#define DEBUG_CHATTER(arg, ...)  do { if (_debug) click_chatter(arg, ## __VA_ARGS__); } while (0)


DSRRouteTable::DSRRouteTable()
  : _rreq_expire_timer(static_rreq_expire_hook, this),
    _rreq_issue_timer(static_rreq_issue_hook, this),
    _sendbuffer_timer(static_sendbuffer_timer_hook, this),
    _sendbuffer_check_routes(false),
    _blacklist_timer(static_blacklist_timer_hook, this),
    _outq(0), _metric(0), _use_blacklist(true),
    _debug(false)
{
  me = new IPAddress;

  _rreq_id = Timestamp::now().sec() & 0xffff;

  // IP packets - input 0
  // incoming DSR packets - input 1
  // packets for which tx failed - input 2

  // IP packets for the kernel - output 0
  // DSR routing packets - output 1
  // DSR data packets - output 2
}

DSRRouteTable::~DSRRouteTable()
{
  flush_sendbuffer();
  uninitialize();

  for (FWReqIter i = _forwarded_rreq_map.begin(); i.live(); i++) {
    ForwardedReqVal &frv = i.value();
    if (frv.p) frv.p->kill();
    frv.p = NULL;
  }

  delete me;
}

int
DSRRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  // read the parameters from a configuration string
  if (Args(conf, this, errh)
      .read_mp("IP", *me)
      .read_mp("LINKTABLE", reinterpret_cast<Element *&>(_link_table))
      .read("OUTQUEUE", reinterpret_cast<Element *&>(_outq))
      .read("METRIC", reinterpret_cast<Element *&>(_metric))
      .read("USE_BLACKLIST", _use_blacklist)
      .read("DEBUG", _debug)
      .complete()<0)
      return -1;

  if (_outq && _outq->cast("SimpleQueue") == 0)
    return errh->error("OUTQUEUE element is not a SimpleQueue");

  if (_metric && _metric->cast("GridGenericMetric") == 0)
    return errh->error("METRIC element is not a GridGenericMetric");

  return 0;
}

void
DSRRouteTable::flush_sendbuffer()
{
  for (SBMapIter i=_sendbuffer_map.begin(); i.live(); i++) {
    SendBuffer &sb = i.value();
    for (int j = 0; j < sb.size(); j++) {
      sb[j].check();
      sb[j]._p->kill();
    }
    sb = SendBuffer();
  }
}

void
DSRRouteTable::check()
{
  assert(me);

  // _link_table
  assert(_link_table);

  // _blacklist
  for (BlacklistIter i = _blacklist.begin(); i.live(); i++)
    i.value().check();

  // _sendbuffer_map
  for (SBMapIter i=_sendbuffer_map.begin(); i.live(); i++) {
    SendBuffer &sb = i.value();
    for (int j = 0; j < sb.size(); j++) {
      sb[j].check();
    }
  }

  // _forwarded_rreq_map
  for (FWReqIter i = _forwarded_rreq_map.begin(); i.live(); i++)
    i.value().check();

  // _initiated_rreq_map
  for (InitReqIter i = _initiated_rreq_map.begin(); i.live(); i++)
    i.value().check();

  // _rreq_expire_timer;
  assert(_rreq_expire_timer.scheduled());

  // _rreq_issue_timer;
  assert(_rreq_issue_timer.scheduled());

  // _sendbuffer_timer;
  assert(_sendbuffer_timer.scheduled());

  // _blacklist_timer;
  assert(_blacklist_timer.scheduled());
}

int
DSRRouteTable::initialize(ErrorHandler *)
{
  // expire entries on list of rreq's we have seen
  _rreq_expire_timer.initialize(this);
  _rreq_expire_timer.schedule_after_msec(DSR_RREQ_EXPIRE_TIMER_INTERVAL);

  // expire packets in the sendbuffer
  _sendbuffer_timer.initialize(this);
  _sendbuffer_timer.schedule_after_msec(DSR_SENDBUFFER_TIMER_INTERVAL);

  // check if it's time to reissue a route request
  _rreq_issue_timer.initialize(this);
  _rreq_issue_timer.schedule_after_msec(DSR_RREQ_ISSUE_TIMER_INTERVAL);

  _blacklist_timer.initialize(this);
  _blacklist_timer.schedule_after_msec(DSR_BLACKLIST_TIMER_INTERVAL);

  return 0;
}

void
DSRRouteTable::uninitialize()
{
  _rreq_expire_timer.unschedule();
  _rreq_issue_timer.unschedule();
  _sendbuffer_timer.unschedule();
  _blacklist_timer.unschedule();
}

//
// functions to handle expiration of rreqs in our forwarded_rreq map
//
void
DSRRouteTable::static_rreq_expire_hook(Timer *, void *v)
{
  DSRRouteTable *r = (DSRRouteTable *)v;
  r->rreq_expire_hook();
}
void
DSRRouteTable::rreq_expire_hook()
{
  // iterate over the _forwarded_rreq_map and remove old entries.

  // also look for requests we're waiting to forward (after a
  // unidirectionality test) and if the last hop is not in the
  // blacklist, then forward it.  if it's been a while since we issued
  // the direct request, kill the packet.
  //
  // give up on the unidirectionality test (and kill the packet) after
  // DSR_RREQ_UNITEST_TIMEOUT.  otherwise we might end up forwarding
  // the request long after it came to us.

  // we also kill the packet if we positively determined
  // unidirectionality -- e.g. if we get a tx error forwarding a route
  // reply in the meantime.

  Timestamp curr_time = Timestamp::now();

//   click_chatter("checking\n");

  Vector<ForwardedReqKey> remove_list;
  for (FWReqIter i = _forwarded_rreq_map.begin(); i.live(); i++) {

    ForwardedReqVal &val = i.value();

    if (val.p != NULL) { // we issued a unidirectionality test
      DSRRoute req_route = extract_request_route(val.p);

      IPAddress last_forwarder = req_route[req_route.size() - 1].ip();
      EtherAddress last_eth = last_forwarder_eth(val.p);

      int status = check_blacklist(last_forwarder);
      if (status == DSR_BLACKLIST_NOENTRY) { // reply came back
	DEBUG_CHATTER(" * unidirectionality test succeeded; forwarding route request\n");
        forward_rreq(val.p);

	// a) we cannot issue a unidirectionality test if there is an
	// existing metric
	//
	// b) if, after we issue a test, this RREQ comes over a
	// different link, with a valid metric, and we forward it,
	// then we essentially cancel the unidirectionality test.
	//
	// so we know that if the test comes back positive we can just
	// just calculate the route metric and call that best.
	req_route.push_back(DSRHop(*me, get_metric(last_eth)));
	val.best_metric = route_metric(req_route);

        val.p = NULL;
      } else if ((status == DSR_BLACKLIST_UNI_PROBABLE) ||
		 (diff_in_ms(curr_time, val._time_unidtest_issued) > DSR_BLACKLIST_UNITEST_TIMEOUT)) {
	DEBUG_CHATTER(" * unidirectionality test failed; killing route request\n");
	val.p->kill();
	val.p = NULL;
      }
    }

//     click_chatter("i.key is %s %s %d %d\n", i.key()._src.unparse().c_str(),
//		  i.key()._target.unparse().c_str(), i.key()._id,
//		  diff_in_ms(curr_time, i.value()._time_forwarded));

    if (diff_in_ms(curr_time, i.value()._time_forwarded) > DSR_RREQ_TIMEOUT) {
      IPAddress src(i.key()._src);
      IPAddress dst(i.key()._target);
      unsigned int id = i.key()._id;
      DEBUG_CHATTER("RREQ entry has expired; %s -> %s (%d)\n",
		    src.unparse().c_str(), dst.unparse().c_str(), id);

      remove_list.push_back(i.key());
    }
  }
  for (int i = 0; i < remove_list.size(); i++) {
    //    click_chatter("XXX removing from forwarded rreq map\n");
    _forwarded_rreq_map.remove(remove_list[i]);
  }

//   click_chatter("exiting %d\n", DSR_RREQ_EXPIRE_TIMER_INTERVAL);
  _rreq_expire_timer.schedule_after_msec(DSR_RREQ_EXPIRE_TIMER_INTERVAL);

  check();

}


//
// functions to manage the sendbuffer
//
void
DSRRouteTable::static_sendbuffer_timer_hook(Timer *, void *v)
{
  DSRRouteTable *rt = (DSRRouteTable*)v;
  rt->sendbuffer_timer_hook();
}
void
DSRRouteTable::sendbuffer_timer_hook()
{
  //  DEBUG_CHATTER ("checking sendbuffer\n");

  Timestamp curr_time = Timestamp::now();

  int total = 0; // total packets sent this scheduling
  bool check_next_time = false;

  for (SBMapIter i=_sendbuffer_map.begin(); i.live(); i++) {

    SendBuffer &sb = i.value();
    IPAddress dst = i.key();

    if (! sb.size()) {
      // DEBUG_CHATTER(" * send buffer for destination %s is empty\n", dst.unparse().c_str());
      continue;
    }

    if (_sendbuffer_check_routes) {  // have we received a route reply recently?

      // XXX should we check for routes for packets in the sendbuffer
      // every time we update the link cache (i.e. if we forward
      // someone else's route request/reply and add new entries to our
      // cache?  right now we only check if we receive a route reply
      // directed to us.

      DEBUG_CHATTER(" * send buffer has %d packet%s with destination %s\n",
		    sb.size(),
		    sb.size() == 1 ? "" : "s",
		    dst.unparse().c_str());

      // search route for destination in the link cache first
      _link_table->dijkstra(false);
      Vector<IPAddress> route = _link_table->best_route(dst, false);

      if (route.size() > 1) { // found the route..
	DEBUG_CHATTER(" * have a route:\n");

	for (int j=0; j<route.size(); j++)
	  DEBUG_CHATTER(" - %d  %s \n",
			j, route[j].unparse().c_str());

	if (total < DSR_SENDBUFFER_MAX_BURST) {

	  int k;
	  for (k = 0; k < sb.size() && total < DSR_SENDBUFFER_MAX_BURST; k++, total++) {
	    // send out each of the buffered packets
	    Packet *p = sb[k]._p;
	    Packet *p_out = add_dsr_header(p, route);
	    output(2).push(p_out);
	  }

	  if (k < sb.size())
	    check_next_time = true; // we still have packets with a route


	  SendBuffer new_sb;
	  for ( ; k < sb.size() ; k++) {
	    // push whatever packets we didn't send onto new_sb, then
	    // replace the existing sendbuffer
	    new_sb.push_back(sb[k]._p);
	  }

	  sb = new_sb;

	}

	// go to the next destination's sendbuffer; we don't check for
	// expired packets if there is a route for that host
	continue;

      } else {
	DEBUG_CHATTER("still no route to %s\n",
		      dst.unparse().c_str());
      }
    }

    // if we're here, either check_routes was false (and we're being
    // called because the timer expired) or there was no route

    SendBuffer new_sb;
    for (int j = 0; j < sb.size(); j++) {
      unsigned long time_elapsed = diff_in_ms(curr_time, sb[j]._time_added);

      if (time_elapsed >= DSR_SENDBUFFER_TIMEOUT) {
	DEBUG_CHATTER(" * packet %d expired in send buffer\n", j);
	sb[j]._p->kill();
      } else {
	// DEBUG_CHATTER(" * packet %d gets to stay\n", i);
	new_sb.push_back(sb[j]);
      }
    }
    sb = new_sb;
    if (sb.size() == 0) {
      // if we expire the last packet in the sendbuffer with this
      // destination, stop sending RREQs
      stop_issuing_request(dst);
    }
  }

  _sendbuffer_check_routes = check_next_time;
  _sendbuffer_timer.schedule_after_msec(DSR_SENDBUFFER_TIMER_INTERVAL);

  check();
}
void
DSRRouteTable::buffer_packet(Packet *p)
{
  IPAddress dst = IPAddress(p->dst_ip_anno());
  DEBUG_CHATTER("buffering packet for %s", dst.unparse().c_str());

  SendBuffer *sb = _sendbuffer_map.findp(dst);
  if (!sb) {
    _sendbuffer_map.insert(dst, SendBuffer());
    sb = _sendbuffer_map.findp(dst);
  }

  if (sb->size() >= DSR_SENDBUFFER_MAX_LENGTH) {
    DEBUG_CHATTER("too many packets for this host; killing\n");
    p->kill();
  } else {
    sb->push_back(p);
    DEBUG_CHATTER("%d packets for this host\n", sb->size());
  }
}


// functions to downgrade blacklist entries
void
DSRRouteTable::static_blacklist_timer_hook(Timer *, void *v)
{
  DSRRouteTable *rt = (DSRRouteTable*)v;
  rt->blacklist_timer_hook();
}
void
DSRRouteTable::blacklist_timer_hook()
{
  Timestamp curr_time = Timestamp::now();

  for (BlacklistIter i = _blacklist.begin(); i.live(); i++) {
    if ((i.value()._status == DSR_BLACKLIST_UNI_PROBABLE) &&
	(diff_in_ms(curr_time, i.value()._time_updated) > DSR_BLACKLIST_ENTRY_TIMEOUT)) {

      BlacklistEntry &e = i.value();

      DEBUG_CHATTER(" * downgrading blacklist entry for host %s\n", i.key().unparse().c_str());

      e._status = DSR_BLACKLIST_UNI_QUESTIONABLE;
    }
  }
  _blacklist_timer.schedule_after_msec(DSR_BLACKLIST_TIMER_INTERVAL);

  check();
}


void
DSRRouteTable::push(int port, Packet *p_in)
{
  const click_ip *ip = p_in->ip_header();

  if (port==0) {  // IP packet from the kernel

    IPAddress dst_addr(ip->ip_dst.s_addr);

    DEBUG_CHATTER(" * DSR (%s): got IP packet with destination is %s\n",
		  this->name().c_str(),
		  dst_addr.unparse().c_str());

    if (dst_addr == *me) { // for simpler debugging config
      // out to the kernel
      output(0).push(p_in);
      return;
    }

    _link_table->dijkstra(false);
    Vector<IPAddress> route = _link_table->best_route(dst_addr, false);
    if (route.size() > 1) {

      DEBUG_CHATTER(" * have cached route:\n");

      for (int j=0; j < route.size(); j++)
	DEBUG_CHATTER(" - %d  %s\n", j, route[j].unparse().c_str());

      // add DSR headers to packet..
      Packet *p = add_dsr_header(p_in, route);

      output(2).push(p);
      return;

    } else {

      DEBUG_CHATTER(" * don't have route to %s; buffering packet\n", dst_addr.unparse().c_str());

      buffer_packet(p_in);
      start_issuing_request(dst_addr);

      return;

    }

  } else if (port==1) { // incoming packet is a DSR packet

    const click_dsr_option *dsr_option = (const click_dsr_option *)(p_in->data() +
								    sizeof(click_ip) +
								    sizeof(click_dsr));

    if (dsr_option->dsr_type == DSR_TYPE_RREQ) {

      const click_dsr_rreq *dsr_rreq = (const click_dsr_rreq *)(p_in->data() +
								sizeof(click_ip) +
								sizeof(click_dsr));
      unsigned src = ip->ip_src.s_addr;
      IPAddress src_addr(src);
      IPAddress dst_addr(dsr_rreq->target.s_addr);

      DEBUG_CHATTER(" * DSR (%s): got route request for destination %s\n",
		    this->name().c_str(),
		    dst_addr.unparse().c_str());

      // add the info from the RREQ to the linkcache
      DSRRoute request_route = extract_request_route(p_in);

      // ETX: get the metric for the last hop
      EtherAddress last_eth = last_forwarder_eth(p_in);
      request_route.push_back(DSRHop(*me, get_metric(last_eth)));

      for (int j=0; j<request_route.size(); j++)
	  DEBUG_CHATTER(" - %d   %s (%d)\n",
			j, request_route[j].ip().unparse().c_str(),
			request_route[j]._metric);

      add_route_to_link_table(request_route);

      if (*me==src_addr) {
	DEBUG_CHATTER(" * I sourced this RREQ; ignore.\n");
	p_in->kill();
	return;
      } else if (*me==dst_addr) {

	// this RREQ is for me, so generate a reply.
	DSRRoute reply_route = reverse_route(request_route);

	DEBUG_CHATTER(" * Generating route reply with source route:\n");
	for (int j=0; j<reply_route.size(); j++)
	  DEBUG_CHATTER(" - %d   %s (%d)\n",
			j, reply_route[j].ip().unparse().c_str(),
			reply_route[j]._metric);

	issue_rrep(dst_addr, src_addr, request_route, reply_route);
	p_in->kill(); // kill the original RREQ

	return;

      } else {

	// this RREQ is not for me.  decide whether to forward it or just kill it.
	// reply from cache would also go here.

	if (ip->ip_ttl == 1) {
	  DEBUG_CHATTER(" * time to live expired; killing packet\n");
	  p_in->kill();
	  return;
	} // ttl is decremented in forward_rreq

	if (route_index_of(request_route, *me) != request_route.size()-1) {
	  // I'm in the route somewhere other than at the end (note
	  // that above, I appended myself)
	  DEBUG_CHATTER(" * I'm already listed; killing packet\n");
	  p_in->kill();
	  return;
	}

	// check to see if we've seen this request lately, or if this
	// one is better
	ForwardedReqKey frk(src_addr, dst_addr, ntohs(dsr_rreq->dsr_id));
	ForwardedReqVal *old_frv = _forwarded_rreq_map.findp(frk);

	// ETX:
	unsigned short this_metric = route_metric(request_route);
	if (old_frv) {
	  DEBUG_CHATTER(" * already forwarded this route request (%d, %d)\n",
			this_metric, old_frv->best_metric);
	  if (metric_preferable(this_metric, old_frv->best_metric))
	    DEBUG_CHATTER(" * but this one is better\n");
	  else
	    DEBUG_CHATTER(" * and this one's not as good\n");
	}

	if (old_frv && ! metric_preferable(this_metric, old_frv->best_metric)) {
	  DEBUG_CHATTER(" * already forwarded this route request\n");

	  p_in->kill();
	  return;
	} else {

	  // we have not seen this request before, or this one is
	  // 'better'; before we do the actual forward, check
	  // blacklist for the node from which we are receiving this.

	  ForwardedReqVal new_frv; // new entry for _forwarded_rreq_map

	  Timestamp current_time = Timestamp::now();
	  new_frv._time_forwarded = current_time;

	  IPAddress last_forwarder = IPAddress(DSR_LAST_HOP_IP_ANNO(p_in));
	  // or:
	  //	  IPAddress last_forwarder = request_route[request_route.size()-2];

	  // click_chatter ("last_forwarder is %s\n", last_forwarder.unparse().c_str());

	  int status = check_blacklist(last_forwarder);

	  if (status == DSR_BLACKLIST_UNI_PROBABLE) {
	    DEBUG_CHATTER(" * request came over a unidirectional link; killing\n");
	    p_in->kill();
	    return;
	  } else if (status == DSR_BLACKLIST_UNI_QUESTIONABLE) {

	    if (old_frv) {
	      // if we're here, then we've already forwarded this
	      // request, but this one is better.  however, we need to
	      // issue a unidirectionality test for this link.

	      // XXX this is incorrect behavior: we don't bother with
	      // the unidirectionality test
	      DEBUG_CHATTER(" * link may be unidirectional but unidirectionality test is already issued;\n");
	      DEBUG_CHATTER(" * dropping this packet instead...\n");
	      p_in->kill();
	      return;
	    }

	    DEBUG_CHATTER(" * link may be unidirectional; sending out 1-hop RREQ\n");

	    // send unicast route request with TTL of 1
	    issue_rreq(last_forwarder, 1, true);
	    new_frv.p = p_in;
	    new_frv._time_unidtest_issued = current_time;

	    // while we're waiting for the test result, don't update the metric
	    // if (old_frv)
	    //   new_frv.best_metric = old_frv->best_metric;
	    // else
	    new_frv.best_metric = DSR_INVALID_ROUTE_METRIC;

	    _forwarded_rreq_map.insert(frk, new_frv);
	    return;

	  } else {

	    if (old_frv && old_frv->p) {
	      // if we're here, then we've already forwarded this
	      // request, but this one is better and we want to
	      // forward it.  however, we've got a pending
	      // unidirectionality test for this RREQ.

	      // what we should do is maintain a list of packet *'s
	      // that we've issued tests for.

	      // XXX instead, we just give up on the potentially
	      // asymmetric link.  whether or not the test comes back,
	      // things should be ok.  nonetheless this is incorrect
	      // behavior.
	      old_frv->p->kill();
	      old_frv->p = NULL;
	    }

	    DEBUG_CHATTER(" * forwarding this RREQ\n");
	    new_frv.p = NULL;
	    new_frv.best_metric = this_metric;
	    _forwarded_rreq_map.insert(frk, new_frv);
	    forward_rreq(p_in);

	    return;

	  }
	}
      }

    } else if (dsr_option->dsr_type == DSR_TYPE_RREP) {

      // process an incoming route request.  if it's for us, issue a reply.
      // if not, check for an entry in the request table, and insert one and
      // forward the request if there is not one.

      IPAddress dst_addr(ip->ip_dst.s_addr);

      // extract the reply route..  convert to node IDs and add to the
      // link cache
      DSRRoute reply_route = extract_reply_route(p_in);

      // XXX really, is this necessary?  or are we only potentially
      // making the link data more stale, while marking it as current?
      add_route_to_link_table(reply_route);

      DEBUG_CHATTER(" * DSR (%s): received route reply with reply route:\n",
		    this->name().c_str());
      for (int i=0; i<reply_route.size(); i++)
	DEBUG_CHATTER(" - %d  %s (%d)\n",
		      i,
		      reply_route[i].ip().unparse().c_str(),
		      reply_route[i]._metric);

      // now check for packets in the sendbuffer whose destination has
      // been found using the information from the route reply
      _sendbuffer_check_routes = true;
      _sendbuffer_timer.schedule_now();

      // remove the last forwarder from the blacklist, if present
      IPAddress last_forwarder = IPAddress(DSR_LAST_HOP_IP_ANNO(p_in));
      // click_chatter ("last_forwarder is %s\n", last_forwarder.unparse().c_str());

	// last_sr_hop(p_in,
	// (sizeof(click_ip)+
	//  sizeof(click_dsr)+
	//  sizeof(click_dsr_rrep)+
	//  sizeof(in_addr) * (reply_route.size()-1)));
      set_blacklist(last_forwarder, DSR_BLACKLIST_NOENTRY);

      if (dst_addr==*me) {
	// the first address listed in the route reply's route must be
	// the destination which we queried; this is not necessarily
	// the same as the destination in the IP header because we
	// might be doing reply-from-cache
	IPAddress reply_dst = reply_route[reply_route.size()-1].ip();
	DEBUG_CHATTER(" * killed (route to %s reached final destination, %s)\n",
		      reply_dst.unparse().c_str(), dst_addr.unparse().c_str());
	stop_issuing_request(reply_dst);
	p_in->kill();
	return;
      } else {
	DEBUG_CHATTER(" * forwarding towards destination %s\n", dst_addr.unparse().c_str());
	forward_rrep(p_in); // determines next hop, sets dest ip anno, and then pushes out to arp table.
	return;
      }

    } else if (dsr_option->dsr_type == DSR_TYPE_RERR) {

      DEBUG_CHATTER(" * DSR (%s): got route error packet\n",
		    this->name().c_str());

      // get a pointer to the route error header
      const click_dsr_rerr *dsr_rerr = (click_dsr_rerr *)dsr_option;

      assert(dsr_rerr->dsr_error == DSR_RERR_TYPE_NODE_UNREACHABLE); // only handled type right now

      const in_addr *unreachable_addr = (in_addr *)((char *)dsr_rerr + sizeof(click_dsr_rerr));

      // get the bad hops
      IPAddress err_src(dsr_rerr->dsr_err_src);
      IPAddress err_dst(dsr_rerr->dsr_err_dst);
      IPAddress unreachable_dst(unreachable_addr->s_addr);

      // now remove the entries from the linkcache
      DEBUG_CHATTER(" - removing link from %s to %s; rerr destination is %s\n",
		    err_src.unparse().c_str(), unreachable_dst.unparse().c_str(), err_dst.unparse().c_str());

      // XXX DSR_INVALID_HOP_METRIC isn't really an appropriate name here
      _link_table->update_both_links(err_src, unreachable_dst, 0, 0, DSR_INVALID_ROUTE_METRIC);

      if (err_dst == *me) {
	DEBUG_CHATTER(" * killed (reached final destination)\n");
	p_in->kill();
      } else {
	forward_rerr(p_in);
      }

      // find packets with this link in their source route and yank
      // them out of our outgoing queue
      if (_outq) {
	Vector<Packet *> y;
	_outq->yank(link_filter(err_src, unreachable_dst), y);
	DEBUG_CHATTER("yanked %d packets; killing...\n", y.size());
	for (int i = 0; i < y.size(); i++)
	  y[i]->kill();
      }

      return;

    } else if (dsr_option->dsr_type == DSR_TYPE_SOURCE_ROUTE) {

      // this is a source-routed data packet

      unsigned ip_dst = ip->ip_dst.s_addr;
      IPAddress dst_addr(ip_dst);

      DEBUG_CHATTER(" * DSR (%s): incoming data pkt for %s; dsr_type is %d\n",
		    this->name().c_str(),
		    dst_addr.unparse().c_str(), dsr_option->dsr_type);

      // remove the last forwarder from the blacklist, if present
      IPAddress last_forwarder = IPAddress(DSR_LAST_HOP_IP_ANNO(p_in));
	                        //  last_sr_hop(p_in,
				//	        sizeof(click_ip)+sizeof(click_dsr));
      set_blacklist(last_forwarder, DSR_BLACKLIST_NOENTRY);
      // click_chatter ("last_forwarder is %s\n", last_forwarder.unparse().c_str());

      if (dst_addr == *me) {
	Packet *p = strip_headers(p_in);
	// out to the kernel
	output(0).push(p);
	return;
      } else {
	// DEBUG_CHATTER("need to forward\n",dst_addr.unparse().c_str());

	// determines next hop, sets dest ip anno, and then pushes out to arp table.
	forward_data(p_in);
	return;
      }
    } else {
      DEBUG_CHATTER("unexpected packet type %d\n", dsr_option->dsr_type);
      p_in->kill();
      return;
    }
  } else if (port == 2) {

    // source-routed packet whose transmission to the next hop failed

    // XXXXX is the IP dest annotation necessarily set here??

    IPAddress bad_src = *me;
    const click_dsr_option *dsr_option = (const click_dsr_option *)(p_in->data() +
								    sizeof(click_ip) +
								    sizeof(click_dsr));

    unsigned int offset = sizeof(click_ip) + sizeof(click_dsr);
    IPAddress bad_dst;

    if (dsr_option->dsr_type == DSR_TYPE_RREQ) {
      // if this is a RREQ, then it must be a one-hop
      // unidirectionality test, originated by me, because no other
      // RREQs are unicast.
      const click_dsr_rreq *dsr_rreq = (const click_dsr_rreq *)dsr_option;
      bad_dst = IPAddress(dsr_rreq->target);
    } else {
      if (dsr_option->dsr_type == DSR_TYPE_RREP) {
	const click_dsr_rrep *dsr_rrep = (const click_dsr_rrep *)dsr_option;
	int hop_count = dsr_rrep->num_addrs();
	// DEBUG_CHATTER ("hop count is %d\n", hop_count);
	offset += sizeof(click_dsr_rrep) + hop_count * sizeof(DSRHop);
      } else if (dsr_option->dsr_type == DSR_TYPE_RERR) {
	const click_dsr_rerr *dsr_rerr = (const click_dsr_rerr *)dsr_option;
	assert(dsr_rerr->dsr_error == DSR_RERR_TYPE_NODE_UNREACHABLE); // only supported error now
	offset += sizeof(click_dsr_rerr) + sizeof(in_addr);
      }
      bad_dst = next_sr_hop(p_in, offset);
    }

    DEBUG_CHATTER(" * packet had bad source route with next hop %s\n",
		  bad_dst.unparse().c_str());

    if (dsr_option->dsr_type == DSR_TYPE_RREP) {
      DEBUG_CHATTER(" * tx error sending route reply; adding entry to blacklist for %s\n",
		    bad_dst.unparse().c_str());
      set_blacklist(bad_dst, DSR_BLACKLIST_UNI_PROBABLE);
    } else if (dsr_option->dsr_type == DSR_TYPE_RREQ) {
      // XXX are we only supposed to set this for failed RREPs?
      DEBUG_CHATTER(" * one-hop unicast RREQ failed\n");
      set_blacklist(bad_dst, DSR_BLACKLIST_UNI_PROBABLE);
    }

    _link_table->update_both_links(bad_src, bad_dst, 0, 0, DSR_INVALID_ROUTE_METRIC);

    const click_ip *ip = p_in->ip_header();
    unsigned src = ip->ip_src.s_addr;
    IPAddress src_addr(src);

    // if I generated the packet, then there is no need to send a route error
    if (src_addr == *me) {
      //      click_chatter(" * i was the source; killing\n");

      p_in->kill();
      return;
    } else {
      // need to send a route error
      DSRRoute source_route, trunc_route, rev_route;

      // send RERR back along its original source route
      source_route = extract_source_route(p_in, offset);

      trunc_route = truncate_route(source_route, *me);
      if (! trunc_route.size()) {
	// this would suggest something is very broken
	DEBUG_CHATTER("couldn't find my address in bad source route!\n");
	return;
      }

      rev_route = reverse_route(trunc_route);
      issue_rerr(bad_src, bad_dst, src_addr, rev_route);

      // find packets with this link in their source route and yank
      // them out of our outgoing queue
      if (_outq) {
	Vector<Packet *> y;
	_outq->yank(link_filter(bad_src, bad_dst), y);
	DEBUG_CHATTER("yanked %d packets; killing...\n", y.size());
	for (int i = 0; i < y.size(); i++)
	  y[i]->kill();
      }

      //   // salvage the packet?
      //   if (dsr_option->dsr_type == DSR_TYPE_RREP) {
      //	// we don't salvage replies
      //	p_in->kill();
      //	return;
      //   } else if (dsr_option->dsr_type == DSR_TYPE_RREQ) {
      //	// unicast route request must be from me... this case should
      //	// never happen.
      //	assert(0);
      //	return;
      //   } else if (dsr_option->dsr_type == DSR_TYPE_RERR) {
      //	// ah, i don't know.  this is complicated.  XXX
      //   } else if (dsr_option->dsr_type == DSR_TYPE_SOURCE_ROUTE) {
      //	salvage(p_in);
      //	return;
      //   }

      p_in->kill();
      return;

    }
  }
  assert(0);
}

Packet *
DSRRouteTable::add_dsr_header(Packet *p_in, const Vector<IPAddress> &source_route)
{
  int old_len = p_in->length();
  int payload;
  int i;

  // the source and destination addresses are not included
  // as hops in the source route
  assert(source_route.size() >= 2);
  int hop_count = source_route.size() - 2;

  payload = (sizeof(click_dsr) +
	     sizeof(click_dsr_source) +
	     hop_count * sizeof(DSRHop));

  DEBUG_CHATTER(" * creating DSR source-routed packet\n");

  // save the IP header
  click_ip *ip = (click_ip *)(p_in->data());
  click_ip old_ip;
  memcpy(&old_ip, ip, sizeof(click_ip));   //copy the old header

  // add the extra header size and get a new packet
  WritablePacket *p = p_in->push(payload);
  if (!p) {
    click_chatter("couldn't add space for new DSR header\n");
    return p;
  }

  ip = (click_ip *)(p->data());
  memcpy(ip, &old_ip, sizeof(click_ip));

  // add the fixed header
  click_dsr *dsr = (click_dsr *)(p->data() + sizeof(click_ip));

  dsr->dsr_next_header = ip->ip_p; // save IP protocol type
  ip->ip_p = IP_PROTO_DSR; // set new protocol type to DSR
  dsr->dsr_len = htons(payload - sizeof(click_dsr));
  dsr->dsr_reserved = 0;

  DEBUG_CHATTER(" * add_dsr_header: new packet size is %d, old was %d \n", p->length(), old_len);

  ip->ip_len = htons(p->length());
  ip->ip_dst.s_addr = (unsigned)p->dst_ip_anno(); // XXX not sure I understand why we need to reset this
  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));

  p->set_ip_header(ip, sizeof(click_ip));

  // add the source option
  click_dsr_source *dsr_source=(click_dsr_source *)(p->data()+sizeof(click_ip)+sizeof(click_dsr));

  dsr_source->dsr_type = DSR_TYPE_SOURCE_ROUTE;
  dsr_source->dsr_len = sizeof(DSRHop) * hop_count + 2;
  dsr_source->dsr_segsleft = hop_count;

  for (i=0; i<hop_count; i++) {
    dsr_source->addr[i]._ip.s_addr = source_route[i+1].addr();
    dsr_source->addr[i]._metric = 0; // to be filled in along the way
  }

  // set the ip dest annotation to the next hop
  p->set_dst_ip_anno(source_route[hop_count].addr());

  DEBUG_CHATTER(" * added source route header");

  return p;
}

// removes all DSR headers, leaving an ordinary IP packet
Packet *
DSRRouteTable::strip_headers(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  click_dsr *dsr = (click_dsr *)(p->data() + sizeof(click_ip));

  assert(ip->ip_p == IP_PROTO_DSR);

  // get the length of the DSR headers from the fixed header
  unsigned dsr_len = sizeof(click_dsr) + ntohs(dsr->dsr_len);

  // save the IP header
  click_ip new_ip;
  memcpy(&new_ip, ip, sizeof(click_ip));
  new_ip.ip_p = dsr->dsr_next_header;

  // remove the headers
  p->pull(dsr_len);
  memcpy(p->data(), &new_ip, sizeof(click_ip));
  ip=reinterpret_cast<click_ip *>(p->data());
  ip->ip_len=htons(p->length());
  ip->ip_sum=0;
  ip->ip_sum=click_in_cksum((unsigned char *)ip,sizeof(click_ip));

  p->set_ip_header((click_ip*)p->data(),sizeof(click_ip));

  DEBUG_CHATTER(" * stripping headers; removed %d bytes\n", dsr_len);

  return p;
}

// takes a RREQ packet, and returns the route which that request has
// so far accumulated.  (we then take this route and add it to the
// link cache)
DSRRoute
DSRRouteTable::extract_request_route(const Packet *p_in)
{
  const click_ip *ip = p_in->ip_header();

  // address of the node originating the rreq
  unsigned src = ip->ip_src.s_addr;
  IPAddress src_addr(src);

  const click_dsr_rreq *dsr_rreq = (const click_dsr_rreq *)(p_in->data()+
							    sizeof(click_ip)+
							    sizeof(click_dsr));
  assert(dsr_rreq->dsr_type == DSR_TYPE_RREQ);

  int num_addr = dsr_rreq->num_addrs();
  //  DEBUG_CHATTER(" * hop count in RREQ so far is %d\n", num_addr);

  // route is { ip src, addr[0], addr[1], ..., ip dst }
  DSRRoute route;
  route.push_back(DSRHop(src_addr));

  for (int i=0; i<num_addr; i++) {
    route.push_back(dsr_rreq->addr[i]);
  }

  //  IPAddress dst_addr(dsr_rreq->target.s_addr);
  //  route.push_back(dst_addr);
  return route;
}

void
DSRRouteTable::issue_rrep(IPAddress src, IPAddress dst,
			  DSRRoute reply_route,
			  DSRRoute source_route)
{
  // exclude src and dst in source route for hop count
  unsigned char src_hop_count = source_route.size() - 2;

  // however the reply route has to include the destination.  the
  // source is in the IP dest field
  unsigned char reply_hop_count = reply_route.size() - 1;

  // creating the payload
  int payload = (sizeof(click_ip) + sizeof(click_dsr) +
		 sizeof(click_dsr_rrep)  +
		 sizeof(DSRHop) * reply_hop_count +
		 sizeof(click_dsr_source) +
		 sizeof(DSRHop) * src_hop_count);

  int i;
  // XXX?
  unsigned ttl=255;

  WritablePacket *p = Packet::make(payload);

  if (!p) {
    DEBUG_CHATTER(" * issue_rrep: couldn't make packet of %d bytes\n", payload);
    return;
  }

  // getting pointers to the headers

  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  click_dsr *dsr = (click_dsr *)(p->data() + sizeof(click_ip));
  click_dsr_rrep *dsr_rrep = (click_dsr_rrep *)( p->data() +
						 sizeof(click_ip) +
						 sizeof(click_dsr));
  click_dsr_source *dsr_source = (click_dsr_source *)( p->data() +
						       sizeof(click_ip) +
						       sizeof(click_dsr) +
						       sizeof(click_dsr_rrep) +
						       sizeof(DSRHop) * reply_hop_count);

  p->set_ip_header(ip, sizeof(click_ip));

  /* fill the source route header */
  dsr_source->dsr_type = DSR_TYPE_SOURCE_ROUTE;
  dsr_source->dsr_len = sizeof(DSRHop) * src_hop_count + 2;
  dsr_source->dsr_segsleft = src_hop_count;

  if (src_hop_count != 0) {
    // fill up the source route header using the source_route
    // addresses, omitting first and last addresses
    for (i=1; i<source_route.size()-1; i++) {
      dsr_source->addr[i-1]._ip = source_route[i]._ip;
      dsr_source->addr[i-1]._metric = 0; // to be filled in along the way
    }
  }

  // fill the route reply header
  dsr_rrep->dsr_type = DSR_TYPE_RREP;
  dsr_rrep->dsr_len = sizeof(DSRHop)*reply_hop_count+1;
  dsr_rrep->dsr_flags = 0;
  //  dsr_rrep->dsr_id = htons(id);

  // fill the route reply addrs
  for (i=1; i<reply_route.size(); i++) {
    // skip first address, which goes in the dest field of the ip
    // header.
    dsr_rrep->addr[i-1]._ip = reply_route[i]._ip;
    dsr_rrep->addr[i-1]._metric = reply_route[i]._metric;
  }

  // make the IP Header
  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  //  ip->ip_id = htons(id); // XXXXX
  ip->ip_p = IP_PROTO_DSR;
  ip->ip_src.s_addr = src.addr();
  ip->ip_dst.s_addr = dst.addr();
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = ttl;
  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));

  // fill the dsr header
  dsr->dsr_next_header = ip->ip_p;
  dsr->dsr_len = htons(payload - sizeof(click_dsr) - sizeof(click_ip));
  dsr->dsr_reserved = 0;

  // setting the next hop annotation
  p->set_dst_ip_anno(source_route[1]._ip);
  IPAddress dst_anno_address(p->dst_ip_anno());
  DEBUG_CHATTER(" * created RREP packet with next hop %s\n", dst_anno_address.unparse().c_str());

  output(1).push(p);
}

// takes info about a bad link from bad_src -> bad_dst, and the
// originator of the packet which failed on this link, and sends a
// route error along the provided source_route
void
DSRRouteTable::issue_rerr(IPAddress bad_src, IPAddress bad_dst, IPAddress src,
			  DSRRoute source_route)
{
  WritablePacket *p = NULL;

  // exclude src and dst in source route for hop count
  int src_hop_count = source_route.size()-2;
  assert(src_hop_count >= 0);

  // creating the payload
  int payload = (sizeof(click_ip) +
		 sizeof(click_dsr) +
		 sizeof(click_dsr_rerr)  +
		 sizeof(in_addr));

  // if the src_hop_count is 0, then no need for a source route option

  // XXX handling of rerrs with no SR option is broken; always insert SR option
  //   if (src_hop_count > 0)
    payload += (sizeof(click_dsr_source) +
		sizeof(DSRHop) * (src_hop_count));

  int i;

  // XXX?
  unsigned ttl=255;

  // make the packet
  p = Packet::make(payload);

  if (!p) {
    DEBUG_CHATTER(" * issue_rerr:  couldn't make packet of %d bytes\n", payload);
    return;
  }

  // getting pointers to the headers
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());

  click_dsr *dsr = (click_dsr *)(p->data() + sizeof(click_ip));

  click_dsr_rerr *dsr_rerr = (click_dsr_rerr *)(p->data() +
						sizeof(click_ip) +
						sizeof(click_dsr));
  in_addr *dsr_unreach_addr=(in_addr *)(p->data() +
					sizeof(click_ip) +
					sizeof(click_dsr) +
					sizeof(click_dsr_rerr));

  click_dsr_source *dsr_source = (click_dsr_source *)(p->data() +
						      sizeof(click_ip) +
						      sizeof(click_dsr) +
						      sizeof(click_dsr_rerr) +
						      sizeof(in_addr));

  p->set_ip_header(ip, sizeof(click_ip));

  // fill in the route error
  dsr_rerr->dsr_type = DSR_TYPE_RERR;
  dsr_rerr->dsr_len = 14;
  dsr_rerr->dsr_error = DSR_RERR_TYPE_NODE_UNREACHABLE;
  dsr_rerr->dsr_flags = 0;
  dsr_rerr->dsr_err_src.s_addr = bad_src.addr();
  dsr_rerr->dsr_err_dst.s_addr = src.addr();

  // add unreachable destination
  dsr_unreach_addr->s_addr = bad_dst.addr();

  // make the IP header
  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());

  // XXX what does the following comment mean?

  // id is set to 1..check what value exactly
  ip->ip_id = htons(1);
  ip->ip_p = IP_PROTO_DSR;
  ip->ip_src.s_addr = bad_src.addr();
  ip->ip_dst.s_addr = src.addr();
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = ttl;
  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));

  // fill the dsr header
  dsr->dsr_next_header = ip->ip_p;
  dsr->dsr_len = htons(payload - sizeof(click_dsr)-sizeof(click_ip));
  dsr->dsr_reserved = 0;

  // fill in the source route header

  // if (src_hop_count > 0) {
    // don't need to do this if the target is one hop away
    dsr_source->dsr_type = DSR_TYPE_SOURCE_ROUTE;
    dsr_source->dsr_len = sizeof(DSRHop) * src_hop_count + 2;
    dsr_source->dsr_segsleft = src_hop_count;

    // get a pointer to the addresses and fill them up using
    // source_route addresses
    for (i=1; i < source_route.size()-1; i++) {
      dsr_source->addr[i-1]._ip = source_route[i]._ip;
      dsr_source->addr[i-1]._metric = 0;
    }
  //  }

  // setting the next hop annotation
  p->set_dst_ip_anno(source_route[1]._ip);

  IPAddress dst_anno_address(p->dst_ip_anno());
  DEBUG_CHATTER(" * created RERR packet with next hop as %s\n",
		dst_anno_address.unparse().c_str() );

  output(1).push(p);
}


// takes an RREP packet, copies out the reply route and returns it
// as a Vector<IPAddress>
DSRRoute
DSRRouteTable::extract_reply_route(const Packet *p)
{
  const click_ip *ip = p->ip_header();
  const click_dsr_rrep *dsr_rrep = (const click_dsr_rrep *)(p->data()+
							    sizeof(click_ip)+
							    sizeof(click_dsr));
  IPAddress dest_ip(ip->ip_dst.s_addr);

  assert(dsr_rrep->dsr_type == DSR_TYPE_RREP);

  int hop_count = dsr_rrep->num_addrs();
  // DEBUG_CHATTER(" * extracting route from %d-hop route reply\n", hop_count);

  DSRRoute route;

  // construct the route from the reply addresses.
  //
  // if the route reply is the result of "reply from cache", then the
  // address in the source field of the IP header may differ from the
  // destination of the route listed.  so the last hop of the route is
  // explicitly specified in the route.
  //
  // the first hop, however, comes from the destination field of the
  // IP header (the intended recipient of this route reply).  so we
  // have to put this (dest_ip) first.

  route.push_back(DSRHop(dest_ip));

  for (int i=0; i < hop_count; i++) {
    route.push_back(dsr_rrep->addr[i]);
  }

  return route;
}

// returns the source route from this packet; used for producing route
// error messages (extract route, truncate at my address, reverse, and
// use that as the source route for the route error)
DSRRoute
DSRRouteTable::extract_source_route(const Packet *p_in, unsigned int offset)
{
  // obtain the pointers to the source route headers
  assert(offset < p_in->length());
  const click_ip *ip = p_in->ip_header();
  const click_dsr_source *dsr_source = (const click_dsr_source *)(p_in->data()+
								  offset);

  assert(dsr_source->dsr_type == DSR_TYPE_SOURCE_ROUTE);

  int source_hops = dsr_source->num_addrs();
  DSRRoute route;

  // get the source and the destination from the IP Header
  IPAddress src(ip->ip_src.s_addr);
  IPAddress dst(ip->ip_dst.s_addr);

  route.push_back(DSRHop(src));

  // append the intermediate nodes to the route
  for (int i=0; i<source_hops; i++) {
    route.push_back(dsr_source->addr[i]);
  }

  route.push_back(DSRHop(dst));
  return route;
}

// p_in is a route reply which we received because we're on its source
// route; return the packet after setting the IP dest annotation
void
DSRRouteTable::forward_rrep(Packet * p_in)
{
  WritablePacket *p=p_in->uniqueify();


  // get pointer to the rrep header
  click_dsr_rrep *dsr_rrep=(click_dsr_rrep *)(p->data()+
					      sizeof(click_ip) +
					      sizeof(click_dsr));

  // again, from the draft
  int num_addr = dsr_rrep->num_addrs();
  //  DEBUG_CHATTER(" * RREP contains %d addresses\n", num_addr);

  // XXX originally it seemed there was code here to check if there
  // was an additional, "optional" header between the RREP option and
  // the source route.  but I don't see any code to actually insert
  // this optional header, so I'm leaving this check out for now;
  // perhaps the idea is to include some data with the route requests
  // and replies?

  unsigned int dsr_source_offset = (sizeof(click_ip) +
				    sizeof(click_dsr) +
				    sizeof(click_dsr_rrep) +
				    num_addr * sizeof(DSRHop));

  forward_sr(p, dsr_source_offset, 1);
}

// forwarding a data packet based on the source route option; set the
// destination IP anno and return.
void
DSRRouteTable::forward_data(Packet *p_in)
{
  forward_sr(p_in, sizeof(click_ip) + sizeof(click_dsr), 2);
}

// take a wild guess.
void
DSRRouteTable::forward_rerr(Packet * p_in)
{
  /* Get the source route pointer */
  unsigned int dsr_source_offset = (sizeof(click_ip) +
				    sizeof(click_dsr) +
				    sizeof(click_dsr_rerr) +
				    sizeof(in_addr));

  forward_sr(p_in, dsr_source_offset, 1);
}

IPAddress
DSRRouteTable::next_hop(Packet *p)
{
  const click_ip *ip = (const click_ip*)(p->data() + sizeof(click_ether));

  click_dsr *dsr = (click_dsr *)(p->data() + sizeof(click_ip));
  const unsigned int dsr_len = dsr->dsr_len;

  click_dsr_option *dsr_option = (click_dsr_option *)(p->data() +
						      sizeof(click_ip) +
						      sizeof(click_dsr));

  if (dsr_option->dsr_type == DSR_TYPE_RREQ) {

    click_chatter("next_hop called on a RREQ?\n");
    IPAddress src_addr(0xffffffff);
    return (src_addr);

  } else if (dsr_option->dsr_type == DSR_TYPE_RREP) {

    const click_dsr_rrep *dsr_rrep = (const click_dsr_rrep*)dsr_option;

    //    DEBUG_CHATTER(" * extracting IP from route reply; num_addr is %d\n", num_addr);

    if (dsr_rrep->length() == dsr_len) {
      // if this RREP option is the only option in the header
      IPAddress dst_addr(ip->ip_dst.s_addr);
      return (dst_addr);
    } else {
      dsr_option = (click_dsr_option *)(dsr_rrep->next_option());

      if (dsr_option->dsr_type != DSR_TYPE_SOURCE_ROUTE) {
	click_chatter(" * DSRArpTable::last_hop: source route option did not follow route reply option\n");

	IPAddress zeros;
	return zeros;
      }
    }

  } else if (dsr_option->dsr_type == DSR_TYPE_RERR) {

    // XXX we might have multiple RERRs.

    const click_dsr_rerr *dsr_rerr = (click_dsr_rerr *)dsr_option;

    if (dsr_rerr->length() == dsr_len) {
      // if this RERR option is the only option in the header
      IPAddress dst_addr(ip->ip_dst.s_addr);
      return (dst_addr);
    } else {
      dsr_option = (click_dsr_option *)(dsr_rerr->next_option());

      if (dsr_option->dsr_type != DSR_TYPE_SOURCE_ROUTE) {
	click_chatter(" * source route option did not follow route error option\n");

	IPAddress zeros;
	return zeros;
      }
    }
  }

  if (dsr_option->dsr_type == DSR_TYPE_SOURCE_ROUTE) {
    // either this is a normal source-routed packet, or a RREP or RERR
    // with a source route header

    click_dsr_source *dsr_source = (click_dsr_source *)(dsr_option);
    assert(dsr_source->dsr_type == DSR_TYPE_SOURCE_ROUTE);

    unsigned char segments = dsr_source->dsr_segsleft;
    unsigned char source_hops = dsr_source->num_addrs();

    assert(segments <= source_hops);

    int index = source_hops - segments;

    if (segments == 0) { // this is the last hop
      IPAddress dst(ip->ip_dst.s_addr);
      return dst;
    } else {
      return dsr_source->addr[index-1].ip();
    }
  }

  assert(0);
  return IPAddress();
}

// returns ip of next hop on source route; split out from forward_sr
// so we can use it when generating route error messages.  offset is
// the offset of the source route option in this packet.
IPAddress
DSRRouteTable::next_sr_hop(Packet *p_in, unsigned int offset)
{
  assert(offset + sizeof(click_dsr_source) <= p_in->length());

  click_dsr_source *dsr_source = (click_dsr_source *)(p_in->data() +
						      offset);

  // click_chatter("type is %d\n", dsr_source->dsr_type);
  assert (dsr_source->dsr_type == DSR_TYPE_SOURCE_ROUTE);

  unsigned char segments = dsr_source->dsr_segsleft;
  unsigned char source_hops = dsr_source->num_addrs();

  // click_chatter("segments %02x, source_hops %02x\n", segments, source_hops);
  assert(segments <= source_hops);

  // this is the index of the address to which this packet should be forwarded
  int index = source_hops - segments;

  if (segments == 0) { // this is the last hop
    const click_ip *ip = p_in->ip_header();
    IPAddress final_dst(ip->ip_dst.s_addr);
    return final_dst;
  } else {
    return dsr_source->addr[index].ip();
  }
}

// offset is the offset of the source route option in this packet
void
DSRRouteTable::forward_sr(Packet *p_in, unsigned int offset, int port)
{
  if (offset > p_in->length()) {
    DEBUG_CHATTER(" * offset passed to forwardSRPacket is too big!  (%d > %d)\n",
		  offset, p_in->length());
    p_in->kill();
    return;
  }

  WritablePacket *p=p_in->uniqueify();

  click_dsr_source *dsr_source = (click_dsr_source *)(p->data() +
						      offset);

  if (dsr_source->dsr_type != DSR_TYPE_SOURCE_ROUTE) {
    DEBUG_CHATTER(" * source route option not found where expected in forward_sr\n");
    p->kill();
    return;
  }

  // after we forward it there will be (segsleft-1) hops left;
  dsr_source->dsr_segsleft--;
  p->set_dst_ip_anno(next_sr_hop(p, offset));

  DEBUG_CHATTER("forward_sr: forwarding to %s\n", next_sr_hop(p, offset).unparse().c_str());

  output(port).push(p);
  return;
}

// rebroadcast a route request.  we've already checked that we haven't
// seen this RREQ lately, and added the info to the forwarded_rreq_map.
void
DSRRouteTable::forward_rreq(Packet *p_in)
{
  click_dsr *orig_dsr = (click_dsr *)(p_in->data()+
				     sizeof(click_ip));
  click_dsr_rreq *orig_rreq = (click_dsr_rreq *)(p_in->data() +
						sizeof(click_ip) +
						sizeof(click_dsr));

  int hop_count = orig_rreq->num_addrs();

  assert(ntohs(orig_dsr->dsr_len) == (sizeof(click_dsr_rreq) +
				      hop_count * sizeof(DSRHop)));

  // add my address to the end of the packet
  WritablePacket *p=p_in->uniqueify();

  p = p->put(sizeof(DSRHop));

  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  click_dsr *dsr = (click_dsr *)(p->data()+
				 sizeof(click_ip));
  click_dsr_rreq *dsr_rreq = (click_dsr_rreq *)(p->data() +
						sizeof(click_ip) +
						sizeof(click_dsr));

  dsr_rreq->addr[hop_count]._ip.s_addr = me->addr();

  EtherAddress last_eth = last_forwarder_eth(p);
  dsr_rreq->addr[hop_count]._metric = get_metric(last_eth);

  dsr_rreq->dsr_len += sizeof(DSRHop);
  dsr->dsr_len = htons(ntohs(dsr->dsr_len)+sizeof(DSRHop));

  ip->ip_ttl--;
  ip->ip_len = htons(p->length());
  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));

  p->set_dst_ip_anno(0xffffffff);

  output(1).push(p);
}

// build and send out a request for the ip
void
DSRRouteTable::issue_rreq(IPAddress dst, unsigned int ttl, bool unicast)
{
  // make a route request packet with room for gratuitious route repair rerrs

  // XXX what does the above mean?  route repair rerrs??

  unsigned payload = (sizeof(click_ip)+
		      sizeof(click_dsr)+
		      sizeof(click_dsr_rreq));
  WritablePacket *p = Packet::make(payload);

  // get header pointers
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  click_dsr *dsr = (click_dsr*)(p->data() + sizeof(click_ip));
  click_dsr_rreq *dsr_rreq = (click_dsr_rreq*)(p->data() +
					       sizeof(click_ip) +
					       sizeof(click_dsr));

  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_rreq_id); // XXX eh?  why this?
  ip->ip_p = IP_PROTO_DSR;
  ip->ip_src.s_addr = me->addr();
  if (unicast)
    ip->ip_dst.s_addr = dst.addr();
  else
    ip->ip_dst.s_addr = 0xffffffff;
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = ttl;
  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));

  dsr->dsr_next_header = 0;
  dsr->dsr_len = htons(sizeof(click_dsr_rreq));
  dsr->dsr_reserved = 0;
  dsr_rreq->dsr_type = DSR_TYPE_RREQ;
  dsr_rreq->dsr_len = 6;
  dsr_rreq->dsr_id = htons(_rreq_id);
  dsr_rreq->target.s_addr = dst.addr();

  p->set_dst_ip_anno(ip->ip_dst.s_addr);

  _rreq_id++;

  output(1).push(p);
}

// start issuing requests for a host.
void
DSRRouteTable::start_issuing_request(IPAddress host)
{
  // check to see if we're already querying for this host
  InitiatedReq *r = _initiated_rreq_map.findp(host);

  if (r) {
    DEBUG_CHATTER(" * start_issuing_request:  already issuing requests for %s\n", host.unparse().c_str());
    return;
  } else {
    // send out the initial request and add an entry to the table
    InitiatedReq new_rreq(host);
    _initiated_rreq_map.insert(host, new_rreq);
    issue_rreq(host, DSR_RREQ_TTL1, false);
    return;
  }
}
// we've received a route reply.  remove the cooresponding entry from
// route request table, so we don't send out more requests
void
DSRRouteTable::stop_issuing_request(IPAddress host)
{
  InitiatedReq *r = _initiated_rreq_map.findp(host);
  if (!r) {
    DEBUG_CHATTER(" * stop_issuing_request:  no entry in request table for %s\n", host.unparse().c_str());
    return;
  } else {
    _initiated_rreq_map.remove(host);
    return;
  }
}
void
DSRRouteTable::static_rreq_issue_hook(Timer *, void *v)
{
  DSRRouteTable *r = (DSRRouteTable *)v;
  r->rreq_issue_hook();
}
void
DSRRouteTable::rreq_issue_hook()
{
  // look through the initiated rreqs and check if it's time to send
  // anything out
  Timestamp curr_time = Timestamp::now();

  // DEBUG_CHATTER("checking issued rreq table\n");

  Vector<IPAddress> remove_list;

  for (InitReqIter i = _initiated_rreq_map.begin(); i.live(); i++) {

    InitiatedReq &ir = i.value();
    assert(ir._target == i.key());

    // we could find out a route by some other means than a direct
    // RREP.  if this is the case, stop issuing requests.
    _link_table->dijkstra(false);
    Vector<IPAddress> route = _link_table->best_route(ir._target,false);
    if (route.size() > 1) { // we have a route
      remove_list.push_back(ir._target);
      continue;
    } else {
      if (diff_in_ms(curr_time, ir._time_last_issued) > ir._backoff_interval) {

	DEBUG_CHATTER("time to issue new request for host %s\n", ir._target.unparse().c_str());

	if (ir._times_issued == 1) {
	  // if this is the second request
	  ir._backoff_interval = DSR_RREQ_DELAY2;
	} else {
	  ir._backoff_interval *= DSR_RREQ_BACKOFF_FACTOR;
	  // i don't think there's any mention in the spec of a limit on
	  // the backoff, but this MAX_DELAY seems reasonable
	  if (ir._backoff_interval > DSR_RREQ_MAX_DELAY)
	    ir._backoff_interval = DSR_RREQ_MAX_DELAY;
	}
	ir._times_issued++;
	ir._time_last_issued = curr_time;
	ir._ttl = DSR_RREQ_TTL2;

	issue_rreq(ir._target, ir._ttl, false);
      }
    }
  }

  for (int j = 0 ; j < remove_list.size() ; j++) {
    _initiated_rreq_map.remove(remove_list[j]);
  }

  _rreq_issue_timer.schedule_after_msec(DSR_RREQ_ISSUE_TIMER_INTERVAL);

  check();
}


// random helper functions

DSRRoute
DSRRouteTable::reverse_route(DSRRoute r)
{
 DSRRoute rev;
 for(int i=r.size()-1; i>=0; i--) {
   rev.push_back(r[i]);
 }
 return rev;
}

DSRRoute
DSRRouteTable::truncate_route(DSRRoute r, IPAddress ip)
{
 DSRRoute t;
 for (int i=0; i < r.size(); i++) {
   t.push_back(r[i]);
   if (r[i].ip() == ip) {
     return t;
   }
 }
 return DSRRoute();
}

int
DSRRouteTable::route_index_of(DSRRoute r, IPAddress ip)
{
 for (int i=0; i<r.size(); i++) {
   if (r[i].ip() == ip)
     return i;
 }
 return -1;
}

void
DSRRouteTable::add_route_to_link_table(DSRRoute route)
{
  for (int i=0; i < route.size() - 1; i++) {
    IPAddress ip1 = route[i].ip();
    IPAddress ip2 = route[i+1].ip();

    // ETX:
    unsigned char metric = route[i+1]._metric;
    if (metric == DSR_INVALID_HOP_METRIC)
      _link_table->update_both_links(ip1, ip2, 0, 0, 9999);
    else
      _link_table->update_both_links(ip1, ip2, 0, 0, metric);

    // DEBUG_CHATTER("_link_table->update_link %s %s %d\n",
    //               route[i].unparse().c_str(), route[i+1].s().c_str(), metric);
  }
}

int
DSRRouteTable::check_blacklist(IPAddress ip)
{
  if (!_use_blacklist) return DSR_BLACKLIST_NOENTRY;

  BlacklistEntry *e = _blacklist.findp(ip);
  if (! e) {
    return DSR_BLACKLIST_NOENTRY;
  } else {
    return e->_status;
  }
}

void
DSRRouteTable::set_blacklist(IPAddress ip, int s)
{
  //  DEBUG_CHATTER ("set blacklist: %s %d\n", ip.unparse().c_str(), s);
  //  DEBUG_CHATTER ("set blacklist: %d\n", check_blacklist(ip));

  _blacklist.remove(ip);
  if (s != DSR_BLACKLIST_NOENTRY) {
    BlacklistEntry e;
    e._time_updated.assign_now();
    e._status = s;
    _blacklist.insert(ip, e);
  }

  //  DEBUG_CHATTER ("set blacklist: %d\n", check_blacklist(ip));
}

unsigned long
DSRRouteTable::diff_in_ms(const Timestamp &t1, const Timestamp &t2)
{
    Timestamp diff = t1 - t2;
    assert(diff.sec() < (Timestamp::seconds_type) ((1 << 31) / 1000));
    return diff.msecval();
}

// Ask LinkStat for the metric for the link from other to us.
// ripped off from srcr.cc
unsigned char
DSRRouteTable::get_metric(EtherAddress other)
{
#if 0
  unsigned char dft = DSR_INVALID_HOP_METRIC; // default metric
  if (_ls){
    unsigned int tau;
    Timestamp tv;
    unsigned int frate, rrate;
    bool res = _ls->get_forward_rate(other, &frate, &tau, &tv);
    if(res == false) {
      return dft;
    }
    res = _ls->get_reverse_rate(other, &rrate, &tau);
    if (res == false) {
      return dft;
    }
    if (frate == 0 || rrate == 0) {
      return dft;
    }

    // rate is 100 * recv %
    // m = 10 x 1/(fwd% x rev%)
    u_short m = 10 * 100 * 100 / (frate * (int) rrate);

    if (m > DSR_INVALID_HOP_METRIC) {
      click_chatter("DSRRouteTable::get_metric: metric too big for one byte?\n");
      return DSR_INVALID_HOP_METRIC;
    }

    return (unsigned char)m;
  }
#endif
  if (_metric) {
    GridGenericMetric::metric_t m = _metric->get_link_metric(other, false);
    unsigned char c = _metric->scale_to_char(m);
    if (!m.good() || c >= DSR_INVALID_HOP_METRIC)
      return DSR_INVALID_HOP_METRIC;
    return c;
  }
  else {
    // default to hop-count, all links have a hop-count of 1
    return 1;
  }
}

bool
DSRRouteTable::metric_preferable(unsigned short a, unsigned short b)
{
  if (!_metric)
    return (a < b); // fallback to minimum hop-count
  else if (a == DSR_INVALID_ROUTE_METRIC || b == DSR_INVALID_ROUTE_METRIC)
    return a; // make arbitrary choice
  else
    return _metric->metric_val_lt(_metric->unscale_from_char(a),
				  _metric->unscale_from_char(b));
}

unsigned short
DSRRouteTable::route_metric(DSRRoute r)
{
#if 0
  unsigned short ret = 0;
  // the metric in r[i+1] represents the link between r[i] and r[i+1],
  // so we start at 1
  for (int i = 1; i < r.size(); i++) {
    if (r[i]._metric == DSR_INVALID_HOP_METRIC)
      return DSR_INVALID_ROUTE_METRIC;
    ret += r[i]._metric;
  }
  return ret;
#endif

  if (r.size() < 2) {
    click_chatter("DSRRouteTable::route_metric: route is too short, less than two nodes?\n");
    return DSR_INVALID_ROUTE_METRIC;
  }
  if (!_metric)
    return r.size(); // fallback to hop-count

  if (r[1]._metric == DSR_INVALID_HOP_METRIC)
    return DSR_INVALID_ROUTE_METRIC;
  GridGenericMetric::metric_t m(_metric->unscale_from_char(r[1]._metric));

  for (int i = 2; i < r.size(); i++) {
     if (r[i]._metric == DSR_INVALID_HOP_METRIC)
      return DSR_INVALID_ROUTE_METRIC;
     m = _metric->append_metric(m, _metric->unscale_from_char(r[i]._metric));
  }
  if (m.good())
    return _metric->scale_to_char(m);
  else
    return DSR_INVALID_ROUTE_METRIC;
}

EtherAddress
DSRRouteTable::last_forwarder_eth(Packet *p)
{
  uint16_t d[3];
  d[0] = DSR_LAST_HOP_ETH_ANNO1(p);
  d[1] = DSR_LAST_HOP_ETH_ANNO2(p);
  d[2] = DSR_LAST_HOP_ETH_ANNO3(p);

  return (EtherAddress((unsigned char *)d));
}

ELEMENT_REQUIRES(LinkTable)
EXPORT_ELEMENT(DSRRouteTable)
CLICK_ENDDECLS
