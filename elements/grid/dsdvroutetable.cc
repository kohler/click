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
#include "gridlogger.hh"

const DSDVRouteTable::metric_t DSDVRouteTable::_bad_metric; // default metric state is ``bad''
DSDVRouteTable *DSDVRouteTable::_instance = 0;

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
  GridGenericRouteTable(1, 1), _log(0), 
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
  assert(!_instance);
  _instance = this;
  MOD_INC_USE_COUNT;
}

DSDVRouteTable::~DSDVRouteTable()
{
  MOD_DEC_USE_COUNT;
  if (_log)
    delete _log;

  if (GridLogger::log_is_open())
    GridLogger::close_log();
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
			cpEthernetAddress, "source Ethernet address", &_eth,
			cpIPAddress, "source IP address", &_ip,
			cpElement, "GridGatewayInfo element", &_gw_info,
			cpElement, "LinkTracker element", &_link_tracker,
			cpElement, "LinkStat element", &_link_stat,
			cpKeywords,
			"MAX_HOPS", cpUnsigned, "max hops", &_max_hops,
			"METRIC", cpString, "route metric", &metric,
			"LOGFILE", cpString, "binary log file", &logfile,
			"WST0", cpUnsigned, "initial weight settling time, wst0 (msec)", &_wst0,
			"ALPHA", cpDouble, "alpha parameter for settling time computation", &_alpha,
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

  _log = GridLogger::get_log();
  if (logfile.length() > 0) 
    GridLogger::open_log(logfile);
  
  return res;
}

int
DSDVRouteTable::initialize(ErrorHandler *)
{
  _hello_timer.initialize(this);
  _hello_timer.schedule_after_ms(_period);
  _log_dump_timer.initialize(this);
  _log_dump_timer.schedule_after_ms(_log_dump_period); 
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
    bool res = _link_tracker->get_bcast_stat(ip, rate, last);
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
    if (r == 0 || r->num_hops > 1)
      return false;
    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res = _link_stat->get_bcast_stats(r->next_hop_eth, last, window, num_rx, num_expected);
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
DSDVRouteTable::insert_route(const RTEntry &r)
{
  Timer **old = _expire_timers.findp(r.dest_ip);
  RTEntry *old_r = _rtes.findp(r.dest_ip);

  // invariant check: running timers exist for all current good
  // routes.  no timers or bogus timer entries exist for bad routes.
  if (old_r && old_r->num_hops > 0)
    assert(old && *old && (*old)->scheduled());
  else 
    assert(old == 0);

  if (old) {
    (*old)->unschedule();
    delete *old;
  }
  
  Timer *t = new Timer(static_expire_hook, (void *) ((unsigned int) r.dest_ip));
  t->initialize(this);
  t->schedule_after_ms(_timeout > r.ttl ? r.ttl : _timeout);
  
  _expire_timers.insert(r.dest_ip, t);
  _rtes.insert(r.dest_ip, r);
}

void
DSDVRouteTable::expire_hook(const IPAddress &ip)
{
  if (_frozen)
    return;

  // invariant check:
  // 1. route to expire should exist
  RTEntry *r = _rtes.findp(ip);
  assert(r != 0);
  
  // 2. route to expire should be good
  assert(r->num_hops > 0 && (r->seq_no & 1) == 0);
  
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

  Vector<RTEntry> expired_routes;
  expired_routes.push_back(*r);
  
  // Only do the next-hop expiration checks if this node is actually
  // its own next hop.  With hopcount, it is always the case that if a
  // node n1 is some node n2's next hop, then n1 is its own next hop;
  // therefore the if() will be true.  For other metrics, there can be
  // cases where we should not expire n2 just because n1 expires, if
  // n1 is not its own next hop.
  if (r->num_hops == 1) {
    for (RTIter i = _rtes.first(); i; i++) {
      RTEntry r = i.value();
      if (r.dest_ip == ip)
	continue; // don't expire this dest twice!

      if (r.num_hops > 0 && // only expire good routes
	  r.next_hop_ip == ip) {

	expired_routes.push_back(r);
	  
	if (_log)
	  _log->log_expired_route(GridLogger::NEXT_HOP_EXPIRED, r.dest_ip);
      }
    }
  }

  int jiff = click_jiffies();

  for (int i = 0; i < expired_routes.size(); i++) {
    RTEntry &r = expired_routes[i];

    // invariant check: 
    // 1. route to expire must be good, and thus should have existing
    // expire timer
    Timer **exp_timer = _expire_timers.findp(r.dest_ip);
    assert(exp_timer && *exp_timer);

    // 2. that existing timer should still be scheduled, except for
    // the timer whose expiry called this hook
    assert(r.dest_ip != ip ? (*exp_timer)->scheduled() : true);
    
    // cleanup pending timers
    if ((*exp_timer)->scheduled())
      (*exp_timer)->unschedule();
    delete *exp_timer;
    _expire_timers.remove(r.dest_ip);

    // mark route as broken
    r.num_hops = 0;
    r.seq_no++;

    // set up triggered ad
    r.advertise_ok_jiffies = jiff;
    r.need_advertisement = true;
    schedule_triggered_update(r.dest_ip, jiff);
  }

  if (_log)
    _log->log_end_expire_handler();

}

void
DSDVRouteTable::schedule_triggered_update(const IPAddress &ip, int when)
{
  // get rid of outstanding triggered request (if any)
  Timer **old = _trigger_timers.findp(ip);
  if (old) {
    assert(*old && (*old)->scheduled());
    (*old)->unschedule();
    delete *old;
  }
  
  // set up new timer
  Timer *t = new Timer(static_trigger_hook, (void *) ((unsigned int) ip));
  t->initialize(this);
  int jiff = click_jiffies();
  t->schedule_after_ms(jiff_to_msec(jiff > when ? 0 : when - jiff));
  _trigger_timers.insert(ip, t);
}

void
DSDVRouteTable::trigger_hook(const IPAddress &ip)
{
  // invariant: the trigger timer must exist for this dest, but must
  // not be running
  Timer **old = _trigger_timers.findp(ip);
  assert(old && *old && !(*old)->scheduled());

  int jiff = click_jiffies();
  int next_trigger_time = _last_triggered_update + _min_triggered_update_period;

  if (jiff < next_trigger_time) {
    // it's too early to send this update

    Vector<IPAddress> remove_list;

    // cancel all oustanding triggered updates that would also be too early
    for (TMIter i = _trigger_timers.first(); i; i++) {
      if (i.key() == ip)
	continue; // don't touch this timer, we'll reschedule it

      RTEntry *r = _rtes.findp(i.key());
      assert(r);
      assert(r->need_advertisement);

      if (r->advertise_ok_jiffies < next_trigger_time) {
	Timer *old2 = i.value();
	assert(old2 && old2->scheduled());
	delete old2;
	remove_list.push_back(i.key());
      }
    }

    for (int i = 0; i < remove_list.size(); i++)
      _trigger_timers.remove(remove_list[i]);

    // reschedule this timer to earliest possible time
    (*old)->schedule_after_ms(jiff_to_msec(next_trigger_time - jiff));
  }
  else 
    send_triggered_update(ip);
}

void
DSDVRouteTable::init_metric(RTEntry &r)
{
  assert(r.num_hops == 1);

  switch (_metric_type) {
  case MetricHopCount:
    r.metric = metric_t(r.num_hops);
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
DSDVRouteTable::update_wst(RTEntry *old_r, RTEntry &new_r)
{
  if (old_r == 0) {
    new_r.wst = _wst0;
    new_r.last_seq_jiffies = click_jiffies();
  }
  else if (old_r->seq_no == new_r.seq_no) {
    new_r.wst = old_r->wst;
    new_r.last_seq_jiffies = old_r->last_seq_jiffies;
  }
  else if (old_r->seq_no < new_r.seq_no) {
    new_r.wst = _alpha * old_r->wst + 
      (1 - _alpha) * jiff_to_msec(old_r->last_updated_jiffies - old_r->last_seq_jiffies);
    new_r.last_seq_jiffies = click_jiffies();
  }
}

void 
DSDVRouteTable::update_metric(RTEntry &r)
{
  assert(r.num_hops > 1);

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
		    next_hop->dest_ip.s().cc(), next_hop->metric);
  case MetricEstTxCount: 
    if (_metric_type == MetricEstTxCount) {
      if (r.metric.val < (unsigned) 100 * (r.num_hops - 1))
	click_chatter("update_metric WARNING received metric (%d) too low for %s (%d hops)",
		      r.metric, r.dest_ip.s().cc(), r.num_hops);
      if (next_hop->metric.val < 100)
	click_chatter("update_metric WARNING next hop %s for %s metric is too low (%d)",
		      next_hop->dest_ip.s().cc(), r.dest_ip.s().cc(), next_hop->metric);
    }
    r.metric.val += next_hop->metric.val;
    break;
  default:
    assert(0);
  }
  r.metric.valid = true;
}

bool
DSDVRouteTable::metric_is_preferable(const RTEntry &r1, const RTEntry &r2)
{
  assert(r1.metric.valid && r2.metric.valid);

  switch (_metric_type) {
  case MetricHopCount:
  case MetricEstTxCount: 
    return r1.metric.val < r2.metric.val;
  default:
    assert(0);
  }
  return false;
}

bool
DSDVRouteTable::should_replace_old_route(const RTEntry &old_route, const RTEntry &new_route)
{
  /* prefer a strictly newer route */
  if (old_route.seq_no > new_route.seq_no) 
    return false;
  if (old_route.seq_no < new_route.seq_no)
    return true;
  
  /* 
   * routes have same seqno, choose based on metric 
   */
  
  /* prefer a route with a valid metric */
  if (old_route.metric.valid && !new_route.metric.valid)
    return false;
  if (!old_route.metric.valid && new_route.metric.valid)
    return true;
  
  /* if neither metric is valid, just keep the route we have -- to aid
   * in stability -- as if I have any notion about that....
   *
   * actually, that's fucked.  would you prefer a 5-hop route or a 
   * 2-hop route, given that you don't have any other information about
   * them?  duh.  fall back to hopcount. 
   * bwahhhaaaahahaha!!! */
  if (!old_route.metric.valid && !new_route.metric.valid) {
    // return false;
    return new_route.num_hops < old_route.num_hops;
  }
  
  // both metrics are valid
  /* update is from same node as last update, we should accept it to avoid unwarranted timeout */
  if (old_route.next_hop_ip == new_route.next_hop_ip)
    return true;
   
  /* update route if the metric is better */
  return metric_is_preferable(new_route, old_route);
}

bool
DSDVRouteTable::metric_different(const metric_t &m1, const metric_t &m2)
{
  if (!m1.valid && !m2.valid) return false;
  if (m1.valid && !m2.valid)  return true;
  if (!m1.valid && m2.valid)  return true;
  
  assert(m1.valid && m2.valid);
  switch (_metric_type) {
  case MetricHopCount: return m1.val != m2.val; break;
  case MetricEstTxCount: {
    unsigned diff = (m1.val > m2.val) ? m1.val - m2.val : m2.val - m1.val;
    return diff > 25; // ignore differences in tx count of less than 0.25
  }
  default: assert(0);
  }
  return false;
}

void
DSDVRouteTable::send_full_update() {

  int jiff = click_jiffies();
  Vector<RTEntry> routes;
  
  for (RTIter i = _rtes.first(); i; i++) {
    const RTEntry &r = i.value();
    if (r.advertise_ok_jiffies > jiff)
      continue;
    routes.push_back(r);
  }

  // reset ``need advertisement'' flag
  for (int i = 0; i < routes.size(); i++) {
    RTEntry *r = _rtes.findp(routes[i].dest_ip);
    assert(r);
    r->need_advertisement = false;
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
}

void
DSDVRouteTable::send_triggered_update(const IPAddress &) 
{
  int jiff = click_jiffies();

  Vector<RTEntry> triggered_routes;
  for (RTIter i = _rtes.first(); i; i++) {
    const RTEntry &r = i.value();
    
    if (r.need_advertisement && r.advertise_ok_jiffies <= jiff)
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
    r->need_advertisement = false;
    r->last_adv_metric = r->metric;
  }

  build_and_tx_ad(triggered_routes); 
  _last_triggered_update = jiff;
}

Packet *
DSDVRouteTable::simple_action(Packet *packet)
{
  assert(packet);
  int jiff = click_jiffies();

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
  
  if (_frozen) {
    if (_log)
      _log->log_end_recv_advertisement();
    packet->kill();
    return 0;
  }

  /*
   * we still do the ping-ponging in route ads as well as pigybacking
   * on unicast data, in case we aren't sending data to that
   * destination.  
   */

  /* 
   * individual link metric smoothing, or route metric smoothing?  we
   * will only smooth the ping-pong measurements on individual links;
   * we won't smooth metrics at the route level.  that's because we
   * can't even be sure that as the metrics change for a route to some
   * destination, the metrics are even for the same route, i.e. same
   * set of links.  
   */

  /* look for ping-pong link stats about us */
  int entry_sz = hlo->nbr_entry_sz;
  char *entry_ptr = (char *) (hlo + 1);
  for (int i = 0; i < hlo->num_nbrs; i++, entry_ptr += entry_sz) {
    grid_nbr_entry *curr = (grid_nbr_entry *) entry_ptr;
    if (_ip == curr->ip && curr->num_hops == 1) {
      struct timeval tv;
      tv = ntoh(curr->measurement_time);
      _link_tracker->add_stat(ipaddr, ntohl(curr->link_sig), ntohl(curr->link_qual), tv);
      tv = ntoh(curr->last_bcast);
      _link_tracker->add_bcast_stat(ipaddr, curr->num_rx, curr->num_expected, tv);
      break;
    }
  }


  /*
   * add 1-hop route to packet's transmitter; perform some sanity
   * checking if entry already existed 
   */

  // XXX triggered update and weighted settling time calc needs to be done for the first hop.

  RTEntry *r = _rtes.findp(ipaddr);

  if (!r)
    click_chatter("DSDVRouteTable %s: adding new 1-hop route %s -- %s", 
		  id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
  else if (r->dest_eth && r->dest_eth != ethaddr)
    click_chatter("DSDVRouteTable %s: ethernet address of %s changed from %s to %s", 
		  id().cc(), ipaddr.s().cc(), r->dest_eth.s().cc(), ethaddr.s().cc());

  if (ntohl(hlo->ttl) > 0) {
    RTEntry new_r(ipaddr, ethaddr, gh, hlo, jiff);
    init_metric(new_r);

    if (r && r->seq_no >= new_r.seq_no) {
      click_chatter("DSDVRouteTable %s: sequence number %d of 1-hop ad from %s is too small (should be > %d)",
		    id().cc(), new_r.seq_no, ipaddr.s().cc(), r->seq_no);
      assert(0);
    }

    update_wst(r, new_r);

    if (r == 0 || should_replace_old_route(*r, new_r)) {
      if (_log)
	_log->log_added_route(GridLogger::WAS_SENDER, make_generic_rte(new_r));
      insert_route(new_r);
      if (new_r.num_hops > 1 && r && r->num_hops == 1) {
	/* clear old 1-hop stats */
	_link_tracker->remove_all_stats(r->dest_ip);
	_link_stat->remove_all_stats(r->next_hop_eth);
      }
    }
    if (r)
      r->dest_eth = ethaddr;
  }
  
  /*
   * loop through and process other route entries in hello message 
   */

  bool need_full_update = false;

  entry_ptr = (char *) (hlo + 1);
  for (int i = 0; i < hlo->num_nbrs; i++, entry_ptr += entry_sz) {
    
    grid_nbr_entry *curr = (grid_nbr_entry *) entry_ptr;
    RTEntry route(ipaddr, ethaddr, curr, jiff); // XXX wst,seqno_at, change_at updates?

    /* ignore route if ttl has run out */
    if (route.ttl <= 0)
      continue;

    /* ignore route to ourself */
    if (route.dest_ip == _ip && curr->num_hops > 0)       
      continue;

    /* over-ride bad info about us */
    if (route.dest_ip == _ip && curr->num_hops == 0) {
      need_full_update = true;
      continue;
    }      

    /* pseudo-split-horizon: ignore routes from nbrs that go back
       through us */
    if (curr->next_hop_ip == (unsigned int) _ip)
      continue;

    update_metric(route);

    RTEntry *our_rte = _rtes.findp(curr->ip);
    
    update_wst(our_rte, route);

    if (curr->num_hops > 0)
      route.advertise_ok_jiffies = jiff + msec_to_jiff(2 * route.wst);
    else
      route.advertise_ok_jiffies = jiff;

    /* 
     * broken route advertisement 
     */
    if (curr->num_hops == 0) {
      assert(route.seq_no & 1);
      /* 
       * if we don't have the route, or... if our next hop to the
       * destination is this packet's sender, AND if the seq_no is
       * newer than any information we have, accept the broken route 
       */
      if (our_rte == 0 || (our_rte && 
			   our_rte->next_hop_ip == ipaddr &&
			   route.seq_no > our_rte->seq_no)) {

	insert_route(route);
	if (our_rte)
	  schedule_triggered_update(route.dest_ip, route.advertise_ok_jiffies);

	if (_log)
	  _log->log_expired_route(GridLogger::BROKEN_AD, route.dest_ip);
      }
      /*
       * otherwise, if we have a good route to the destination with a
       * newer seq_no, advertise our new information.  
       */
      else if (our_rte->num_hops > 0 && 
	       our_rte->seq_no > route.seq_no && 
	       our_rte->ttl > 0) {
	assert(!(our_rte->seq_no & 1));
	
	our_rte->advertise_ok_jiffies = jiff;
	schedule_triggered_update(our_rte->dest_ip, jiff);
	if (_log)
	  _log->log_triggered_route(our_rte->dest_ip);
      }
      continue;
    } // end of broken route handling

    /* skip routes with too many hops */
    // this would change if using proxies
    if (route.num_hops + 1U > _max_hops)
      continue;

    /* 
     * regular route entry -- should we accept it?
     */
    if (our_rte == 0 || 
	should_replace_old_route(*our_rte, route)) {
      insert_route(route);

      if (!our_rte || 
	  route.seq_no > our_rte->seq_no || 
	  metric_different(route.metric, our_rte->last_adv_metric))
	schedule_triggered_update(route.dest_ip, route.advertise_ok_jiffies);

      if (_log)
	_log->log_added_route(GridLogger::WAS_ENTRY, make_generic_rte(route));
    }
  }

  if (_log)
    _log->log_end_recv_advertisement();

  if (need_full_update)
    send_full_update();

  packet->kill();
  return 0;
}

String 
DSDVRouteTable::print_rtes_v(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;

  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    const RTEntry &f = i.value();
    s += f.dest_ip.s() 
      + " next=" + f.next_hop_ip.s() 
      + " hops=" + String((int) f.num_hops) 
      + " gw=" + (f.is_gateway ? "y" : "n")
      + " loc=" + f.dest_loc.s()
      + " err=" + (f.loc_good ? "" : "-") + String(f.loc_err) // negate loc if invalid
      + " seq=" + String(f.seq_no)
      + " metric_valid=" + (f.metric.valid ? "yes" : "no")
      + " metric=" + String(f.metric.val)
      + "\n";
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
DSDVRouteTable::print_nbrs_v(Element *e, void *)
{
  DSDVRouteTable *n = (DSDVRouteTable *) e;
  
  String s;
  for (RTIter i = n->_rtes.first(); i; i++) {
    /* only print immediate neighbors */
    if (i.value().num_hops != 1) // XXX
      continue;
    s += i.key().s();
    s += " eth=" + i.value().next_hop_eth.s();
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


int
DSDVRouteTable::write_start_log(const String &arg, Element *, 
				void *, ErrorHandler *errh)
{
  if (GridLogger::log_is_open())
    GridLogger::close_log();
  
  bool res = GridLogger::open_log(arg);
  if (!res) 
    return errh->error("unable to start logging to file %s; any previous logging has been disabled",
		       ((String) arg).cc());
  return 0;
}

int
DSDVRouteTable::write_stop_log(const String &, Element *, 
			       void *, ErrorHandler *)
{
  if (GridLogger::log_is_open())
    GridLogger::close_log();
  return 0;
}

String
DSDVRouteTable::print_links(Element *e, void *)
{
  DSDVRouteTable *rt = (DSDVRouteTable *) e;
  
  String s = "Metric type: " + metric_type_to_string(rt->_metric_type) + "\n";

  for (RTIter i = rt->_rtes.first(); i; i++) {
    const RTEntry &r = i.value();
    if (r.num_hops > 1)
      continue;

    /* get our measurements of the link *from* this neighbor */
    LinkStat::stat_t *s1 = rt->_link_stat->_stats.findp(r.next_hop_eth);
    struct timeval last;
    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res1 = rt->_link_stat->get_bcast_stats(r.next_hop_eth, last, window, num_rx, num_expected);

    /* get estimates of our link *to* this neighbor */
    int tx_sig = 0;
    int tx_qual = 0;
    bool res2 = rt->_link_tracker->get_stat(r.dest_ip, tx_sig, tx_qual, last);
    double bcast_rate = 0;
    bool res3 = rt->_link_tracker->get_bcast_stat(r.dest_ip, bcast_rate, last);

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
  add_write_handler("start_log", write_start_log, 0);
  add_write_handler("stop_log", write_stop_log, 0);
  add_read_handler("frozen", print_frozen, 0);
  add_write_handler("frozen", write_frozen, 0);
}

void
DSDVRouteTable::hello_hook()
{
  int msecs_to_next_ad = _period;

  int jiff = click_jiffies();
  unsigned int msec_since_last = jiff_to_msec(jiff - _last_periodic_update);
  if (msec_since_last < 2 * _period / 3) {
    // a full periodic update was sent ahead of schedule (because
    // there were so many triggered updates to send).  reschedule this
    // period update to one period after the last periodic update
    
    int jiff_period = msec_to_jiff(_period);
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
  int jitter = (int) (((double) _jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  _hello_timer.schedule_after_ms(msecs_to_next_ad + (int) jitter);
}


void
DSDVRouteTable::build_and_tx_ad(Vector<RTEntry> &rtes_to_send)
{
  /*
   * build and send routing update packet advertising the contents of
   * the rtes_to_send vector.  
   */

  if (_frozen)
    return;

  int hdr_sz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_hello);
  int max_rtes = (1500 - hdr_sz) / sizeof(grid_nbr_entry);
  int num_rtes = (max_rtes < rtes_to_send.size() ? max_rtes : rtes_to_send.size()); // min
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

  hlo->is_gateway = _gw_info->is_gateway ();

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
DSDVRouteTable::RTEntry::fill_in(grid_nbr_entry *nb, LinkStat *ls)
{
  nb->ip = dest_ip;
  nb->next_hop_ip = next_hop_ip;
  nb->num_hops = num_hops;
  nb->loc = dest_loc;
  nb->loc_err = htons(loc_err);
  nb->loc_good = loc_good;
  nb->seq_no = htonl(seq_no);
  nb->metric = htonl(metric.val);
  nb->metric_valid = metric.valid;
  nb->is_gateway = is_gateway;
  nb->ttl = htonl(ttl);
  
  /* ping-pong link stats back to sender */
  nb->link_qual = 0;
  nb->link_sig = 0;
  nb->measurement_time.tv_sec = nb->measurement_time.tv_usec = 0;
  if (ls && num_hops == 1) {
    LinkStat::stat_t *s = ls->_stats.findp(next_hop_eth);
    if (s) {
      nb->link_qual = htonl(s->qual);
      nb->link_sig = htonl(s->sig);
      nb->measurement_time.tv_sec = htonl(s->when.tv_sec);
      nb->measurement_time.tv_usec = htonl(s->when.tv_usec);
    }
    else
      click_chatter("DSDVRouteTable: error!  unable to get signal strength or quality info for one-hop neighbor %s\n",
		    IPAddress(dest_ip).s().cc());

    nb->num_rx = 0;
    nb->num_expected = 0;
    nb->last_bcast.tv_sec = nb->last_bcast.tv_usec = 0;
    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res = ls->get_bcast_stats(next_hop_eth, nb->last_bcast, window, num_rx, num_expected);
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
DSDVRouteTable::log_dump_hook()
{
  if (_log) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    Vector<RouteEntry> vec;
    get_all_entries(vec);
    _log->log_route_dump(vec, tv);
  }
  _log_dump_timer.schedule_after_ms(_log_dump_period); 
}

ELEMENT_REQUIRES(userlevel)
ELEMENT_REQUIRES(gridlogger)
EXPORT_ELEMENT(DSDVRouteTable)

#include <click/bighashmap.cc>
template class BigHashMap<IPAddress, DSDVRouteTable::RTEntry>;
template class BigHashMap<IPAddress, Timer *>;
#include <click/vector.cc>
template class Vector<DSDVRouteTable::RTEntry>;

