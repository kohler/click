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
#include <cstddef>
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/standard/scheduleinfo.hh>
#include <click/router.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <elements/grid/dsdvroutetable.hh>
#include <elements/grid/linkstat.hh>
#include <elements/grid/gridgatewayinfo.hh>
#include <elements/grid/timeutils.hh>
CLICK_DECLS

#define DBG  0
#define DBG2 0
#define DBG3 0

#define FULL_DUMP_ON_TRIG_UPDATE 0

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define dsdv_assert(e) ((e) ? (void) 0 : dsdv_assert_(__FILE__, __LINE__, #e))

bool
DSDVRouteTable::get_one_entry(IPAddress &dest_ip, RouteEntry &entry) 
{
  RTEntry *r = _rtes.findp(dest_ip);
  if (r == 0)
    return false;
#if USE_OLD_SEQ
  if (use_old_route(dest_ip, dsdv_jiffies())) 
    r = _old_rtes.findp(dest_ip);
#endif
  entry = *r;
  return true;  
}

void
DSDVRouteTable::get_all_entries(Vector<RouteEntry> &vec)
{
#if USE_OLD_SEQ
  unsigned jiff = dsdv_jiffies();
#endif
  for (RTIter iter = _rtes.begin(); iter; iter++) {
    const RTEntry &rte = iter.value();
#if USE_OLD_SEQ
    if (use_old_route(rte.dest_ip, jiff))
      vec.push_back(_old_rtes[rte.dest_ip]);
    else
      vec.push_back(rte);
#else
    vec.push_back(rte);
#endif
  }
}


#if USE_OLD_SEQ
bool
DSDVRouteTable::use_old_route(const IPAddress &dst, unsigned jiff)
{
  if (!_use_old_route)
    return false;

  RTEntry *real = _rtes.findp(dst);
  RTEntry *old = _old_rtes.findp(dst);
#if USE_GOOD_NEW_ROUTES
  return
    (real && real->good() && 
     old && old->good() &&
     (real->advertise_ok_jiffies > jiff ||
      metric_preferable(*real, *old)));
#else
  return
    (real && real->good() &&             // if real route is bad, don't use good but old route
     old && old->good() &&               // if old route is bad, don't use it
     real->advertise_ok_jiffies > jiff); // if ok to advertise real route, don't use old route
#endif
}
#endif

DSDVRouteTable::DSDVRouteTable() : 
  GridGenericRouteTable(1, 1), _gw_info(0),
  _link_stat(0), _log(0), 
  _seq_no(0), _bcast_count(0),
  _max_hops(3), _alpha(88), _wst0(6000),
  _last_periodic_update(0),
  _last_triggered_update(0), 
  _hello_timer(static_hello_hook, this),
  _log_dump_timer(static_log_dump_hook, this),
  _metric_type(MetricHopCount),
  _est_type(EstByMeas),
  _frozen(false)
{
  MOD_INC_USE_COUNT;
}

DSDVRouteTable::~DSDVRouteTable()
{
  MOD_DEC_USE_COUNT;

  for (TMIter i = _expire_timers.begin(); i; i++) {
    if (i.value()->scheduled())
      i.value()->unschedule();
    delete i.value();
  }
  for (TMIter i = _trigger_timers.begin(); i; i++) {
    if (i.value()->scheduled())
      i.value()->unschedule();
    delete i.value();
  }
  for (HMIter i = _expire_hooks.begin(); i; i++) 
    delete i.value();
  for (HMIter i = _trigger_hooks.begin(); i; i++) 
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
  String metric("hopcount");
  String logfile;
  int res = cp_va_parse(conf, this, errh,
			cpUnsigned, "entry timeout (msec)", &_timeout,
			cpUnsigned, "route broadcast period (msec)", &_period,
			cpUnsigned, "route broadcast jitter (msec)", &_jitter,
			cpUnsigned, "minimum triggered update period (msec)", &_min_triggered_update_period,
			cpEthernetAddress, "source Ethernet address", &_eth,
			cpIPAddress, "source IP address", &_ip,
			cpKeywords,
			"GW", cpElement, "GridGatewayInfo element", &_gw_info,
			"LS", cpElement, "LinkStat element", &_link_stat,
			"MAX_HOPS", cpUnsigned, "max hops", &_max_hops,
			"METRIC", cpString, "route metric", &metric,
			"LOG", cpElement, "GridGenericLogger element", &_log,
			"WST0", cpUnsigned, "initial weight settling time, wst0 (msec)", &_wst0,
			"ALPHA", cpUnsigned, "alpha parameter for settling time computation, in percent (0 <= ALPHA <= 100)", &_alpha,
			"SEQ0", cpUnsigned, "initial sequence number (must be even)", &_seq_no,
			0);

  if (res < 0)
    return res;

  if (_seq_no & 1) 
    return errh->error("initial sequence number must be even");

  if (_timeout == 0)
    return errh->error("timeout interval must be greater than 0");
  if (_period == 0)
    return errh->error("period must be greater than 0");
  // _jitter is allowed to be 0
  if (_jitter > _period)
    return errh->error("jitter is bigger than period");
  if (_max_hops == 0)
    return errh->error("max hops must be greater than 0");
  if (_alpha > 100) 
    return errh->error("alpha must be between 0 and 100 inclusive");

  _metric_type = check_metric_type(metric);
  if (_metric_type < 0)
    return errh->error("Unknown metric type ``%s''", metric.cc());

  if (_log && _log->cast("GridGenericLogger") == 0) 
    return errh->error("LOG element is not a GridGenericLogger");

  if (_gw_info == 0)
    errh->warning("No GridGatewayInfo element specified, will not advertise as gateway");
  if (_link_stat == 0)
    errh->warning("LinkStat elements not specified, some metrics may not work");

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
#if USE_OLD_SEQ
  _use_old_route = false;
#endif
  return 0;
}

bool
DSDVRouteTable::current_gateway(RouteEntry &entry)
{
  for (RTIter i = _rtes.begin(); i; i++) {
    if (i.value().is_gateway) {
      entry = i.value();
      return true;
    }
  }
  return false;
}

bool
DSDVRouteTable::est_forward_delivery_rate(const IPAddress &ip, unsigned int &rate)
{
  switch (_est_type) {
  case EstByMeas: {
    if (!_link_stat) 
      return false;
    RTEntry *r = _rtes.findp(ip);
    if (r == 0 || r->num_hops() > 1) 
      return false;
    unsigned int tau;
    struct timeval t;
    bool res = _link_stat->get_forward_rate(r->dest_eth, &rate, &tau, &t);
    if (res && rate > 100) {
      click_chatter("DSDVRouteTable %s: ERROR: forward rate %u%% is too high for %s, capping at 100%%",
		    id().cc(), rate, ip.s().cc());
      rate = 100;
    }
    return res;
  }

  default:
    return false;
  }
}

bool
DSDVRouteTable::est_reverse_delivery_rate(const IPAddress &ip, unsigned int &rate)
{
  switch (_est_type) {
  case EstByMeas: {
    if (!_link_stat)
      return false;
    RTEntry *r = _rtes.findp(ip);
    if (r == 0 || r->num_hops() > 1)
      return false;
    unsigned int tau;
    bool res = _link_stat->get_reverse_rate(r->dest_eth, &rate, &tau);
    if (res && rate > 100) {
      click_chatter("DSDVRouteTable %s: ERROR: reverse rate %u%% is too high for %s, capping at 100%%",
		    id().cc(), rate, ip.s().cc());
      rate = 100;
    }
    return res;
  }

  default:
    return false;
  }
}

void
DSDVRouteTable::insert_route(const RTEntry &r, const GridGenericLogger::reason_t why)
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
    dsdv_assert(old && *old && (*old)->scheduled() && oldhp && *oldhp);
  else { 
    dsdv_assert(old == 0);
    dsdv_assert(oldhp == 0);
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

#if USE_OLD_SEQ
  // if we are getting new seqno, save route for old seqno
  if (old_r && old_r->seq_no() < r.seq_no()) 
    _old_rtes.insert(r.dest_ip, *old_r);
#endif

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
  dsdv_assert(r != 0);
  
  // 2. route to expire should be good
  dsdv_assert(r->good() && (r->seq_no() & 1) == 0);
  
  // 3. the expire timer for this dest should exist, but should not be
  // running.
  Timer **old = _expire_timers.findp(ip);
  HookPair **oldhp = _expire_hooks.findp(ip);
  dsdv_assert(old && *old && !(*old)->scheduled() && oldhp && *oldhp);

  if (_log) {
    timeval tv;
    click_gettimeofday(&tv);
    _log->log_start_expire_handler(tv);
    _log->log_expired_route(GridGenericLogger::TIMEOUT, ip);
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
    dsdv_assert(r->next_hop_ip == r->dest_ip);
    for (RTIter i = _rtes.begin(); i; i++) {
      RTEntry r = i.value();
      if (r.dest_ip == ip)
	continue; // don't expire this dest twice!

      if (r.good() && 
	  r.next_hop_ip == ip) {
	expired_dests.push_back(r.dest_ip);
	if (_log)
	  _log->log_expired_route(GridGenericLogger::NEXT_HOP_EXPIRED, r.dest_ip);
      }
    }
  }

  unsigned int jiff = dsdv_jiffies();

  for (int i = 0; i < expired_dests.size(); i++) {
    RTEntry *r = _rtes.findp(expired_dests[i]);
    dsdv_assert(r);

    // invariant check: 
    // 1. route to expire must be good, and thus should have existing
    // expire timer and hook object.
    Timer **exp_timer = _expire_timers.findp(r->dest_ip);
    dsdv_assert(exp_timer && *exp_timer);
    HookPair **hp = _expire_hooks.findp(r->dest_ip);
    dsdv_assert(hp && *hp);

    // 2. that existing timer should still be scheduled, except for
    // the timer whose expiry called this hook
    dsdv_assert(r->dest_ip != ip ? (*exp_timer)->scheduled() : true);
    
    // cleanup pending timers
    if ((*exp_timer)->scheduled())
      (*exp_timer)->unschedule();
    delete *exp_timer;
    delete *hp;
    _expire_timers.remove(r->dest_ip);
    _expire_hooks.remove(r->dest_ip);

    // mark route as broken
    r->invalidate(jiff);
#if USE_OLD_SEQ
    RTEntry *old_r = _old_rtes.findp(r->dest_ip);
    if (old_r && old_r->good())
      old_r->invalidate(jiff);
#endif
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
    dsdv_assert(*old && (*old)->scheduled() && oldhp && *oldhp);
    (*old)->unschedule();
    delete *old;
    delete *oldhp;
  }
  
  // set up new timer
  HookPair *hp = new HookPair(this, ip);
  Timer *t = new Timer(static_trigger_hook, (void *) hp);
  t->initialize(this);
  unsigned int jiff = dsdv_jiffies();
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
  dsdv_assert(old && *old && !(*old)->scheduled() && oldhp && *oldhp);

  unsigned int jiff = dsdv_jiffies();
  unsigned int next_trigger_jiff = _last_triggered_update + msec_to_jiff(_min_triggered_update_period);

#if DBG
  click_chatter("%s: XXX trigger_hoook(%s)\n", id().cc(), ip.s().cc());
#endif

  if (jiff >= next_trigger_jiff) {
    // It's ok to send a triggered update now.  Cleanup expired timer.
    delete *old;
    delete *oldhp;
    _trigger_timers.remove(ip);
    _trigger_hooks.remove(ip);

    send_triggered_update(ip);
#if DBG
    click_chatter("%s: XXX sent triggered update\n", id().cc());
#endif
  }
  else {
#if DBG
    click_chatter("%s: XXX too early to send triggered update (jiff=%d, next_trigger_jiff=%d, _min_triggered_update_period=%d)\n", id().cc(), jiff, next_trigger_jiff, _min_triggered_update_period);
#endif
    // it's too early to send this update, so cancel all oustanding
    // triggered updates that would also be too early
    Vector<IPAddress> remove_list;
    for (TMIter i = _trigger_timers.begin(); i; i++) {
      if (i.key() == ip)
	continue; // don't touch this timer, we'll reschedule it


      Timer *old2 = i.value();
      dsdv_assert(old2 && old2->scheduled());

      HookPair **oldhp = _trigger_hooks.findp(i.key());
      dsdv_assert(oldhp && *oldhp);

      RTEntry *r = _rtes.findp(i.key());
      if (r->advertise_ok_jiffies < next_trigger_jiff) {
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
    (*old)->schedule_after_ms(jiff_to_msec(next_trigger_jiff - jiff));
  }

  check_invariants();
}

void
DSDVRouteTable::init_metric(RTEntry &r)
{
  dsdv_assert(r.num_hops() == 1);

  switch (_metric_type) {
  case MetricHopCount:
    r.metric = metric_t(r.num_hops());
    break;
  case MetricEstTxCount: {
    unsigned fwd_rate = 0;
    unsigned rev_rate = 0;
    bool res = est_forward_delivery_rate(r.next_hop_ip, fwd_rate);
    bool res2 = est_reverse_delivery_rate(r.next_hop_ip, rev_rate);
#if DBG3
    click_chatter("%s: XXX init_metric est_tx_count, res=%s, fwd_rate=%u, res2=%s rev_rate=%u\n",
		  id().cc(), res ? "true" : "false", fwd_rate, res2 ? "true" : "false", rev_rate);
#endif
    if (res && res2 && fwd_rate > 0 && rev_rate > 0) {
      r.metric = metric_t(100 * 100 * 100 / (fwd_rate * rev_rate));
      if (r.metric.val < 100) 
	click_chatter("DSDVRouteTable %s: init_metric WARNING: metric %d%% transmissions to %s is too low for one hop",
		      id().cc(), r.metric.val, r.next_hop_ip.s().cc());
    } 
    else 
      r.metric = _bad_metric;
    break;
  }
  case MetricRevDeliveryRateProduct:
  case MetricDeliveryRateProduct: {
    unsigned rate;
    bool res = false;
    if (_metric_type == MetricDeliveryRateProduct) 
      res = est_forward_delivery_rate(r.next_hop_ip, rate);
    else if (_metric_type == MetricRevDeliveryRateProduct) 
      res = est_reverse_delivery_rate(r.next_hop_ip, rate);
    else
      dsdv_assert(0);
    if (res)
      r.metric = metric_t(rate);
    else
      r.metric = _bad_metric;
    break;
  }
  default:
    dsdv_assert(0);
  }
} 

void
DSDVRouteTable::update_wst(RTEntry *old_r, RTEntry &new_r, unsigned int jiff)
{
  if (old_r) {
    old_r->check();
    dsdv_assert(old_r->last_updated_jiffies <= new_r.last_updated_jiffies);
  }

  if (old_r == 0) {
    new_r.wst = _wst0;
    new_r.last_seq_jiffies = jiff;
    new_r.check();
  }
  else if (old_r->seq_no() == new_r.seq_no()) {
    new_r.wst = old_r->wst;
    new_r.last_seq_jiffies = old_r->last_seq_jiffies;
    new_r.check();
  }
  else if (old_r->seq_no() < new_r.seq_no()) {
    dsdv_assert(old_r->last_updated_jiffies >= old_r->last_seq_jiffies); // XXX failed!
    new_r.wst = _alpha * old_r->wst + 
      (100 - _alpha) * jiff_to_msec(old_r->last_updated_jiffies - old_r->last_seq_jiffies);
    new_r.wst /= 100; // since _alpha is 0-100 percent
    new_r.last_seq_jiffies = jiff;
    new_r.check();
  }
  else {
    dsdv_assert(old_r->seq_no() > new_r.seq_no());
    // Do nothing.  We will never accept this route anyway.  Well,
    // almost never.  See the reboot/wraparound handling in
    // handle_update()
  }
  
  // XXX when our current route is broken, and the new route is good,
  // what happens to wst?

  // from dsdv ns code: Note that we don't touch the changed_at time,
  // so that when wst is computed, it doesn't consider the infinte
  // metric the best one at that sequence number.
}

void 
DSDVRouteTable::update_metric(RTEntry &r)
{
  dsdv_assert(r.num_hops() > 1);

  RTEntry *next_hop = _rtes.findp(r.next_hop_ip);
  if (!next_hop) {
    click_chatter("DSDVRouteTable %s: ERROR updating metric for %s; no information for next hop %s; invalidating metric",
		  id().cc(), r.dest_ip.s().cc(), r.next_hop_ip.s().cc());
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
    r.metric.val += next_hop->metric.val;
    if (r.metric.val != r.num_hops())
      click_chatter("DSDVRouteTable %s: WARNING metric type is hop count but %s metric doesn't match hopcount",
		    id().cc(), next_hop->dest_ip.s().cc());    
    break;

  case MetricEstTxCount: 
    if (r.metric.val < (unsigned) 100 * (r.num_hops() - 1))
      click_chatter("DSDVRouteTable %s: update_metric WARNING received transmission count %u%% is too low for %s (%d hops)",
		    id().cc(), r.metric.val, r.dest_ip.s().cc(), r.num_hops());
    if (next_hop->metric.val < 100)
      click_chatter("DSDVRouteTable %s: update_metric WARNING next hop %s transmission count %u%% is too low for %s",
		    id().cc(), next_hop->dest_ip.s().cc(), next_hop->metric.val, r.dest_ip.s().cc());
    r.metric.val += next_hop->metric.val;
    break;

  case MetricDeliveryRateProduct:
  case MetricRevDeliveryRateProduct:
    if (r.metric.val > 100)
      click_chatter("DSDVRouteTable %s: update_metric WARNING received delivery rate product %u%% is too high for %d",
		    id().cc(), r.metric.val, r.dest_ip.s().cc());
    if (next_hop->metric.val > 100)
      click_chatter("DSDVRouteTable %s: update_metric WARNING next hop %s delivery rate product %u%% is too high for %s",
		    id().cc(), next_hop->dest_ip.s().cc(), next_hop->metric.val, r.dest_ip.s().cc());
    r.metric.val *= next_hop->metric.val;
    r.metric.val /= 100;
    break;

  default:
    dsdv_assert(0);
  }
  r.metric.valid = true;
}

bool
DSDVRouteTable::metric_preferable(const RTEntry &r1, const RTEntry &r2)
{
  // true if r1 is preferable to r2
#if DBG2
  click_chatter("%s: XXX metric_preferable valid?  1:%s  2:%s   1 < 2? %s", id().cc(),
		(r1.metric.valid ? "yes" : "no"), (r2.metric.valid ? "yes" : "no"),
		(metric_val_lt(r1.metric.val, r2.metric.val) ? "yes" : "no"));
#endif
  // prefer a route with a valid metric
  if (r1.metric.valid && !r2.metric.valid)
    return true;
  if (!r1.metric.valid && r2.metric.valid)
    return false;
  
  // If neither metric is valid, fall back to hopcount.  Would you
  // prefer a 5-hop route or a 2-hop route, given that you don't have
  // any other information about them?  duh.
  if (!r1.metric.valid && !r2.metric.valid) {
    return r1.num_hops() < r2.num_hops();
  }
  
  dsdv_assert(r1.metric.valid && r2.metric.valid);
  return metric_val_lt(r1.metric.val, r2.metric.val);
}

bool
DSDVRouteTable::metric_val_lt(unsigned int v1, unsigned int v2)
{
  // Better metric should be ``less''

  switch (_metric_type) {
  case MetricHopCount:               return v1 < v2; break;
  case MetricEstTxCount:             return v1 < v2; break; // + 25; break; // add 0.25 tx count fudge factor
  case MetricDeliveryRateProduct:    return v1 > v2; break;
  case MetricRevDeliveryRateProduct: return v1 > v2; break;
  default: dsdv_assert(0);
  }
  return false;
}

bool
DSDVRouteTable::metrics_differ(const metric_t &m1, const metric_t &m2)
{
  if (!m1.valid && !m2.valid) return false;
  if (m1.valid && !m2.valid)  return true;
  if (!m1.valid && m2.valid)  return true;
  
  dsdv_assert(m1.valid && m2.valid);
  return metric_val_lt(m1.val, m2.val) || metric_val_lt(m2.val, m1.val);
}

void
DSDVRouteTable::send_full_update() 
{
  check_invariants();
#if DBG
  click_chatter("%s: XXX sending full update\n", id().cc());
#endif
  unsigned int jiff = dsdv_jiffies();
  Vector<RTEntry> routes;
  
  for (RTIter i = _rtes.begin(); i; i++) {
    const RTEntry &r = i.value();
    if (r.advertise_ok_jiffies <= jiff)
      routes.push_back(r);
#if DBG
    else
      click_chatter("%s: XXX excluding %s\n", id().cc(), r.dest_ip.s().cc());
#endif
  }

  // reset ``need advertisement'' flag
  for (int i = 0; i < routes.size(); i++) {
    RTEntry *r = _rtes.findp(routes[i].dest_ip);
    dsdv_assert(r);
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
  dsdv_assert(r);

  unsigned int jiff = dsdv_jiffies();

  Vector<RTEntry> triggered_routes;
  for (RTIter i = _rtes.begin(); i; i++) {
    const RTEntry &r = i.value();
    
    if ((r.need_seq_ad || r.need_metric_ad) && 
	r.advertise_ok_jiffies <= jiff)
      triggered_routes.push_back(r);    
  }
#if FULL_DUMP_ON_TRIG_UPDATE
  // ns implementation of dsdv has this ``heuristic'' to decide when
  // to just do a full update.  slightly bogus, i mean, why > 3?
  if (3*triggered_routes.size() > _rtes.size() && triggered_routes.size() > 3) {
    send_full_update();
    return;
  }
#endif
  if (triggered_routes.size() == 0)
    return;

  // reset ``need advertisement'' flag
  for (int i = 0; i < triggered_routes.size(); i++) {
    RTEntry *r = _rtes.findp(triggered_routes[i].dest_ip);
    dsdv_assert(r);
    r->need_seq_ad = false; // XXX why not reset need_metric_ad flag as well?
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

  dsdv_assert(was_sender ? new_r.num_hops() == 1 : new_r.num_hops() != 1);

  if (new_r.good() && new_r.num_hops() >_max_hops)
    return; // ignore ``non-local'' routes

  if (was_sender)
    init_metric(new_r);
  else if (new_r.good())
    update_metric(new_r);

  RTEntry *old_r = _rtes.findp(new_r.dest_ip);
  update_wst(old_r, new_r, jiff);

  // If the new route is good, and the old route (if any) was good,
  // wait for the settling time to pass since we first heard this
  // sequence number before advertising.  Otherwise, propagate the
  // route immediately (e.g. a newly appearing node, or broken route).
  //
  // Note: I think the DSDV code in the ns simulator has this wrong.
  // See line 652 in file cmu/dsdv/dsdv.cc.  That code sets the time
  // it's ok to advertise at to the current time + 2 * wst, which
  // would be as if we were adding jiff instead of
  // new_r.last_seq_jiffies in the code below.
  if (new_r.good() && (!old_r || old_r->good())) // XXX comment implies: new_r.good() && old_r && old_r->good()
    new_r.advertise_ok_jiffies = new_r.last_seq_jiffies + msec_to_jiff((unsigned int) (2 * new_r.wst));
  else
    new_r.advertise_ok_jiffies = jiff;

#if DBG
  click_chatter("%s: XXX dest=%s advertise_ok_jiffies=%d wst=%f jiff=%d\n", 
		id().cc(), new_r.dest_ip.s().cc(),
		new_r.advertise_ok_jiffies, new_r.wst, jiff);
#endif

  if (!old_r) {
    // Never heard of this destination before
    if (new_r.good()) {
      new_r.need_metric_ad = true;
      schedule_triggered_update(new_r.dest_ip, new_r.advertise_ok_jiffies);
#if DBG
      click_chatter("%s: XXX scheduled brand-new route to %s to be advertised in %d jiffies from now\n", id().cc(),
		    new_r.dest_ip.s().cc(), new_r.advertise_ok_jiffies - jiff);
#endif
    }
    insert_route(new_r, was_sender ? GridGenericLogger::NEW_DEST_SENDER : GridGenericLogger::NEW_DEST);
  }
  else if (old_r->seq_no() == new_r.seq_no()) {
    // Accept if better route
    dsdv_assert(new_r.good() ? old_r->good() : old_r->broken()); // same seq ==> same broken state
#if DBG2
    click_chatter("%s: XXX checking for better route to %s from %s with same seqno %u",
		  id().cc(), new_r.dest_ip.s().cc(), new_r.next_hop_ip.s().cc(), new_r.seq_no());
    click_chatter("%s: XXX good=%s  preferable=%s", id().cc(), new_r.good() ? "yes" : "no",
		  metric_preferable(new_r, *old_r) ? "yes" : "no");
#endif
    if (new_r.good() && metric_preferable(new_r, *old_r)) {
      if (metrics_differ(new_r.metric, new_r.last_adv_metric)) {
	new_r.need_metric_ad = true;
	schedule_triggered_update(new_r.dest_ip, new_r.advertise_ok_jiffies);
      }
      insert_route(new_r, was_sender ? GridGenericLogger::BETTER_RTE_SENDER : GridGenericLogger::BETTER_RTE);
    }
  }
  else if (old_r->seq_no() < new_r.seq_no()) {
    // Must *always* accept newer info
    new_r.need_seq_ad = true; // XXX this may not be best, see bake-off paper
    schedule_triggered_update(new_r.dest_ip, new_r.advertise_ok_jiffies);
    if (metrics_differ(new_r.metric, new_r.last_adv_metric))
      new_r.need_metric_ad = true;
    insert_route(new_r, was_sender ? GridGenericLogger::NEWER_SEQ_SENDER : GridGenericLogger::NEWER_SEQ);
  }
  else {
    dsdv_assert(old_r->seq_no() > new_r.seq_no());
    if (new_r.broken() && old_r->good()) {
      // Someone has stale info, give them good info
      old_r->advertise_ok_jiffies = jiff;
      old_r->need_metric_ad = true;
      schedule_triggered_update(old_r->dest_ip, jiff);
    }
    else if (new_r.good() && old_r->broken()) {
      // Perhaps a node rebooted?  This case is not handled by ns
      // simulator DSDV code.
      dsdv_assert(jiff >= old_r->last_expired_jiffies);
      unsigned age = jiff_to_msec(jiff - old_r->last_expired_jiffies);
      if (age > 2 * grid_hello::MAX_TTL_DEFAULT || was_sender) {
	// Assume we got a new entry, not a stale entry that has been
	// floating around the network.  Treat as newer sequence number,
	// but slightly differently: reboot implies everything could
	// be different.
	new_r.need_seq_ad = true;
	new_r.need_metric_ad = true;
	new_r.last_seq_jiffies = jiff; // not done by update_wst() becaue seq is less, so do it here
	dsdv_assert(new_r.last_updated_jiffies == new_r.last_seq_jiffies);
	schedule_triggered_update(new_r.dest_ip, new_r.advertise_ok_jiffies);
	insert_route(new_r, was_sender ? GridGenericLogger::REBOOT_SEQ_SENDER : GridGenericLogger::REBOOT_SEQ);
      }
    }
  }
  check_invariants();
}

Packet *
DSDVRouteTable::simple_action(Packet *packet)
{
  check_invariants();

  dsdv_assert(packet);
  unsigned int jiff = dsdv_jiffies();

  /* 
   * sanity check the packet, get pointers to headers.  These should
   * be redundant due to classifiers etc. in the Grid Click
   * configuration, but don't dis paranoia.
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

  if (ethaddr == _eth) {
    click_chatter("DSDVRouteTable %s: received own Grid packet; ignoring it", id().cc());
    packet->kill();
    return 0;
  }

  grid_hello *hlo = (grid_hello *) (gh + 1);
   
  if (_log) {
    struct timeval tv;
    click_gettimeofday(&tv);
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

  RTEntry new_r(ipaddr, ethaddr, gh, hlo, PAINT_ANNO(packet), jiff);
  handle_update(new_r, true, jiff);
  
  // update this dest's eth
  r = _rtes.findp(ipaddr);
  dsdv_assert(r);
  r->dest_eth = ethaddr;

  // handle each entry in message
  bool need_full_update = false;
  int entry_sz = hlo->nbr_entry_sz;
  char *entry_ptr = (char *) (hlo + 1);
  for (int i = 0; i < hlo->num_nbrs; i++, entry_ptr += entry_sz) {
    
    grid_nbr_entry *curr = (grid_nbr_entry *) entry_ptr;
    RTEntry route(ipaddr, ethaddr, curr, PAINT_ANNO(packet), jiff); 
    
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

  unsigned int jiff = dsdv_jiffies();

  String s;
  for (RTIter i = n->_rtes.begin(); i; i++) {
#if USE_OLD_SEQ
    RTEntry f = i.value();
    if (n->use_old_route(f.dest_ip, jiff))
      f = n->_old_rtes[f.dest_ip];
#else
    const RTEntry &f = i.value();
#endif
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
  for (RTIter i = n->_rtes.begin(); i; i++) {
#if USE_OLD_SEQ
    unsigned jiff = dsdv_jiffies();
    RTEntry f = i.value();
    if (n->use_old_route(f.dest_ip, jiff))
      f = n->_old_rtes[f.dest_ip];
#else
    const RTEntry &f = i.value();
#endif
    s += f.dest_ip.s() 
      + " next=" + f.next_hop_ip.s() 
      + " hops=" + String((int) f.num_hops()) 
      + " gw=" + (f.is_gateway ? "y" : "n")
      + " seq=" + String(f.seq_no())
      + " if=" + String((int) f.next_hop_interface)
      + "\n";
  }
  
  return s;
}

String
DSDVRouteTable::print_nbrs_v(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;
  
  String s;
  for (RTIter i = n->_rtes.begin(); i; i++) {
    // only print immediate neighbors 
    if (!i.value().dest_eth) 
      continue;
    s += i.key().s();
    s += " eth=" + i.value().dest_eth.s();
    char buf[300];
    snprintf(buf, 300, " metric_valid=%s metric=%d if=%d",
	     i.value().metric.valid ? "yes" : "no", 
	     i.value().metric.val, (int) i.value().next_hop_interface);
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
  for (RTIter i = n->_rtes.begin(); i; i++) {
    // only print immediate neighbors 
    if (!i.value().dest_eth)
      continue;
    s += i.key().s();
    s += " eth=" + i.value().dest_eth.s();
    s += " if=" + String((int) i.value().next_hop_interface);
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
  case MetricHopCount:        return "hopcount"; break;
  case MetricEstTxCount:      return "est_tx_count"; break;
  case MetricDeliveryRateProduct:    return "delivery_rate_product"; break;
  case MetricRevDeliveryRateProduct: return "reverse_delivery_rate_product"; break;
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
  if      (s2 == "hopcount")                return MetricHopCount;
  else if (s2 == "est_tx_count")            return MetricEstTxCount;
  else if (s2 == "delivery_rate_product")   return MetricDeliveryRateProduct;
  else if (s2 == "reverse_delivery_rate_product")   return MetricRevDeliveryRateProduct;
  else return MetricUnknown;
}

int
DSDVRouteTable::write_metric_type(const String &arg, Element *el, 
				  void *, ErrorHandler *errh)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  String s;
  if (!cp_word(arg, &s))
    return errh->error("unable to get metric type from argument ``%s''", ((String) arg).cc());
  MetricType type = check_metric_type(s);
  if (type < 0)
    return errh->error("unknown metric type ``%s''", ((String) arg).cc());
  
  if (type != rt->_metric_type) {
    /* get rid of old metric values */
    Vector<IPAddress> entries;
    for (RTIter i = rt->_rtes.begin(); i; i++) {
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
			       void *, ErrorHandler *errh)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  unsigned est_type = 0;
  if (!cp_unsigned(arg, &est_type))
    return errh->error("est_type must be unsigned");
  switch (est_type) {
  case EstByMeas:
    rt->_est_type = est_type; break;
  default:
    return errh->error("est_type %u is not valid", est_type);
  }
  return 0;
}

String
DSDVRouteTable::print_seqno(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  return String(rt->_seq_no) + "\n";
}

int
DSDVRouteTable::write_seqno(const String &arg, Element *el, 
			       void *, ErrorHandler *errh)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  unsigned u;
  if (!cp_unsigned(arg, &u)) 
    return errh->error("sequence number must be unsigned");
  if (u & 1)
    return errh->error("sequence number must be even");
  rt->_seq_no = u;
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
			     void *, ErrorHandler *errh)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  if (!cp_bool(arg, &rt->_frozen))
    return errh->error("`frozen' must be a boolean");
  
  click_chatter("DSDVRouteTable %s: setting _frozen to %s", 
		rt->id().cc(), rt->_frozen ? "true" : "false");
  return 0;
}

#if USE_OLD_SEQ
String
DSDVRouteTable::print_use_old_route(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  return (rt->_use_old_route ? "true\n" : "false\n");
}

int
DSDVRouteTable::write_use_old_route(const String &arg, Element *el, 
			     void *, ErrorHandler *errh)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) el;
  if (!cp_bool(arg, &rt->_use_old_route))
    return errh->error("`use_old_route' must be a boolean");
  
  click_chatter("DSDVRouteTable %s: setting _use_old_route to %s", 
		rt->id().cc(), rt->_use_old_route ? "true" : "false");
  return 0;
}
#endif

String
DSDVRouteTable::print_dump(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  StringAccum sa;
  for (RTIter i = rt->_rtes.begin(); i; i++) {
    sa << i.value().dump() << "\n";
  }

  return sa.take_string();
}

String
DSDVRouteTable::print_links(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  
  String s = "Metric type: " + metric_type_to_string(rt->_metric_type) + "\n";

  for (RTIter i = rt->_rtes.begin(); i; i++) {
    const RTEntry &r = i.value();
    if (!r.dest_eth)
      continue;

    // XXX what the heck are we trying to do here?
#if 0
    char buf[255];
    int tx_rate = 100 * num_rx;
    tx_rate -= 50;
    tx_rate /= num_expected;
    snprintf(buf, 255, "%s %s metric=%u (%s) rx_sig=%d rx_qual=%d rx_rate=%d tx_sig=%d tx_qual=%d tx_rate=%d\n",
	     r.dest_ip.s().cc(), r.next_hop_eth.s().cc(), r.metric.val, r.metric.valid ? "valid" : "invalid",
	     s1 ? s1->sig : -1, s1 ? s1->qual : -1, res1 ? tx_rate : -1,
	     res2 ? tx_sig : -1, res2 ? tx_qual : -1, res3 ? (int) bcast_rate : -1);
    s += buf;
#endif
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
  add_read_handler("seqno", print_seqno, 0);
  add_write_handler("seqno", write_seqno, 0);
  add_read_handler("frozen", print_frozen, 0);
  add_write_handler("frozen", write_frozen, 0);
#if USE_OLD_SEQ
  add_read_handler("use_old_route", print_use_old_route, 0);
  add_write_handler("use_old_route", write_use_old_route, 0);
#endif
  add_read_handler("dump", print_dump, 0);
}

void
DSDVRouteTable::hello_hook()
{
  unsigned int msecs_to_next_ad = _period;

  unsigned int jiff = dsdv_jiffies();
  dsdv_assert(jiff >= _last_periodic_update);
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
  long r2 = random();
  long r = (r2 >> 1);
  unsigned int jitter = (unsigned int) (r % (_jitter + 1));
  if (r2 & 1) {
    if (jitter <= msecs_to_next_ad) // hello can only happen in the future
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

  dsdv_assert(psz <= 1500);
  if (num_rtes < rtes_to_send.size())
    click_chatter("DSDVRouteTable %s: too many routes, truncating route advertisement",
		  id().cc());

  /* allocate and align the packet */
  WritablePacket *p = Packet::make(psz + 2); // for alignment
  if (p == 0) {
    click_chatter("DSDVRouteTable %s: cannot make packet!", id().cc());
    dsdv_assert(0);
  } 
  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  /* fill in the timestamp */
  struct timeval tv;
  click_gettimeofday(&tv);
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
  dsdv_assert(num_rtes <= 255);
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

  unsigned int jiff = dsdv_jiffies();
  unsigned int ttl_decrement = jiff_to_msec(good() ? jiff - last_updated_jiffies : jiff - last_expired_jiffies);
  nb->ttl = htonl(decr_ttl(ttl, max(ttl_decrement, grid_hello::MIN_TTL_DECREMENT)));
  
  /* ping-pong link stats back to sender */
#ifndef SMALL_GRID_HEADERS
  nb->link_qual = 0;
  nb->link_sig = 0;
  nb->measurement_time.tv_sec = nb->measurement_time.tv_usec = 0;
  if (ls && num_hops() == 1) {
#if 0 // #ifdef CLICK_USERLEVEL
    LinkStat::stat_t *s = ls ? ls->_stats.findp(next_hop_eth) : 0;
#else
    struct {
      int qual;
      int sig;
      struct timeval when;
    } *s = 0;
#endif
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
#else
  ls = 0; // supress warning
#endif
}

void
DSDVRouteTable::log_dump_hook(bool reschedule)
{
  if (_log) {
    struct timeval tv;
    click_gettimeofday(&tv);
    Vector<RouteEntry> vec;
    get_all_entries(vec);
    _log->log_route_dump(vec, tv);
  }
  if (reschedule)
    _log_dump_timer.schedule_after_ms(_log_dump_period); 
}

String
DSDVRouteTable::RTEntry::dump() const
{
  check();

  unsigned int jiff = dsdv_jiffies();
  
  StringAccum sa;
  sa << "  curr jiffies: "  << jiff << "\n"
     << "       dest_ip: " << dest_ip.s().cc() << "\n"
     << "      dest_eth: " << dest_eth.s().cc() << "\n"
     << "   next_hop_ip: " << next_hop_ip.s().cc() << "\n"
     << "  next_hop_eth: " << next_hop_eth.s().cc() << "\n"
     << "next_hop_iface: " << next_hop_interface << "\n"
     << "        seq_no: " << seq_no() << "\n"
     << "      num_hops: " << (unsigned int) num_hops() << "\n"
     << "  last_updated: " << jiff_diff_string(last_updated_jiffies, jiff).cc() << "\n"
     << "  last_expired: " << jiff_diff_string(last_expired_jiffies, jiff).cc() << "\n"
     << "      last_seq: " << jiff_diff_string(last_seq_jiffies, jiff).cc() << "\n"
     << "  advertise_ok: " << jiff_diff_string(advertise_ok_jiffies, jiff).cc() << "\n"
     << "        metric: " << metric.val << "\n"
     << "need_metric_ad: " << need_metric_ad << "\n"
     << "   need_seq_ad: " << need_seq_ad << "\n"
     << "           wst: " << (unsigned int) wst << "\n";
  return sa.take_string();
}

void
DSDVRouteTable::check_invariants(const IPAddress *ignore) const
{
  for (RTIter i = _rtes.begin(); i; i++) {
    const RTEntry &r = i.value();

    r.check();

    if (ignore && *ignore == i.key())
      continue;

    // check expire timer invariants
    Timer **t = _expire_timers.findp(r.dest_ip);
    HookPair **hp = _expire_hooks.findp(r.dest_ip);

    if (r.good()) {
      dsdv_assert(t);
      dsdv_assert(*t);
      dsdv_assert((*t)->scheduled());
      dsdv_assert(hp);
      dsdv_assert(*hp);
      dsdv_assert((*hp)->obj == this);
      dsdv_assert((*hp)->ip == (unsigned int) r.dest_ip);
    }
    else {
      dsdv_assert(!t);
      dsdv_assert(!hp);
    }      

    // check trigger timer invariants
    t = _trigger_timers.findp(r.dest_ip);
    hp = _trigger_hooks.findp(r.dest_ip);
    if (t) {
      dsdv_assert(*t);
      dsdv_assert((*t)->scheduled());
      dsdv_assert(hp);
      dsdv_assert(*hp);
      dsdv_assert((*hp)->obj == this);
      dsdv_assert((*hp)->ip == (unsigned int) r.dest_ip);
      // dsdv_assert(r.need_seq_ad || r.need_metric_ad); // see note for trigger invariants
    }
    else {
      dsdv_assert(!hp);
    }
  }
}

void
DSDVRouteTable::dsdv_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("DSDVRouteTable %s assertion \"%s\" failed: file %s, line %d",
		id().cc(), expr, file, line);
  click_chatter("Routing table state:");
  for (RTIter i = _rtes.begin(); i; i++) {
    click_chatter("%s\n", i.value().dump().cc());
  }
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif
}

ELEMENT_PROVIDES(GridGenericRouteTable)
EXPORT_ELEMENT(DSDVRouteTable)

#include <click/bighashmap.cc>
#include <click/vector.cc>
template class BigHashMap<IPAddress, DSDVRouteTable::RTEntry>;
template class BigHashMap<IPAddress, Timer *>;
template class Vector<DSDVRouteTable::RTEntry>;
CLICK_ENDDECLS
