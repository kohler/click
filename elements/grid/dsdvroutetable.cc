/*
 * dsdvroutetable.{cc,hh} -- DSDV routing element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <stddef.h>
#include <click/standard/scheduleinfo.hh>
#include <click/router.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include "dsdvroutetable.hh"
#include "timeutils.hh"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

const DSDVRouteTable::metric_t DSDVRouteTable::_bad_metric; // default metric state is ``bad''

bool
DSDVRouteTable::get_one_entry(IPAddress &dest_ip, RouteEntry &entry) 
{
  RTEntry *r = _rtes.findp(dest_ip);
  if (r == 0)
    return false;
  entry = *r;
  return true;  
}

void
DSDVRouteTable::get_all_entries(Vector<RouteEntry> &vec)
{
  for (RTIter iter = _rtes.first(); iter; iter++) {
    const RTEntry &rte = iter.value();
    vec.push_back(rte);
  }
}

DSDVRouteTable::DSDVRouteTable() : 
  GridGenericRouteTable(1, 1), _gw_info(0),
  _link_tracker(0), _link_stat(0), _log(0), 
  _seq_no(0), _bcast_count(0),
  _max_hops(3), _alpha(0.875), _wst0(6000),
  _last_periodic_update(0),
  _last_triggered_update(0), 
  _hello_timer(static_hello_hook, this),
  _log_dump_timer(static_log_dump_hook, this),
  _metric_type(MetricEstTxCount),
  _est_type(EstByMeas),
  _frozen(false)
{
  MOD_INC_USE_COUNT;
}

DSDVRouteTable::~DSDVRouteTable()
{
  MOD_DEC_USE_COUNT;

  for (TMIter i = _expire_timers.first(); i; i++) {
    if (i.value()->scheduled())
      i.value()->unschedule();
    delete i.value();
  }
  for (TMIter i = _trigger_timers.first(); i; i++) {
    if (i.value()->scheduled())
      i.value()->unschedule();
    delete i.value();
  }
  for (HMIter i = _expire_hooks.first(); i; i++) 
    delete i.value();
  for (HMIter i = _trigger_hooks.first(); i; i++) 
    delete i.value();
}

void *
DSDVRouteTable::cast(const char *n)
{
  if (strcmp(n, "DSDVRouteTable") == 0)
    return (DSDVRouteTable *) this;
  else if (strcmp(n, "GridGenericRouteTable") == 0)
    return (GridGenericRouteTable *) this;
  else
    return 0;
}

int
DSDVRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String metric("est_tx_count");
  String logfile;
  int res = cp_va_parse(conf, this, errh,
			cpUnsigned, "entry timeout (msec)", &_timeout,
			cpUnsigned, "route broadcast period (msec)", &_period,
			cpUnsigned, "route broadcast jitter (msec)", &_jitter,
			cpUnsigned, "minimum triggered update period (msec)", &_min_triggered_update_period,
			cpEthernetAddress, "source Ethernet address", &_eth,
			cpIPAddress, "source IP address", &_ip,
			cpOptional,
			cpElement, "GridGatewayInfo element", &_gw_info,
			cpElement, "LinkTracker element", &_link_tracker,
			cpElement, "LinkStat element", &_link_stat,
			cpKeywords,
			"MAX_HOPS", cpUnsigned, "max hops", &_max_hops,
			"METRIC", cpString, "route metric", &metric,
			"LOG", cpElement, "GridLogger element", &_log,
			"WST0", cpUnsigned, "initial weight settling time, wst0 (msec)", &_wst0,
			"ALPHA", cpDouble, "alpha parameter for settling time computation (0 <= ALPHA <= 1)", &_alpha,
			0);

  if (res < 0)
    return res;

  if (_timeout == 0)
    return errh->error("timeout interval must be greater than 0");
  if (_period == 0)
    return errh->error("period must be greater than 0");
  // _jitter is allowed to be 0
  if (_jitter > _period)
    return errh->error("jitter is bigger than period");
  if (_max_hops == 0)
    return errh->error("max hops must be greater than 0");
  if (_alpha <= 0 || _alpha > 1) 
    return errh->error("alpha must be between 0 and 1 inclusive");

  _metric_type = check_metric_type(metric);
  if (_metric_type < 0)
    return errh->error("Unknown metric type ``%s''", metric.cc());

  if (_gw_info == 0)
    errh->warning("No GridGatewayInfo element specified, will not advertise as gateway");
  if (_link_tracker == 0 || _link_stat == 0)
    errh->warning("One or both of LinkTracker and LinkStat not specified, some metrics may not work");

  return res;
}

int
DSDVRouteTable::initialize(ErrorHandler *)
{
  _hello_timer.initialize(this);
  _hello_timer.schedule_after_ms(_period);
  _log_dump_timer.initialize(this);
  _log_dump_timer.schedule_after_ms(_log_dump_period); 

  check_invariants();

  return 0;
}

bool
DSDVRouteTable::current_gateway(RouteEntry &entry)
{
  for (RTIter i = _rtes.first(); i; i++) {
    if (i.value().is_gateway) {
      entry = i.value();
      return true;
    }
  }
  return false;
}

bool
DSDVRouteTable::est_forward_delivery_rate(const IPAddress &ip, double &rate)
{
  switch (_est_type) {
  case EstByMeas: {
    struct timeval last;
    bool res = _link_tracker ? _link_tracker->get_bcast_stat(ip, rate, last) : false;
    return res;
    break;
  }
  default:
    return false;
  }
}

bool
DSDVRouteTable::est_reverse_delivery_rate(const IPAddress &ip, double &rate)
{
  switch (_est_type) {
  case EstByMeas: {
    struct timeval last;
    RTEntry *r = _rtes.findp(ip);
    if (r == 0 || r->num_hops() > 1)
      return false;
    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res = _link_stat ? _link_stat->get_bcast_stats(r->next_hop_eth, last, window, num_rx, num_expected) : false;
    if (!res || num_expected <= 1)
      return false;
    double num_rx_ = num_rx;
    double num_expected_ = num_expected;
    if (num_rx > num_expected)
      click_chatter("WARNING: est_reverse_delivery_rate: num_rx (%d) > num_expected (%d) for %s",
		    num_rx, num_expected, r->next_hop_eth.s().cc());
    rate = (num_rx_ - 0.5) / num_expected_;
    return true;
    break;
  }
  default:
    return false;
  }
}

void
DSDVRouteTable::insert_route(const RTEntry &r, const GridLogger::reason_t why)
{
  check_invariants();
  r.check();
  
  RTEntry *old_r = _rtes.findp(r.dest_ip);
  
  // invariant check: running timers exist for all current good
  // routes.  no timers or bogus timer entries exist for bad routes.
  // hook objects exist for each timer.
  Timer **old = _expire_timers.findp(r.dest_ip);
  HookPair **oldhp = _expire_hooks.findp(r.dest_ip);
  if (old_r && old_r->good())
    assert(old && *old && (*old)->scheduled() && oldhp && *oldhp);
  else { 
    assert(old == 0);
    assert(oldhp == 0);
  }

  // get rid of old expire timer
  if (old) {
    (*old)->unschedule();
    delete *old;
    delete *oldhp;
    _expire_timers.remove(r.dest_ip);
    _expire_hooks.remove(r.dest_ip);
  }
  
  // Note: ns dsdv only schedules a timeout for the sender of each
  // route ad, relying on the next-hop expiry logic to get all routes
  // via that next hop.  However, that won't work for general metrics,
  // so we install a timeout for *every* newly installed good route.
  if (r.good()) {
    HookPair *hp = new HookPair(this, r.dest_ip);
    Timer *t = new Timer(static_expire_hook, (void *) hp);
    t->initialize(this);
    t->schedule_after_ms(min(r.ttl, _timeout));
    
    _expire_timers.insert(r.dest_ip, t);
    _expire_hooks.insert(r.dest_ip, hp);
  }

  _rtes.insert(r.dest_ip, r);

  // note, we don't change any pending triggered update for this
  // updated dest.  ... but shouldn't we postpone it?  -- shouldn't
  // matter if timer fires too early, since the advertise_ok_jiffies
  // should tell us it's too early.

  if (_log)
    _log->log_added_route(why, make_generic_rte(r), (unsigned int) r.wst);
  check_invariants();
}

void
DSDVRouteTable::expire_hook(const IPAddress &ip)
{
  check_invariants(&ip);

  // invariant check:
  // 1. route to expire should exist
  RTEntry *r = _rtes.findp(ip);
  assert(r != 0);
  
  // 2. route to expire should be good
  assert(r->good() && (r->seq_no() & 1) == 0);
  
  // 3. the expire timer for this dest should exist, but should not be
  // running.
  Timer **old = _expire_timers.findp(ip);
  HookPair **oldhp = _expire_hooks.findp(ip);
  assert(old && *old && !(*old)->scheduled() && oldhp && *oldhp);

  if (_log) {
    timeval tv;
    gettimeofday(&tv, NULL);
    _log->log_start_expire_handler(tv);
    _log->log_expired_route(GridLogger::TIMEOUT, ip);
  }

  // Note that for metrics other than hopcount the loop (copied from
  // the dsdv ns code) may not actually expire the dest (ip) whose
  // timeout called this hook.  That's because a node may not be its
  // own next hop in that case.  So we explicitly note that ip is
  // expired before the loop.

  Vector<IPAddress> expired_dests;
  expired_dests.push_back(ip);
  
  // Do next-hop expiration checks, but only if this node is actually
  // its own next hop.  With hopcount, it is always the case that if a
  // node n1 is some node n2's next hop, then n1 is its own next hop;
  // therefore the if() will be true.  For other metrics, there can be
  // cases where we should not expire n2 just because n1 expires, if
  // n1 is not its own next hop.
  if (r->num_hops() == 1) {
    assert(r->next_hop_ip == r->dest_ip);
    for (RTIter i = _rtes.first(); i; i++) {
      RTEntry r = i.value();
      if (r.dest_ip == ip)
	continue; // don't expire this dest twice!

      if (r.good() && 
	  r.next_hop_ip == ip) {
	expired_dests.push_back(r.dest_ip);
	if (_log)
	  _log->log_expired_route(GridLogger::NEXT_HOP_EXPIRED, r.dest_ip);
      }
    }
  }

  unsigned int jiff = click_jiffies();

  for (int i = 0; i < expired_dests.size(); i++) {
    RTEntry *r = _rtes.findp(expired_dests[i]);
    assert(r);

    // invariant check: 
    // 1. route to expire must be good, and thus should have existing
    // expire timer and hook object.
    Timer **exp_timer = _expire_timers.findp(r->dest_ip);
    assert(exp_timer && *exp_timer);
    HookPair **hp = _expire_hooks.findp(r->dest_ip);
    assert(hp && *hp);

    // 2. that existing timer should still be scheduled, except for
    // the timer whose expiry called this hook
    assert(r->dest_ip != ip ? (*exp_timer)->scheduled() : true);
    
    // cleanup pending timers
    if ((*exp_timer)->scheduled())
      (*exp_timer)->unschedule();
    delete *exp_timer;
    delete *hp;
    _expire_timers.remove(r->dest_ip);
    _expire_hooks.remove(r->dest_ip);

    // mark route as broken
    r->invalidate(jiff);
    r->ttl = grid_hello::MAX_TTL_DEFAULT;

    // set up triggered ad
    r->advertise_ok_jiffies = jiff;
    r->need_seq_ad = true;
    r->need_metric_ad = true;
    schedule_triggered_update(r->dest_ip, jiff);
  }

  if (_log)
    _log->log_end_expire_handler();

  check_invariants();
}

void
DSDVRouteTable::schedule_triggered_update(const IPAddress &ip, unsigned int when)
{
  check_invariants();

  // get rid of outstanding triggered request (if any)
  Timer **old = _trigger_timers.findp(ip);
  HookPair **oldhp = _trigger_hooks.findp(ip);
  if (old) {
    assert(*old && (*old)->scheduled() && oldhp && *oldhp);
    (*old)->unschedule();
    delete *old;
    delete *oldhp;
  }
  
  // set up new timer
  HookPair *hp = new HookPair(this, ip);
  Timer *t = new Timer(static_trigger_hook, (void *) hp);
  t->initialize(this);
  unsigned int jiff = click_jiffies();
  t->schedule_after_ms(jiff_to_msec(jiff > when ? 0 : when - jiff));
  _trigger_timers.insert(ip, t);
  _trigger_hooks.insert(ip, hp);

  check_invariants();
}

void
DSDVRouteTable::trigger_hook(const IPAddress &ip)
{
  check_invariants(&ip);

  // invariant: the trigger timer must exist for this dest, but must
  // not be running
  Timer **old = _trigger_timers.findp(ip);
  HookPair **oldhp = _trigger_hooks.findp(ip);
  assert(old && *old && !(*old)->scheduled() && oldhp && *oldhp);

  unsigned int jiff = click_jiffies();
  unsigned int next_trigger_time = _last_triggered_update + _min_triggered_update_period;

  if (jiff >= next_trigger_time) {
    // It's ok to send a triggered update now.  Cleanup expired timer.
    delete *old;
    delete *oldhp;
    _trigger_timers.remove(ip);
    _trigger_hooks.remove(ip);

    send_triggered_update(ip);
  }
  else {
    // it's too early to send this update, so cancel all oustanding
    // triggered updates that would also be too early
    Vector<IPAddress> remove_list;
    for (TMIter i = _trigger_timers.first(); i; i++) {
      if (i.key() == ip)
	continue; // don't touch this timer, we'll reschedule it


      Timer *old2 = i.value();
      assert(old2 && old2->scheduled());

      HookPair **oldhp = _trigger_hooks.findp(i.key());
      assert(oldhp && *oldhp);

      RTEntry *r = _rtes.findp(i.key());
      if (r->advertise_ok_jiffies < next_trigger_time) {
	delete old2;
	delete *oldhp;
	remove_list.push_back(i.key());
      }
    }

    for (int i = 0; i < remove_list.size(); i++) {
      _trigger_timers.remove(remove_list[i]);
      _trigger_hooks.remove(remove_list[i]);
    }

    // reschedule this timer to earliest possible time -- when it
    // fires, its update will also include updates that would have
    // fired before then but were cancelled just above.
    (*old)->schedule_after_ms(jiff_to_msec(next_trigger_time - jiff));
  }

  check_invariants();
}

void
DSDVRouteTable::init_metric(RTEntry &r)
{
  assert(r.num_hops() == 1);

  switch (_metric_type) {
  case MetricHopCount:
    r.metric = metric_t(r.num_hops());
    break;
  case MetricEstTxCount: {
    double fwd_rate = 0;
    double rev_rate = 0;
    bool res = est_forward_delivery_rate(r.next_hop_ip, fwd_rate);
    bool res2 = est_reverse_delivery_rate(r.next_hop_ip, rev_rate);

    if (res && res2 && fwd_rate > 0 && rev_rate > 0) {
      if (fwd_rate >= 1) {
	click_chatter("init_metric ERROR: fwd rate %d is too high for %s",
		      (int) (100 * fwd_rate), r.next_hop_ip.s().cc());
	fwd_rate = 1;
      }
      if (rev_rate >= 1) {
	click_chatter("init_metric ERROR: rev rate %d is too high for %s",
		      (int) (100 * rev_rate), r.next_hop_ip.s().cc());
	rev_rate = 1;
      }
      r.metric = metric_t((unsigned int) (100 / (fwd_rate * rev_rate)));
      if (r.metric .val < 100) 
	click_chatter("init_metric WARNING: metric too small (%d) for %s",
		      r.metric.val, r.next_hop_ip.s().cc());
    } 
    else 
      r.metric = _bad_metric;
    break;
  }
  default:
    assert(0);
  }
} 

void
DSDVRouteTable::update_wst(RTEntry *old_r, RTEntry &new_r, unsigned int jiff)
{
  if (old_r == 0) {
    new_r.wst = _wst0;
    new_r.last_seq_jiffies = jiff;
  }
  else if (old_r->seq_no() == new_r.seq_no()) {
    new_r.wst = old_r->wst;
    new_r.last_seq_jiffies = old_r->last_seq_jiffies;
  }
  else if (old_r->seq_no() < new_r.seq_no()) {
    assert(old_r->last_updated_jiffies >= old_r->last_seq_jiffies);
    new_r.wst = _alpha * old_r->wst + 
      (1 - _alpha) * jiff_to_msec(old_r->last_updated_jiffies - old_r->last_seq_jiffies);
    new_r.last_seq_jiffies = jiff;
  }
  else {
    assert(old_r->seq_no() > new_r.seq_no());
    // Do nothing.  We will never accept this route anyway.
  }
  
  // XXX what happens when our current route is broken, and the new
  // route is good, what happens to wst?

  // from dsdv ns code: Note that in the if we don't touch the
  // changed_at time, so that when wst is computed, it doesn't
  // consider the infinte metric the best one at that sequence number.

}

void 
DSDVRouteTable::update_metric(RTEntry &r)
{
  assert(r.num_hops() > 1);

  RTEntry *next_hop = _rtes.findp(r.next_hop_ip);
  if (!next_hop) {
    click_chatter("DSDVRouteTable: ERROR updating metric for %s; no information for next hop %s; invalidating metric",
		  r.dest_ip.s().cc(), r.next_hop_ip.s().cc());
    r.metric = _bad_metric;
    return;
  }

  if (!r.metric.valid)
    return;

  if (!next_hop->metric.valid) {
    r.metric = _bad_metric;
    return;
  }

  switch (_metric_type) {
  case MetricHopCount:
    if (next_hop->metric.val > 1)
      click_chatter("DSDVRouteTable: WARNING metric type is hop count but next-hop %s metric is > 1 (%u)",
		    next_hop->dest_ip.s().cc(), next_hop->metric.val);
  case MetricEstTxCount: 
    if (_metric_type == MetricEstTxCount) {
      if (r.metric.val < (unsigned) 100 * (r.num_hops() - 1))
	click_chatter("update_metric WARNING received metric (%u) too low for %s (%d hops)",
		      r.metric.val, r.dest_ip.s().cc(), r.num_hops());
      if (next_hop->metric.val < 100)
	click_chatter("update_metric WARNING next hop %s for %s metric is too low (%u)",
		      next_hop->dest_ip.s().cc(), r.dest_ip.s().cc(), next_hop->metric.val);
    }
    r.metric.val += next_hop->metric.val;
    break;
  default:
    assert(0);
  }
  r.metric.valid = true;
}

bool
DSDVRouteTable::metric_preferable(const RTEntry &r1, const RTEntry &r2)
{
  // prefer a route with a valid metric
  if (r1.metric.valid && !r2.metric.valid)
    return false;
  if (!r1.metric.valid && r2.metric.valid)
    return true;
  
  // If neither metric is valid, fall back to hopcount.  Would you
  // prefer a 5-hop route or a 2-hop route, given that you don't have
  // any other information about them?  duh.
  if (!r1.metric.valid && !r2.metric.valid) {
    return r1.num_hops() < r2.num_hops();
  }
  
  assert(r1.metric.valid && r2.metric.valid);
  return metric_val_lt(r1.metric.val, r2.metric.val);
}

bool
DSDVRouteTable::metric_val_lt(unsigned int v1, unsigned int v2)
{
  switch (_metric_type) {
  case MetricHopCount: return v1 < v2; break;
  case MetricEstTxCount: 
    // add 0.25 tx count fudge factor
    return v2 > v1 + 25; break;
  default:
    assert(0);
  }
  return false;
}

bool
DSDVRouteTable::metrics_differ(const metric_t &m1, const metric_t &m2)
{
  if (!m1.valid && !m2.valid) return false;
  if (m1.valid && !m2.valid)  return true;
  if (!m1.valid && m2.valid)  return true;
  
  assert(m1.valid && m2.valid);
  return metric_val_lt(m1.val, m2.val) || metric_val_lt(m2.val, m1.val);
}

void
DSDVRouteTable::send_full_update() 
{
  check_invariants();

  unsigned int jiff = click_jiffies();
  Vector<RTEntry> routes;
  
  for (RTIter i = _rtes.first(); i; i++) {
    const RTEntry &r = i.value();
    if (r.advertise_ok_jiffies <= jiff)
      routes.push_back(r);
  }

  // reset ``need advertisement'' flag
  for (int i = 0; i < routes.size(); i++) {
    RTEntry *r = _rtes.findp(routes[i].dest_ip);
    assert(r);
    r->need_seq_ad = false;
    r->need_metric_ad = false;
    r->last_adv_metric = r->metric;
  }
  
  build_and_tx_ad(routes);

  /* 
   * Update the sequence number for periodic updates, but not for
   * triggered updates.  Originating sequence numbers are even,
   * starting at 0.  Odd numbers are reserved for other nodes to
   * advertise broken routes 
   */
  _seq_no += 2;
  _last_periodic_update = jiff;
  _last_triggered_update = jiff;

  check_invariants();
}

void
DSDVRouteTable::send_triggered_update(const IPAddress &ip) 
{
  check_invariants();

  // Check that there is actually a route to the destination which
  // prompted this trigger.  There ought to be since we never actually
  // take entries out of the route table; we only mark them as
  // expired.
  RTEntry *r = _rtes.findp(ip);
  assert(r);

  unsigned int jiff = click_jiffies();

  Vector<RTEntry> triggered_routes;
  for (RTIter i = _rtes.first(); i; i++) {
    const RTEntry &r = i.value();
    
    if ((r.need_seq_ad || r.need_metric_ad) && 
	r.advertise_ok_jiffies <= jiff)
      triggered_routes.push_back(r);    
  }

  // ns implementation of dsdv has this ``heuristic'' to decide when
  // to just do a full update.  slightly bogus, i mean, why > 3?
  if (3*triggered_routes.size() > _rtes.size() && triggered_routes.size() > 3) {
    send_full_update();
    return;
  }

  if (triggered_routes.size() == 0)
    return;

  // reset ``need advertisement'' flag
  for (int i = 0; i < triggered_routes.size(); i++) {
    RTEntry *r = _rtes.findp(triggered_routes[i].dest_ip);
    assert(r);
    r->need_seq_ad = false;
    r->last_adv_metric = r->metric;
  }

  build_and_tx_ad(triggered_routes); 
  _last_triggered_update = jiff;

  check_invariants();
}

void
DSDVRouteTable::handle_update(RTEntry &new_r, const bool was_sender, const unsigned int jiff)
{
  check_invariants();
  new_r.check();

  assert(was_sender ? new_r.num_hops() == 1 : new_r.num_hops() != 1);

  if (new_r.good() && new_r.num_hops() >_max_hops)
    return; // ignore ``non-local'' routes

  if (was_sender)
    init_metric(new_r);
  else if (new_r.good())
    update_metric(new_r);

  RTEntry *old_r = _rtes.findp(new_r.dest_ip);
  update_wst(old_r, new_r, jiff);

  // If the new route is good, and the old route (if any) was good,
  // wait for the settling time to expire before advertising.
  // Otherwise, propagate the route immediately (e.g. a newly
  // appearing node, or broken route)
  if (new_r.good() && (!old_r || old_r->good())) // XXX comment implies: new_r.good() && old_r && old_r->good()
    new_r.advertise_ok_jiffies = jiff + msec_to_jiff(2 * new_r.wst);
  else
    new_r.advertise_ok_jiffies = jiff;

  if (!old_r) {
    // Never heard of this destination before
    if (new_r.good()) {
      new_r.need_metric_ad = true;
      schedule_triggered_update(new_r.dest_ip, new_r.advertise_ok_jiffies);
    }
    insert_route(new_r, was_sender ? GridLogger::NEW_DEST_SENDER : GridLogger::NEW_DEST);
  }
  else if (old_r->seq_no() == new_r.seq_no()) {
    // Accept if better route
    assert(new_r.good() ? old_r->good() : old_r->broken()); // same seq ==> same broken state
    if (new_r.good() && metric_preferable(new_r, *old_r)) {
      if (metrics_differ(new_r.metric, new_r.last_adv_metric)) {
	new_r.need_metric_ad = true;
	schedule_triggered_update(new_r.dest_ip, new_r.advertise_ok_jiffies);
      }
      insert_route(new_r, was_sender ? GridLogger::BETTER_RTE_SENDER : GridLogger::BETTER_RTE);
    }
  }
  else if (old_r->seq_no() < new_r.seq_no()) {
    // Must *always* accept newer info
    new_r.need_seq_ad = true; // XXX this may not be best, see bake-off paper
    schedule_triggered_update(new_r.dest_ip, new_r.advertise_ok_jiffies);
    if (metrics_differ(new_r.metric, new_r.last_adv_metric))
      new_r.need_metric_ad = true;
    insert_route(new_r, was_sender ? GridLogger::NEWER_SEQ_SENDER : GridLogger::NEWER_SEQ);
  }
  else {
    assert(old_r->seq_no() > new_r.seq_no());
    if (new_r.broken() && old_r->good()) {
      // Someone has stale info, give them good info
      old_r->advertise_ok_jiffies = jiff;
      old_r->need_metric_ad = true;
      schedule_triggered_update(old_r->dest_ip, jiff);
    }
    else if (new_r.good() && old_r->broken()) {
      // Perhaps a node rebooted?  This case is not handled by ns
      // simulator DSDV code.
      assert(jiff >= old_r->last_expired_jiffies);
      unsigned age = jiff_to_msec(jiff - old_r->last_expired_jiffies);
      if (age > 2 * grid_hello::MAX_TTL_DEFAULT || was_sender) {
	// Assume we got a new entry, not a stale entry that has been
	// floating around the network.  Treat as new sequence number,
	// but slightly differently: reboot implies everything could
	// be different.
	new_r.need_seq_ad = true;
	new_r.need_metric_ad = true;
	schedule_triggered_update(new_r.dest_ip, new_r.advertise_ok_jiffies);
	insert_route(new_r, was_sender ? GridLogger::REBOOT_SEQ_SENDER : GridLogger::REBOOT_SEQ);
      }
    }
  }
  check_invariants();
}

Packet *
DSDVRouteTable::simple_action(Packet *packet)
{
  check_invariants();

  assert(packet);
  unsigned int jiff = click_jiffies();

  /* 
   * sanity check the packet, get pointers to headers 
   */  
  click_ether *eh = (click_ether *) packet->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("DSDVRouteTable %s: got non-Grid packet type", id().cc());
    packet->kill();
    return 0;
  }
  grid_hdr *gh = (grid_hdr *) (eh + 1);

  if (gh->type != grid_hdr::GRID_LR_HELLO) {
    click_chatter("DSDVRouteTable %s: received unknown Grid packet; ignoring it", id().cc());
    packet->kill();
    return 0;
  }
    
  IPAddress ipaddr((unsigned char *) &gh->tx_ip);
  EtherAddress ethaddr((unsigned char *) eh->ether_shost);

  // this should be redundant (see HostEtherFilter in grid.click)
  if (ethaddr == _eth) {
    click_chatter("DSDVRouteTable %s: received own Grid packet; ignoring it", id().cc());
    packet->kill();
    return 0;
  }

  grid_hello *hlo = (grid_hello *) (gh + 1);
   
  if (_log) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    _log->log_start_recv_advertisement(ntohl(hlo->seq_no), ipaddr, tv);
  }
  
#if 0
  if (_frozen) {
    if (_log)
      _log->log_end_recv_advertisement();
    packet->kill();
    return 0;
  }
#endif

  // maybe add new route for message transmitter, sanity check existing entry
  RTEntry *r = _rtes.findp(ipaddr);
  if (!r)
    click_chatter("DSDVRouteTable %s: new 1-hop nbr %s -- %s", 
		  id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
  else if (r->dest_eth && r->dest_eth != ethaddr)
    click_chatter("DSDVRouteTable %s: ethernet address of %s changed from %s to %s", 
		  id().cc(), ipaddr.s().cc(), r->dest_eth.s().cc(), ethaddr.s().cc());

  RTEntry new_r(ipaddr, ethaddr, gh, hlo, jiff);
  handle_update(new_r, true, jiff);
  
  // update this dest's eth
  r = _rtes.findp(ipaddr);
  assert(r);
  r->dest_eth = ethaddr;

  // handle each entry in message
  bool need_full_update = false;
  int entry_sz = hlo->nbr_entry_sz;
  char *entry_ptr = (char *) (hlo + 1);
  for (int i = 0; i < hlo->num_nbrs; i++, entry_ptr += entry_sz) {
    
    grid_nbr_entry *curr = (grid_nbr_entry *) entry_ptr;
        
    // Check for ping-pong link stats about us. We still do the
    // ping-ponging in route ads as well as pigybacking on unicast
    // data, in case we aren't sending data to that destination.
    if (curr->ip == (unsigned int) _ip && curr->num_hops == 1 && _link_tracker) {
      _link_tracker->add_stat(ipaddr, ntohl(curr->link_sig), ntohl(curr->link_qual), 
			      ntoh(curr->measurement_time));
      _link_tracker->add_bcast_stat(ipaddr, curr->num_rx, curr->num_expected, ntoh(curr->last_bcast));
    }

    RTEntry route(ipaddr, ethaddr, curr, jiff); 
    
    if (route.ttl == 0) // ignore expired ttl
      continue;
    if (route.dest_ip == _ip) { // ignore route to self
      if (route.broken()) // override broken route to us with new ad
	need_full_update = true;
      continue;
    }
    if (curr->next_hop_ip == (unsigned int) _ip)
      continue; // pseudo split-horizon

    handle_update(route, false, jiff);
  }

  if (_log)
    _log->log_end_recv_advertisement();

  if (need_full_update)
    send_full_update();

  packet->kill();
  check_invariants();
  return 0;
}

String
DSDVRouteTable::jiff_diff_string(unsigned int j1, unsigned int j2) {
  bool neg;
  unsigned int d = jiff_diff_as_msec(j1, j2, neg);
  String ret(d);
  if (neg)
    ret = "-" + ret;
  return ret;
}

unsigned int
DSDVRouteTable::jiff_diff_as_msec(unsigned int j1, unsigned int j2, bool &neg)
{
  neg = j2 > j1;
  return jiff_to_msec(neg ? j2 - j1 : j1 - j2);
}

String 
DSDVRouteTable::print_rtes_v(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;

  unsigned int jiff = click_jiffies();

  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    const RTEntry &f = i.value();
    s += f.dest_ip.s() 
      + " next=" + f.next_hop_ip.s() 
      + " hops=" + String((int) f.num_hops()) 
      + " gw=" + (f.is_gateway ? "y" : "n")
      + " loc=" + f.dest_loc.s()
      + " err=" + (f.loc_good ? "" : "-") + String(f.loc_err) // negate loc if invalid
      + " seq=" + String(f.seq_no())
      + " metric_valid=" + (f.metric.valid ? "yes" : "no")
      + " metric=" + String(f.metric.val)
      + " ttl=" + String(f.ttl)
      + " wst=" + String((unsigned long) f.wst)
      + " need_seq_ad=" + (f.need_seq_ad ? "yes" : "no")
      + " need_metric_ad=" + (f.need_metric_ad ? "yes" : "no")
      + " last_expired=" + jiff_diff_string(f.last_expired_jiffies, jiff)
      + " last_updated=" + jiff_diff_string(f.last_updated_jiffies, jiff)
      + " last_seq=" + jiff_diff_string(f.last_seq_jiffies, jiff)
      + " advertise_ok=" + jiff_diff_string(f.advertise_ok_jiffies, jiff);
    
    s += "\n";
  }
  
  return s;
}

String 
DSDVRouteTable::print_rtes(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;

  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    const RTEntry &f = i.value();
    s += f.dest_ip.s() 
      + " next=" + f.next_hop_ip.s() 
      + " hops=" + String((int) f.num_hops()) 
      + " gw=" + (f.is_gateway ? "y" : "n")
      + " seq=" + String(f.seq_no())
      + "\n";
  }
  
  return s;
}

String
DSDVRouteTable::print_nbrs_v(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;
  
  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    // only print immediate neighbors 
    if (!i.value().dest_eth) 
      continue;
    s += i.key().s();
    s += " eth=" + i.value().dest_eth.s();
    char buf[300];
    snprintf(buf, 300, " metric_valid=%s metric=%d",
	     i.value().metric.valid ? "yes" : "no", i.value().metric.val);
    s += buf;
    s += "\n";
  }

  return s;
}

String
DSDVRouteTable::print_nbrs(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;
  
  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    // only print immediate neighbors 
    if (!i.value().dest_eth)
      continue;
    s += i.key().s();
    s += " eth=" + i.value().dest_eth.s();
    s += "\n";
  }

  return s;
}

String
DSDVRouteTable::print_ip(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;
  return n->_ip.s();
}

String
DSDVRouteTable::print_eth(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;
  return n->_eth.s();
}

String
DSDVRouteTable::metric_type_to_string(MetricType t)
{
  switch (t) {
  case MetricHopCount: return "hopcount"; break;
  case MetricEstTxCount:   return "est_tx_count"; break;
  default: 
    return "unknown_metric_type";
  }
}

String
DSDVRouteTable::print_metric_type(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;
  return metric_type_to_string(n->_metric_type) + "\n";
}

DSDVRouteTable::MetricType 
DSDVRouteTable::check_metric_type(const String &s)
{
  String s2 = s.lower();
  if (s2 == "hopcount")
    return MetricHopCount;
  else if (s2 == "est_tx_count")
    return MetricEstTxCount;
  else
    return MetricUnknown;
}

int
DSDVRouteTable::write_metric_type(const String &arg, Element *el, 
				  void *, ErrorHandler *errh)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  MetricType type = check_metric_type(arg);
  if (type < 0)
    return errh->error("unknown metric type ``%s''", ((String) arg).cc());
  
  if (type != rt->_metric_type) {
    /* get rid of old metric values */
    Vector<IPAddress> entries;
    for (RTIter i = rt->_rtes.first(); i; i++) {
      /* skanky, but there's no reason for this to be quick.  i guess
         the BigHashMap doesn't let you change its values. */
      entries.push_back(i.key());
    }
    
    for (int i = 0; i < entries.size(); i++) 
      rt->_rtes.findp(entries[i])->metric = rt->_bad_metric;
  }

  rt->_metric_type = type;
  return 0;
}

String
DSDVRouteTable::print_est_type(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  return String(rt->_est_type) + "\n";
}

int
DSDVRouteTable::write_est_type(const String &arg, Element *el, 
			       void *, ErrorHandler *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  rt->_est_type = atoi(((String) arg).cc());
  return 0;
}

String
DSDVRouteTable::print_seq_delay(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  return String(rt->_seq_delay) + "\n";
}

int
DSDVRouteTable::write_seq_delay(const String &arg, Element *el, 
				void *, ErrorHandler *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  rt->_seq_delay = atoi(((String) arg).cc());
  return 0;
}

String
DSDVRouteTable::print_frozen(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  return (rt->_frozen ? "true\n" : "false\n");
}

int
DSDVRouteTable::write_frozen(const String &arg, Element *el, 
			     void *, ErrorHandler *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  rt->_frozen = atoi(((String) arg).cc());
  click_chatter("DSDVRouteTable: setting _frozen to %s", rt->_frozen ? "true" : "false");
  return 0;
}

String
DSDVRouteTable::print_links(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  
  String s = "Metric type: " + metric_type_to_string(rt->_metric_type) + "\n";

  for (RTIter i = rt->_rtes.first(); i; i++) {
    const RTEntry &r = i.value();
    if (!r.dest_eth)
      continue;

    /* get our measurements of the link *from* this neighbor */
    LinkStat::stat_t *s1 = rt->_link_stat ? rt->_link_stat->_stats.findp(r.next_hop_eth) : 0;
    struct timeval last;
    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res1 = rt->_link_stat ? 
      rt->_link_stat->get_bcast_stats(r.next_hop_eth, last, window, num_rx, num_expected) : 0;

    /* get estimates of our link *to* this neighbor */
    int tx_sig = 0;
    int tx_qual = 0;
    bool res2 = rt->_link_tracker ? rt->_link_tracker->get_stat(r.dest_ip, tx_sig, tx_qual, last) : false;
    double bcast_rate = 0;
    bool res3 = rt->_link_tracker? rt->_link_tracker->get_bcast_stat(r.dest_ip, bcast_rate, last) : false;

    char buf[255];
    double tx_rate = num_rx;
    tx_rate -= 0.5;
    tx_rate /= num_expected;
    snprintf(buf, 255, "%s %s metric=%u (%s) rx_sig=%d rx_qual=%d rx_rate=%d tx_sig=%d tx_qual=%d tx_rate=%d\n",
	     r.dest_ip.s().cc(), r.next_hop_eth.s().cc(), r.metric.val, r.metric.valid ? "valid" : "invalid",
	     s1 ? s1->sig : -1, s1 ? s1->qual : -1, res1 ? ((int) (100 * tx_rate)) : -1,
	     res2 ? tx_sig : -1, res2 ? tx_qual : -1, res3 ? (int) (100 * bcast_rate) : -1);
    s += buf;
  }
  return s;
}

void
DSDVRouteTable::add_handlers()
{
  add_default_handlers(false);
  add_read_handler("nbrs_v", print_nbrs_v, 0);
  add_read_handler("nbrs", print_nbrs, 0);
  add_read_handler("rtes_v", print_rtes_v, 0);
  add_read_handler("rtes", print_rtes, 0);
  add_read_handler("ip", print_ip, 0);
  add_read_handler("eth", print_eth, 0);
  add_read_handler("links", print_links, 0);
  add_read_handler("metric_type", print_metric_type, 0);
  add_write_handler("metric_type", write_metric_type, 0);
  add_read_handler("est_type", print_est_type, 0);
  add_write_handler("est_type", write_est_type, 0);
  add_read_handler("frozen", print_frozen, 0);
  add_write_handler("frozen", write_frozen, 0);
}

void
DSDVRouteTable::hello_hook()
{
  unsigned int msecs_to_next_ad = _period;

  unsigned int jiff = click_jiffies();
  assert(jiff >= _last_periodic_update);
  unsigned int msec_since_last = jiff_to_msec(jiff - _last_periodic_update);
  if (msec_since_last < 2 * _period / 3) {
    // a full periodic update was sent ahead of schedule (because
    // there were so many triggered updates to send).  reschedule this
    // period update to one period after the last periodic update
    
    unsigned int jiff_period = msec_to_jiff(_period);
    msecs_to_next_ad = jiff_to_msec(_last_periodic_update + jiff_period - jiff);
  }
  else {
    send_full_update();
    _last_periodic_update = jiff;
    _last_triggered_update = jiff;
  }

  // reschedule periodic update
  int r2 = random();
  double r = (double) (r2 >> 1);
  unsigned int jitter = (unsigned int) (((double) _jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1) {
    if (jitter <= msecs_to_next_ad)
      msecs_to_next_ad -= jitter;
  }
  else 
    msecs_to_next_ad += jitter;

  _hello_timer.schedule_after_ms(msecs_to_next_ad);
}


void
DSDVRouteTable::build_and_tx_ad(Vector<RTEntry> &rtes_to_send)
{
  /*
   * build and send routing update packet advertising the contents of
   * the rtes_to_send vector.  
   */

  int hdr_sz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_hello);
  int max_rtes = (1500 - hdr_sz) / sizeof(grid_nbr_entry);
  int num_rtes = min(max_rtes, rtes_to_send.size()); 
  int psz = hdr_sz + sizeof(grid_nbr_entry) * num_rtes;

  assert(psz <= 1500);
  if (num_rtes < rtes_to_send.size())
    click_chatter("DSDVRouteTable %s: too many routes, truncating route advertisement",
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

  hlo->is_gateway = _gw_info ? _gw_info->is_gateway() : false;

  if (_log)
    _log->log_sent_advertisement(_seq_no, tv);

  
  _bcast_count++;
  grid_hdr::set_pad_bytes(*gh, htonl(_bcast_count));

  hlo->ttl = htonl(grid_hello::MAX_TTL_DEFAULT);

  grid_nbr_entry *curr = (grid_nbr_entry *) (hlo + 1);

  for (int i = 0; i < num_rtes; i++, curr++) 
    rtes_to_send[i].fill_in(curr, _link_stat);

  output(0).push(p);
}


void
DSDVRouteTable::RTEntry::fill_in(grid_nbr_entry *nb, LinkStat *ls) const
{
  check();
  nb->ip = dest_ip;
  nb->next_hop_ip = next_hop_ip;
  nb->num_hops = num_hops();
  nb->loc = dest_loc;
  nb->loc_err = htons(loc_err);
  nb->loc_good = loc_good;
  nb->seq_no = htonl(seq_no());
  nb->metric = htonl(metric.val);
  nb->metric_valid = metric.valid;
  nb->is_gateway = is_gateway;

  unsigned int jiff = click_jiffies();
  unsigned int ttl_decrement = jiff_to_msec(good() ? jiff - last_updated_jiffies : jiff - last_expired_jiffies);
  nb->ttl = htonl(decr_ttl(ttl, max(ttl_decrement, grid_hello::MIN_TTL_DECREMENT)));
  
  /* ping-pong link stats back to sender */
  nb->link_qual = 0;
  nb->link_sig = 0;
  nb->measurement_time.tv_sec = nb->measurement_time.tv_usec = 0;
  if (ls && num_hops() == 1) {
    LinkStat::stat_t *s = ls ? ls->_stats.findp(next_hop_eth) : 0;
    if (s) {
      nb->link_qual = htonl(s->qual);
      nb->link_sig = htonl(s->sig);
      nb->measurement_time.tv_sec = htonl(s->when.tv_sec);
      nb->measurement_time.tv_usec = htonl(s->when.tv_usec);
    }
    else if (ls)
      click_chatter("DSDVRouteTable: error!  unable to get signal strength or quality info for one-hop neighbor %s\n",
		    IPAddress(dest_ip).s().cc());

    nb->num_rx = 0;
    nb->num_expected = 0;
    nb->last_bcast.tv_sec = nb->last_bcast.tv_usec = 0;
    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res = ls ? ls->get_bcast_stats(next_hop_eth, nb->last_bcast, window, num_rx, num_expected) : 0;
    if (res) {
      if (num_rx > 255 || num_expected > 255) {
	click_chatter("DSDVRouteTable: error! overflow on broadcast loss stats for one-hop neighbor %s",
		      IPAddress(dest_ip).s().cc());
	num_rx = num_expected = 255;
      }
      nb->num_rx = num_rx;
      nb->num_expected = num_expected;
      nb->last_bcast = hton(nb->last_bcast);
    }
  }
}

void
DSDVRouteTable::log_dump_hook(bool reschedule)
{
  if (_log) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    Vector<RouteEntry> vec;
    get_all_entries(vec);
    _log->log_route_dump(vec, tv);
  }
  if (reschedule)
    _log_dump_timer.schedule_after_ms(_log_dump_period); 
}

void
DSDVRouteTable::RTEntry::dump() const
{
  check();
  unsigned int jiff = click_jiffies();
  click_chatter(" 	dest_ip: %s", dest_ip.s().cc());
  click_chatter("      dest_eth: %s", dest_eth.s().cc());
  click_chatter("   next_hop_ip: %s", next_hop_ip.s().cc());
  click_chatter("  next_hop_eth: %s", next_hop_eth.s().cc());
  click_chatter(" 	 seq_no: %u", seq_no());
  click_chatter("      num_hops: %u", (unsigned int) num_hops());
  click_chatter("  last_updated: %s", jiff_diff_string(last_updated_jiffies, jiff).cc());
  click_chatter("  last_expired: %s", jiff_diff_string(last_expired_jiffies, jiff).cc());
  click_chatter("      last_seq: %s", jiff_diff_string(last_seq_jiffies, jiff).cc());
  click_chatter("  advertise_ok: %s", jiff_diff_string(advertise_ok_jiffies, jiff).cc());
  click_chatter(" 	 metric: %u", metric.val);
  click_chatter("need_metric_ad: %s", need_metric_ad ? "yes" : "no");
  click_chatter("   need_seq_ad: %s", need_seq_ad ? "yes" : "no");
  click_chatter("           wst: %u", (unsigned int) wst);
}

void
DSDVRouteTable::check_invariants(const IPAddress *ignore) const
{
  for (RTIter i = _rtes.first(); i; i++) {
    const RTEntry &r = i.value();

    r.check();

    if (ignore && *ignore == i.key())
      continue;

    // check expire timer invariants
    Timer **t = _expire_timers.findp(r.dest_ip);
    HookPair **hp = _expire_hooks.findp(r.dest_ip);

    if (r.good()) {
      assert(t);
      assert(*t);
      assert((*t)->scheduled());
      assert(hp);
      assert(*hp);
      assert((*hp)->obj == this);
      assert((*hp)->ip == (unsigned int) r.dest_ip);
    }
    else {
      assert(!t);
      assert(!hp);
    }      

    // check trigger timer invariants
    t = _trigger_timers.findp(r.dest_ip);
    hp = _trigger_hooks.findp(r.dest_ip);
    if (t) {
      assert(*t);
      assert((*t)->scheduled());
      assert(hp);
      assert(*hp);
      assert((*hp)->obj == this);
      assert((*hp)->ip == (unsigned int) r.dest_ip);
      // assert(r.need_seq_ad || r.need_metric_ad); // see note for trigger invariants
    }
    else {
      assert(!hp);
    }
  }
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(DSDVRouteTable)

#include <click/bighashmap.cc>
template class BigHashMap<IPAddress, DSDVRouteTable::RTEntry>;
template class BigHashMap<IPAddress, Timer *>;
#include <click/vector.cc>
template class Vector<DSDVRouteTable::RTEntry>;
