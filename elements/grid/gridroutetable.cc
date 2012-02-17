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
#include "timeutils.hh" /* includes <cmath> which may #undef NULL, so
			   must become before <cstddef> */
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/standard/scheduleinfo.hh>
#include <click/router.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include "gridroutetable.hh"
#include "gridlogger.hh"
CLICK_DECLS

#define NEXT_HOP_ETH_FIXUP 0

bool
GridRouteTable::get_one_entry(const IPAddress &dest_ip, RouteEntry &entry)
{
  RTEntry *r = _rtes.findp(dest_ip);
  if (r == 0)
    return false;

  entry = RouteEntry(dest_ip, r->loc_good, r->loc_err, r->loc,
		     r->next_hop_eth, r->next_hop_ip,
		     0, // ignore interface number info
		     r->seq_no(), r->num_hops());
  return true;
}

void
GridRouteTable::get_all_entries(Vector<RouteEntry> &vec)
{
  for (RTIter iter = _rtes.begin(); iter.live(); iter++) {
    const RTEntry &rte = iter.value();
    vec.push_back(RouteEntry(rte.dest_ip, rte.loc_good, rte.loc_err, rte.loc,
			     rte.next_hop_eth, rte.next_hop_ip, 0, // ignore interface info
			     rte.seq_no(), rte.num_hops()));
  }
}


GridRouteTable::GridRouteTable() :
  _log(0), _dump_tick(0),
  _seq_no(0), _fake_seq_no(0), _bcast_count(0),
  _seq_delay(1),
  _max_hops(3),
  _expire_timer(expire_hook, this),
  _hello_timer(hello_hook, this),
  _metric_type(MetricEstTxCount),
  _max_metric(0), _min_metric(0),
  _est_type(EstBySigQual),
  _frozen(false)
{
}

GridRouteTable::~GridRouteTable()
{
}



void *
GridRouteTable::cast(const char *n)
{
  if (strcmp(n, "GridRouteTable") == 0)
    return (GridRouteTable *) this;
  else if (strcmp(n, "GridGenericRouteTable") == 0)
    return (GridGenericRouteTable *) this;
  else
    return 0;
}



void
GridRouteTable::log_route_table ()
{
  char str[80];
  for (RTIter i = _rtes.begin(); i.live(); i++) {
    const RTEntry &f = i.value();

    snprintf(str, sizeof(str),
	     "%s %s %s %d %c %u\n",
	     f.dest_ip.unparse().c_str(),
	     f.loc.s().c_str(),
	     f.next_hop_ip.unparse().c_str(),
	     f.num_hops(),
	     (f.is_gateway ? 'y' : 'n'),
	     f.seq_no());
    _extended_logging_errh->message(str);
  }
  _extended_logging_errh->message("\n");
}


int
GridRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String chan("routelog");
  String metric("est_tx_count");
  int res = Args(conf, this, errh)
      .read_mp("TIMEOUT", _timeout)
      .read_mp("PERIOD", _period)
      .read_mp("JITTER", _jitter)
      .read_mp("ETH", _eth)
      .read_mp("IP", _ip)
      .read_mp("GATEWAYINFO", reinterpret_cast<Element *&>(_gw_info))
      .read_mp("LINKTRACKER", reinterpret_cast<Element *&>(_link_tracker))
      .read_mp("LINKSTAT", reinterpret_cast<Element *&>(_link_stat))
      .read("MAX_HOPS", _max_hops)
      .read("LOGCHANNEL", chan)
      .read("METRIC", metric)
      .read("LOG", reinterpret_cast<Element *&>(_log))
      .complete();

  if (res < 0)
    return res;

  // convert msecs to jiffies
  if (_timeout == 0)
    _timeout = -1;
  if (_timeout > 0) {
    _timeout_jiffies = msec_to_jiff(_timeout);
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

  _extended_logging_errh = router()->chatter_channel(chan);
  assert(_extended_logging_errh);

  _metric_type = check_metric_type(metric);
  if (_metric_type < 0)
    return errh->error("Unknown metric type ``%s''", metric.c_str());

  return res;
}



int
GridRouteTable::initialize(ErrorHandler *)
{
  _hello_timer.initialize(this);
  _hello_timer.schedule_after_msec(_period); // Send periodically

  _expire_timer.initialize(this);
  if (_timeout > 0)
    _expire_timer.schedule_after_msec(EXPIRE_TIMER_PERIOD);

  return 0;
}


bool
GridRouteTable::current_gateway(RouteEntry &entry)
{
  for (RTIter i = _rtes.begin(); i.live(); i++) {
    const RTEntry &f = i.value();

    if (f.is_gateway) {
      entry = RouteEntry(f.dest_ip, f.loc_good, f.loc_err, f.loc,
			 f.next_hop_eth, f.next_hop_ip, 0, // ignore interface info
			 f.seq_no(), f.num_hops());
      return true;
      }
  }

  return false;
}


unsigned int
GridRouteTable::qual_to_pct(int q)
{
  /* smaller quality is better, so should be a higher pct when closer to min quality */
  if (q > _max_metric)
    return 0;
  else if (q < _min_metric)
    return 100;

  int delta = _max_metric - _min_metric;
  return (100 * (_max_metric - q)) / delta;
}

unsigned int
GridRouteTable::sig_to_pct(int s)
{
  /* large signal is better, so should be a higher pct when closer to max sig */
  if (s > _max_metric)
    return 100;
  else if (s < _min_metric)
    return 0;

  int delta = _max_metric - _min_metric;
  return (100 * (s - _min_metric)) / delta;
}

// #define h(x) click_chatter("XXXX %d", x);
#define h(x)

bool
GridRouteTable::est_forward_delivery_rate(const IPAddress ip, double &rate)
{
  switch (_est_type) {
  case EstBySig:
  case EstByQual:
  case EstBySigQual: {
    int sig = 0;
    int qual = 0;
    Timestamp last;
    bool res = _link_tracker->get_stat(ip, sig, qual, last);
    if (!res) {
      h(1);
      return false;
    }
    if (_est_type == EstByQual) {
      return false;
    }
    else if (_est_type == EstBySig) {
      return false;
    }
    else if (_est_type == EstBySigQual) {
      h(2);
#if 0
      click_chatter("XXX %s", ip.unparse().c_str());
#endif
      /*
       * use jinyang's parameters, based on 1sec broadcast loss rates
       * with 50-byte UDP packets.
       *
       * good = delivery rate > 80%.
       *
       * these parameters are chosen to correctly classify 85% of good
       * links as good, while only classifying 2% of bad links as
       * good.
       */
      res = _link_tracker->get_bcast_stat(ip, rate, last);
      double z = 9.4519 + 0.0391 * sig + 0.5518 * qual;
      double z2 = 1 / (1 + exp(z));
      double thresh = 0.8;
      bool link_good = z2 > thresh;
      /* paper fuckedness: */
      if (link_good)
	rate = 1;
      else
	rate = 0.1;
      return true;
      h(3);
      if (!link_good && !res) {
	h(4);
	return false;
      }
      else if (link_good && !res) {
	h(5);
	rate = 0.8;
	return true;
      }
      else if (!link_good && res) {
	h(6);
	return true; /* rate was set in call to get_bcast_stat */
      }
      else /* link_good && res */ {
	h(7);
	if (rate < 0.8) {
	  h(8);
	  rate = 0.8;
	}
	return true;
      }
      h(9);
    }
    else {
      h(10);
      return false;
    }
  }
  case EstByMeas: {
    h(11);
    Timestamp last;
    bool res = _link_tracker->get_bcast_stat(ip, rate, last);
    return res;
    break;
  }
  default:
    h(12);
    return false;
  }
}

bool
GridRouteTable::est_reverse_delivery_rate(const IPAddress ip, double &rate)
{
  switch (_est_type) {
  case EstBySig:
  case EstByQual:
    h(101);
    return false;
    break;
  case EstBySigQual: {
    h(102);
    RTEntry *r = _rtes.findp(ip);
    if (r == 0 || r->num_hops() > 1) {
      h(103);
      return false;
    }
#if 0
    struct timeval last;
#if 0
    LinkStat::stat_t *s = _link_stat->_stats.findp(r->next_hop_eth);
#else
    struct {
      int qual;
      int sig;
      struct timeval when;
    } *s = 0;
#endif
    if (s == 0) {
      return false;
      h(104);
    }
    double z = 9.4519 + 0.0391 * s->sig + 0.5518 * s->qual;
    double z2 = 1 / (1 + exp(z));
    double thresh = 0.8;
    bool link_good = z2 > thresh;

    /* paper fuckedness: */
    if (link_good)
      rate = 1;
    else
      rate = 0.1;
    return true;

    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res = false;_link_stat->get_bcast_stats(r->next_hop_eth, last, window, num_rx, num_expected);
    if (res && num_expected > 1) {
      h(105);
      double num_rx_ = num_rx;
      /* we assume all nodes have same hello period */
      double num_expected_ = num_expected;
      rate = (num_rx_ - 0.5) / num_expected_;
    }
    else {
      h(107);
      rate = 0;
    }
    if (!link_good && !res) {
      h(108);
      return false;
    }
    else if (link_good && !res) {
      h(109);
      rate = 0.8;
      return true;
    }
    else if (!link_good && res) {
      h(110);
      return true; /* rate was set in call to get_bcast_stats */
    }
    else /* link_good && res */ {
      h(111);
      if (rate < 0.8) {
	h(112);
	rate = 0.8;
      }
      return true;
    }
#else
    rate = 0;
    return false;
#endif
  }
  h(113);
  case EstByMeas: {
#if 0
    struct timeval last;
    RTEntry *r = _rtes.findp(ip);
    if (r == 0 || r->num_hops() > 1)
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
		    num_rx, num_expected, r->next_hop_eth.unparse().c_str());
    rate = (num_rx_ - 0.5) / num_expected_;
#endif
    return true;
    break;
  }
  default:
    return false;
  }
}


void
GridRouteTable::init_metric(RTEntry &r)
{
  assert(r.num_hops() == 1);

  switch (_metric_type) {
  case MetricHopCount:
    r.metric = r.num_hops();
    r.metric_valid = true;
    break;
  case MetricMinDeliveryRate:
  case MetricCumulativeDeliveryRate:
    assert(0);
    /* code to estimate our delivery rate to this 1-hop nbr goes here */
    r.metric_valid = true;
    break;
  case MetricMinSigStrength:
  case MetricMinSigQuality:
  case MetricCumulativeQualPct:
  case MetricCumulativeSigPct: {
    int sig = 0;
    int qual = 0;
    Timestamp last;
    bool res = _link_tracker->get_stat(r.dest_ip, sig, qual, last);
    if (!res) {
      click_chatter("GridRouteTable: no link sig/qual stats from 1-hop neighbor %s; not initializing metric\n",
		    r.dest_ip.unparse().c_str());
      r.metric = _bad_metric;
      r.metric_valid = false;
      return;
    }
    Timestamp now = Timestamp::now() - last;
    int delta_ms = now.msecval();
    if (delta_ms > _timeout) {
      click_chatter("GridRouteTable: link sig/qual stats from 1-hop neighbor %s are too old; not initializing metric\n",
		    r.dest_ip.unparse().c_str());
      r.metric = _bad_metric;
      r.metric_valid = false;
      return;
    }
    if (_metric_type == MetricMinSigQuality)
      r.metric = (unsigned int) qual;
    else if (_metric_type == MetricMinSigStrength)
      r.metric = (unsigned int) -sig; // deal in -dBm
    else if (_metric_type == MetricCumulativeQualPct)
      r.metric = qual_to_pct(qual);
    else // _metric_type == MetricCumulativeSigPct
      r.metric = sig_to_pct(sig);
    r.metric_valid = true;
  }
  break;
  case MetricEstTxCount: {
#if 0
    click_chatter("XXX");
#endif
    double fwd_rate = 0;
    double rev_rate = 0;
    bool res = est_forward_delivery_rate(r.next_hop_ip, fwd_rate);
    bool res2 = est_reverse_delivery_rate(r.next_hop_ip, rev_rate);
#if 0
    char buf[255];
    snprintf(buf, 255, "YYY %s %s -- res: %s, res2: %s, fwd: %f, rev: %f",
	     r.dest_ip.unparse().c_str(), r.next_hop_ip.s().c_str(),
	     res ? "true" : "false", res2 ? "true" : "false",
	     fwd_rate, rev_rate);
    click_chatter(buf);
#endif
    if (res && res2 && fwd_rate > 0 && rev_rate > 0) {
      if (fwd_rate >= 1) {
	click_chatter("init_metric ERROR: fwd rate %d is too high for %s",
		      (int) (100 * fwd_rate), r.next_hop_ip.unparse().c_str());
	fwd_rate = 1;
      }
      if (rev_rate >= 1) {
	click_chatter("init_metric ERROR: rev rate %d is too high for %s",
		      (int) (100 * rev_rate), r.next_hop_ip.unparse().c_str());
	rev_rate = 1;
      }
      r.metric = (int) (100 / (fwd_rate * rev_rate));
      if (r.metric < 100)
	click_chatter("init_metric WARNING: metric too small (%d) for %s",
		      r.metric, r.next_hop_ip.unparse().c_str());
      r.metric_valid = true;
      h(201);
    }
    else {
      r.metric = _bad_metric;
      r.metric_valid = false;
      h(202);
    }
    break;
  }
  default:
    assert(0);
  }
}

void
GridRouteTable::update_metric(RTEntry &r)
{
  if (r.num_hops() == 0)
    return; // is broken route ad

  RTEntry *next_hop = _rtes.findp(r.next_hop_ip);
  if (!next_hop) {
    click_chatter("GridRouteTable: ERROR updating metric for %s; no information for next hop %s; invalidating metric",
		  r.dest_ip.unparse().c_str(), r.next_hop_ip.unparse().c_str());
    r.metric_valid = false;
    return;
  }

  if (!r.metric_valid)
    return;

  if (!next_hop->metric_valid) {
    r.metric_valid = false;
    return;
  }

  switch (_metric_type) {
  case MetricHopCount:
    if (next_hop->metric > 1)
      click_chatter("GridRouteTable: WARNING metric type is hop count but next-hop %s metric is > 1 (%u)",
		    next_hop->dest_ip.unparse().c_str(), next_hop->metric);
  case MetricEstTxCount:
    if (_metric_type == MetricEstTxCount) {
      if (r.metric < (unsigned) 100 * (r.num_hops() - 1))
	click_chatter("update_metric WARNING received metric (%d) too low for %s (%d hops)",
		      r.metric, r.dest_ip.unparse().c_str(), r.num_hops());
      if (next_hop->metric < 100)
	click_chatter("update_metric WARNING next hop %p{ip_ptr} for %p{ip_ptr} metric is too low (%d)",
		      &next_hop->dest_ip, &r.dest_ip, next_hop->metric);
    }
    r.metric += next_hop->metric;
    break;
  case MetricCumulativeDeliveryRate:
  case MetricCumulativeQualPct:
  case MetricCumulativeSigPct:
    r.metric = (r.metric * next_hop->metric) / 100;
    break;
  case MetricMinDeliveryRate:
    r.metric = (next_hop->metric < r.metric) ? next_hop->metric : r.metric;
    break;
  case MetricMinSigStrength:
  case MetricMinSigQuality:
    // choose weakest signal, which is largest -dBm
    // *or* choose worst quality, which is smaller numbers
    r.metric = (next_hop->metric > r.metric) ? next_hop->metric : r.metric;
    break;
  default:
    assert(0);
  }
  r.metric_valid = true;
}


bool
GridRouteTable::metric_is_preferable(const RTEntry &r1, const RTEntry &r2)
{
  assert(r1.metric_valid && r2.metric_valid);

  switch (_metric_type) {
  case MetricHopCount:
  case MetricEstTxCount:
    return r1.metric < r2.metric;
  case MetricCumulativeDeliveryRate:
  case MetricMinDeliveryRate:
    return r1.metric > r2.metric;
  case MetricMinSigQuality:
  case MetricMinSigStrength:
    // smaller -dBm is stronger signal
    // *or* prefer smaller quality number
    return r1.metric < r2.metric;
  case MetricCumulativeQualPct:
  case MetricCumulativeSigPct:
  default:
    assert(0);
  }
  return false;
}


bool
GridRouteTable::should_replace_old_route(const RTEntry &old_route, const RTEntry &new_route)
{
  /* prefer a strictly newer route */
  if (old_route.seq_no() > new_route.seq_no())
    return false;
  if (old_route.seq_no() < new_route.seq_no())
    return true;

  /*
   * routes have same seqno, choose based on metric
   */

  /* prefer a route with a valid metric */
  if (old_route.metric_valid && !new_route.metric_valid)
    return false;
  if (!old_route.metric_valid && new_route.metric_valid)
    return true;

  /* if neither metric is valid, just keep the route we have -- to aid
   * in stability -- as if I have any notion about that....
   *
   * actually, that's fucked.  would you prefer a 5-hop route or a
   * 2-hop route, given that you don't have any other information about
   * them?  duh.  fall back to hopcount.
   * bwahhhaaaahahaha!!! */
  if (!old_route.metric_valid && !new_route.metric_valid) {
    // return false;
    return new_route.num_hops() < old_route.num_hops();
  }

  // both metrics are valid
  /* update is from same node as last update, we should accept it to avoid unwarranted timeout */
  if (old_route.next_hop_ip == new_route.next_hop_ip)
    return true;

  /* update route if the metric is better */
  return metric_is_preferable(new_route, old_route);
}


/*
 * expects grid LR packets, with ethernet and grid hdrs
 */
Packet *
GridRouteTable::simple_action(Packet *packet)
{
  assert(packet);
  unsigned int jiff = click_jiffies();

  /*
   * sanity check the packet, get pointers to headers
   */
  click_ether *eh = (click_ether *) packet->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("GridRouteTable %s: got non-Grid packet type", name().c_str());
    packet->kill();
    return 0;
  }
  grid_hdr *gh = (grid_hdr *) (eh + 1);

  if (gh->type != grid_hdr::GRID_LR_HELLO) {
    click_chatter("GridRouteTable %s: received unknown Grid packet; ignoring it", name().c_str());
    packet->kill();
    return 0;
  }

  IPAddress ipaddr((unsigned char *) &gh->tx_ip);
  EtherAddress ethaddr((unsigned char *) eh->ether_shost);

  // this should be redundant (see HostEtherFilter in grid.click)
  if (ethaddr == _eth) {
    click_chatter("GridRouteTable %s: received own Grid packet; ignoring it", name().c_str());
    packet->kill();
    return 0;
  }

  grid_hello *hlo = (grid_hello *) (gh + 1);

  // extended logging
  Timestamp ts = Timestamp::now();
  _extended_logging_errh->message("recvd %u from %s %d %d", ntohl(hlo->seq_no), ipaddr.unparse().c_str(), ts.sec(), ts.usec());
  if (_log)
    _log->log_start_recv_advertisement(ntohl(hlo->seq_no), ipaddr, ts);

  if (_frozen) {
    if (_log)
      _log->log_end_recv_advertisement();
    packet->kill();
    return 0;
  }


  /*
   * add 1-hop route to packet's transmitter; perform some sanity
   * checking if entry already existed
   */

  RTEntry *r = _rtes.findp(ipaddr);

  if (!r)
    click_chatter("GridRouteTable %s: adding new 1-hop route %p{ip_ptr} -- %p{ether_ptr}",
		  name().c_str(), &ipaddr, &ethaddr);
  else if (r->num_hops() == 1 && r->next_hop_eth != ethaddr)
    click_chatter("GridRouteTable %s: ethernet address of %p{ip_ptr} changed from %p{ether_ptr} to %p{ether_ptr}",
		  name().c_str(), &ipaddr, &r->next_hop_eth, &ethaddr);

  /*
   * well, for now we'll just do the ping-pong on route ads.  ideally
   * we would piggyback the ping-pong data for a destination on any
   * unicast packet to that destination, using the latest info from
   * that destination.  we sould still do the ping-ponging in route
   * ads as well, in case we aren't sending data to that destination.
   * this would probably entail adding two elements: one to fill in
   * outgoing packets with the right stats, and another to pick up the
   * stats from incoming packets.  there would probably be a third
   * element which is a table to hold these stats and take care of the
   * averaging.  this could be the same as either the first or second
   * element, and should have a hook so that the routing table (*this*
   * element) can add information gleaned from the route ads.  one
   * drag is that we can't properly do the averaging -- each new
   * reading comes at a different time, not neccessarily evenly
   * spaced.  we should use some time-weighted average, instead of the
   * usual sample-based average.  the node measuring at the other end
   * of the link needs to timestamp when packet come in and it takes
   * the readings.
   */

  /*
   * individual link metric smoothing, or route metric smoothing?  we
   * will only smooth the ping-pong measurements on individual links;
   * we won't smooth metrics at the route level.  that's because we
   * can't even be sure that as the metrics change for a route to some
   * destination, the metric are even for the same route, i.e. same
   * set of links.
   */

  int entry_sz = hlo->nbr_entry_sz;
  char *entry_ptr = (char *) (hlo + 1);

  /* look for ping-pong link stats about us */
#ifndef SMALL_GRID_HEADERS
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
#endif

  if (ntohl(hlo->ttl) > 0) {
    RTEntry new_r(ipaddr, ethaddr, gh, hlo, jiff);
    init_metric(new_r);
    if (r == 0 || should_replace_old_route(*r, new_r)) {
      if (_log)
	_log->log_added_route(GridLogger::WAS_SENDER, make_generic_rte(new_r));
      _rtes.insert(ipaddr, new_r);
      if (new_r.num_hops() > 1 && r && r->num_hops() == 1) {
	/* clear old 1-hop stats */
	_link_tracker->remove_all_stats(r->dest_ip);
      }
    }
    if (r)
      r->dest_eth = ethaddr;
  }

  /*
   * loop through and process other route entries in hello message
   */
  Vector<RTEntry> triggered_rtes;
  Vector<IPAddress> broken_dests;

  entry_ptr = (char *) (hlo + 1);
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

    update_metric(route);

    RTEntry *our_rte = _rtes.findp(curr->ip);

    /*
     * broken route advertisement
     */
    if (curr->num_hops == 0) {

      if ((route.seq_no() & 1) == 0) {
	// broken routes should have odd seq_no
	click_chatter("ignoring invalid broken route entry from %p{ip_ptr} for %p{ip_ptr}: num_hops was 0 but seq_no was even\n",
		      &ipaddr, &route.dest_ip);
	continue;
      }

      /* if we don't have a route to this destination, ignore it */
      if (!our_rte)
	continue;

      /*
       * if our next hop to the destination is this packet's sender,
       * AND if the seq_no is newer than any information we have.
       * remove the broken route.
       */
      if (our_rte->next_hop_ip == ipaddr &&
	  route.seq_no() > our_rte->seq_no()) {
	broken_dests.push_back(route.dest_ip);

	/* generate triggered broken route advertisement */
	triggered_rtes.push_back(route);

	if (_log)
	  _log->log_expired_route(GridLogger::BROKEN_AD, route.dest_ip);
      }
      /*
       * otherwise, triggered advertisement: if we have a good route
       * to the destination with a newer seq_no, advertise our new
       * information.
       */
      else if (route.seq_no() < our_rte->seq_no()) {
	assert(!(our_rte->seq_no() & 1)); // valid routes have even seq_no
	if (our_rte->ttl > 0 && our_rte->metric_valid) {
	  triggered_rtes.push_back(*our_rte);

	  if (_log)
	    _log->log_triggered_route(our_rte->dest_ip);
	}
      }
      continue;
    }

    /* skip routes with too many hops */
    // this would change if using proxies
    if (route.num_hops() + 1 > _max_hops)
      continue;

    /*
     * regular route entry -- should we stick it in the table?
     */
    if (our_rte == 0 || should_replace_old_route(*our_rte, route)) {
      _rtes.insert(route.dest_ip, route);
      if (route.num_hops() > 1 && our_rte && our_rte->num_hops() == 1) {
	/* clear old 1-hop stats */
	_link_tracker->remove_all_stats(our_rte->dest_ip);

#if NEXT_HOP_ETH_FIXUP
	/* fix up route entries for which this dest had been the next hop */
	Vector<RTEntry> changed_next_hop;
	for (RTIter i = _rtes.begin(); i; i++) {
	  if (i.value().next_hop_ip == route.dest_ip) {
	    RTEntry r = i.value();
	    /* some skoochy stuff here, this will make it very hard to keep track of things... */
	    r.next_hop_ip = route.next_hop_ip;
	    r.next_hop_eth = route.next_hop_eth;
	    // XXX how to calculate num_hops???, metric, etc.
	    // XXX also, last_updated_jiffies, etc.
	  }
	}
	for (int i = 0; i < changed_next_hop.size(); i++)
	  _rtes.insert(changed_next_hop[i].dest_ip, changed_next_hop[i]);
#endif

      }

      if (_log)
	_log->log_added_route(GridLogger::WAS_ENTRY, make_generic_rte(route));
    }
  }

  /* delete broken routes */
  for (int i = 0; i < broken_dests.size(); i++) {
    bool removed = _rtes.remove(broken_dests[i]);
    assert(removed);
  }

  log_route_table();  // extended logging

  if (_log)
    _log->log_end_recv_advertisement();

  /* send triggered updates */
  if (triggered_rtes.size() > 0)
    send_routing_update(triggered_rtes, false); // XXX should seq_no get incremented for triggered routes -- probably?

  packet->kill();
  return 0;
}



String
GridRouteTable::print_rtes_v(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;

  String s;
  for (RTIter i = n->_rtes.begin(); i.live(); i++) {
    const RTEntry &f = i.value();
    s += f.dest_ip.unparse()
      + " next=" + f.next_hop_ip.unparse()
      + " hops=" + String((int) f.num_hops())
      + " gw=" + (f.is_gateway ? "y" : "n")
      + " loc=" + f.loc.s()
      + " err=" + (f.loc_good ? "" : "-") + String(f.loc_err) // negate loc if invalid
      + " seq=" + String(f.seq_no())
      + " metric_valid=" + (f.metric_valid ? "yes" : "no")
      + " metric=" + String(f.metric)
      + "\n";
  }

  return s;
}

String
GridRouteTable::print_rtes(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;

  String s;
  for (RTIter i = n->_rtes.begin(); i.live(); i++) {
    const RTEntry &f = i.value();
    s += f.dest_ip.unparse()
      + " next=" + f.next_hop_ip.unparse()
      + " hops=" + String((int) f.num_hops())
      + " gw=" + (f.is_gateway ? "y" : "n")
      //      + " loc=" + f.loc.unparse()
      //      + " err=" + (f.loc_good ? "" : "-") + String(f.loc_err) // negate loc if invalid
      + " seq=" + String(f.seq_no())
      + "\n";
  }

  return s;
}

String
GridRouteTable::print_nbrs_v(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;

  String s;
  for (RTIter i = n->_rtes.begin(); i.live(); i++) {
    /* only print immediate neighbors */
    if (i.value().num_hops() != 1)
      continue;
    s += i.key().unparse();
    s += " eth=" + i.value().next_hop_eth.unparse();
    char buf[300];
    snprintf(buf, 300, " metric_valid=%s metric=%d",
	     i.value().metric_valid ? "yes" : "no", i.value().metric);
    s += buf;
    s += "\n";
  }

  return s;
}

String
GridRouteTable::print_nbrs(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;

  String s;
  for (RTIter i = n->_rtes.begin(); i.live(); i++) {
    /* only print immediate neighbors */
    if (i.value().num_hops() != 1)
      continue;
    s += i.key().unparse();
    s += " eth=" + i.value().next_hop_eth.unparse();
    s += "\n";
  }

  return s;
}


String
GridRouteTable::print_ip(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  return n->_ip.unparse();
}


String
GridRouteTable::print_eth(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  return n->_eth.unparse();
}

String
GridRouteTable::metric_type_to_string(MetricType t)
{
  switch (t) {
  case MetricHopCount: return "hopcount"; break;
  case MetricCumulativeDeliveryRate: return "cumulative_delivery_rate"; break;
  case MetricMinDeliveryRate: return "min_delivery_rate"; break;
  case MetricMinSigStrength: return "min_sig_strength"; break;
  case MetricMinSigQuality: return "min_sig_quality"; break;
  case MetricCumulativeQualPct: return "cumulative_qual_pct"; break;
  case MetricCumulativeSigPct: return "cumulative_sig_pct"; break;
  case MetricEstTxCount:   return "est_tx_count"; break;
  default:
    return "unknown_metric_type";
  }
}


String
GridRouteTable::print_metric_type(Element *e, void *)
{
  GridRouteTable *n = (GridRouteTable *) e;
  return metric_type_to_string(n->_metric_type) + "\n";
}

GridRouteTable::MetricType
GridRouteTable::check_metric_type(const String &s)
{
  String s2 = s.lower();
  if (s2 == "hopcount")
    return MetricHopCount;
  else if (s2 == "cumulative_delivery_rate")
    return MetricCumulativeDeliveryRate;
  else if (s2 == "min_delivery_rate")
    return MetricMinDeliveryRate;
  else if (s2 == "min_sig_strength")
    return MetricMinSigStrength;
  else if (s2 == "min_sig_quality")
    return MetricMinSigQuality;
  else if (s2 == "cumulative_qual_pct")
    return MetricCumulativeQualPct;
  else if (s2 == "cumulative_sig_pct")
    return MetricCumulativeSigPct;
  else if (s2 == "est_tx_count")
    return MetricEstTxCount;
  else
    return MetricUnknown;
}

int
GridRouteTable::write_metric_type(const String &arg, Element *el,
				  void *, ErrorHandler *errh)
{
  GridRouteTable *rt = (GridRouteTable *) el;
  MetricType type = check_metric_type(arg);
  if (type < 0)
    return errh->error("unknown metric type ``%s''", ((String) arg).c_str());

  if (type != rt->_metric_type) {
    rt->_metric_type = type;

    if (type == MetricCumulativeSigPct) {
      rt->_max_metric = _max_sig;
      rt->_min_metric = _min_sig;
    }
    else if (type == MetricCumulativeQualPct) {
      rt->_max_metric = _max_qual;
      rt->_min_metric = _min_qual;
    }
    else
      rt->_max_metric = rt->_min_metric = 0;

    /* make sure we don't try to use the old metric for a route */

    Vector<RTEntry> entries;
    for (RTIter i = rt->_rtes.begin(); i.live(); i++) {
      /* skanky, but there's no reason for this to be quick.  i guess
         the HashMap doesn't let you change its values. */
      RTEntry e = i.value();
      e.metric_valid = false;
      entries.push_back(e);
    }

    for (int i = 0; i < entries.size(); i++)
      rt->_rtes.insert(entries[i].dest_ip, entries[i]);
  }
  return 0;
}

String
GridRouteTable::print_metric_range(Element *e, void *)
{
  GridRouteTable *rt = (GridRouteTable *) e;

  return "max=" + String(rt->_max_metric) + " min=" + String(rt->_min_metric) + "\n";
}

int
GridRouteTable::write_metric_range(const String &arg, Element *el,
				   void *, ErrorHandler *errh)
{
  GridRouteTable *rt = (GridRouteTable *) el;
  int max, min;
  int res = Args(rt, errh).push_back_words(arg)
      .read_mp("MAX", max)
      .read_mp("MIN", min)
      .complete();
  if (res < 0)
    return -1;

  if (max < min) {
    int t = max;
    max = min;
    min = t;
  }

  rt->_max_metric = max;
  rt->_min_metric = min;

  return 0;
}

String
GridRouteTable::print_est_type(Element *e, void *)
{
  GridRouteTable *rt = (GridRouteTable *) e;

  return String(rt->_est_type) + "\n";
}

int
GridRouteTable::write_est_type(const String &arg, Element *el,
			       void *, ErrorHandler *)
{
  GridRouteTable *rt = (GridRouteTable *) el;
  rt->_est_type = atoi(((String) arg).c_str());

  return 0;
}


String
GridRouteTable::print_seq_delay(Element *e, void *)
{
  GridRouteTable *rt = (GridRouteTable *) e;

  return String(rt->_seq_delay) + "\n";
}

int
GridRouteTable::write_seq_delay(const String &arg, Element *el,
				void *, ErrorHandler *)
{
  GridRouteTable *rt = (GridRouteTable *) el;
  rt->_seq_delay = atoi(((String) arg).c_str());

  return 0;
}


String
GridRouteTable::print_frozen(Element *e, void *)
{
  GridRouteTable *rt = (GridRouteTable *) e;

  return (rt->_frozen ? "true\n" : "false\n");
}

int
GridRouteTable::write_frozen(const String &arg, Element *el,
				void *, ErrorHandler *)
{
  GridRouteTable *rt = (GridRouteTable *) el;
  rt->_frozen = atoi(((String) arg).c_str());

  click_chatter("GridRouteTable: setting _frozen to %s", rt->_frozen ? "true" : "false");

  return 0;
}

String
GridRouteTable::print_links(Element *e, void *)
{
  GridRouteTable *rt = (GridRouteTable *) e;

  String s = "Metric type: " + metric_type_to_string(rt->_metric_type) + "\n";

  for (RTIter i = rt->_rtes.begin(); i.live(); i++) {
    const RTEntry &r = i.value();
    if (r.num_hops() > 1)
      continue;

    /* get our measurements of the link *from* this neighbor */
#if 0
#if 0
    LinkStat::stat_t *s1 = rt->_link_stat->_stats.findp(r.next_hop_eth);
#else
    struct {
      int qual;
      int sig;
      struct timeval when;
    } *s1 = 0;
#endif

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
	     r.dest_ip.unparse().c_str(), r.next_hop_eth.s().c_str(), r.metric, r.metric_valid ? "valid" : "invalid",
	     s1 ? s1->sig : -1, s1 ? s1->qual : -1, res1 ? ((int) (100 * tx_rate)) : -1,
	     res2 ? tx_sig : -1, res2 ? tx_qual : -1, res3 ? (int) (100 * bcast_rate) : -1);
    s += buf;
#endif
  }
  return s;
}

void
GridRouteTable::add_handlers()
{
  add_read_handler("nbrs_v", print_nbrs_v, 0);
  add_read_handler("nbrs", print_nbrs, 0);
  add_read_handler("rtes_v", print_rtes_v, 0);
  add_read_handler("rtes", print_rtes, 0);
  add_read_handler("ip", print_ip, 0);
  add_read_handler("eth", print_eth, 0);
  add_read_handler("links", print_links, 0);
  add_read_handler("metric_type", print_metric_type, 0);
  add_write_handler("metric_type", write_metric_type, 0);
  add_read_handler("metric_range", print_metric_range, 0);
  add_write_handler("metric_range", write_metric_range, 0);
  add_read_handler("est_type", print_est_type, 0);
  add_write_handler("est_type", write_est_type, 0);
  add_read_handler("seq_delay", print_seq_delay, 0);
  add_write_handler("seq_delay", write_seq_delay, 0);
  add_read_handler("frozen", print_frozen, 0);
  add_write_handler("frozen", write_frozen, 0);
}


void
GridRouteTable::expire_hook(Timer *, void *thunk)
{
  GridRouteTable *n = (GridRouteTable *) thunk;
  n->expire_routes();
  n->_expire_timer.schedule_after_msec(EXPIRE_TIMER_PERIOD);
}


Vector<GridRouteTable::RTEntry>
GridRouteTable::expire_routes()
{
  /*
   * remove expired routes from the routing table.  return a vector of
   * expired routes which is suitable for inclusion in a broken route
   * advertisement.
   */

  /* overloading this timer function to occasionally dump full route table to log */
  _dump_tick++;
  if (_dump_tick == 50) {
    _dump_tick = 0;
    if (_log) {
      Vector<RouteEntry> vec;
      get_all_entries(vec);
      _log->log_route_dump(vec, Timestamp::now());
    }
  }

  assert(_timeout > 0);
  unsigned int jiff = click_jiffies();

  Vector<RTEntry> retval;

  if (_frozen)
    return retval;


  typedef HashMap<IPAddress, bool> xip_t; // ``expired ip''
  xip_t expired_rtes;
  xip_t expired_next_hops;

  Timestamp ts = Timestamp::now();
  if (_log)
    _log->log_start_expire_handler(ts);

  bool table_changed = false;

  /* 1. loop through RT once, remembering destinations which have been
     in our RT too long (last_updated_jiffies too old) or have
     exceeded their ttl.  Also note those expired 1-hop entries --
     they may be someone's next hop. */
  for (RTIter i = _rtes.begin(); i.live(); i++) {
    if (jiff - i.value().last_updated_jiffies > _timeout_jiffies ||
	decr_ttl(i.value().ttl, jiff_to_msec(jiff - i.value().last_updated_jiffies)) == 0) {
      expired_rtes.insert(i.value().dest_ip, true);

      _extended_logging_errh->message ("expiring %s %d %d", i.value().dest_ip.unparse().c_str(), ts.sec(), ts.usec());  // extended logging
      table_changed = true;

      if (_log)
	_log->log_expired_route(GridLogger::TIMEOUT, i.value().dest_ip);

      if (i.value().num_hops() == 1) {
	/* may be another route's next hop */
	expired_next_hops.insert(i.value().dest_ip, true);
	/* clear link stats */
	_link_tracker->remove_all_stats(i.value().dest_ip);
      }
    }
  }

  /* 2. Loop through RT a second time, picking up any multi-hop
     entries whose next hop is expired, and are not yet expired. */
  for (RTIter i = _rtes.begin(); i.live(); i++) {
    // don't re-expire 1-hop routes, they are their own next hop
    if (i.value().num_hops() > 1 &&
	expired_next_hops.findp(i.value().next_hop_ip) &&
	!expired_rtes.findp(i.value().dest_ip)) {
      expired_rtes.insert(i.value().dest_ip, true);

      _extended_logging_errh->message("next to %s expired %d %d", i.value().dest_ip.unparse().c_str(), ts.sec(), ts.usec());  // extended logging

      if (_log)
	_log->log_expired_route(GridLogger::NEXT_HOP_EXPIRED, i.value().dest_ip);
    }
  }

  /* 3. Then, push all expired entries onto the return vector and
     erase them from the RT.  */
  for (xip_t::iterator i = expired_rtes.begin(); i.live(); i++) {
    RTEntry *r = _rtes.findp(i.key());
    assert(r);
    r->invalidate();
    r->ttl = grid_hello::MAX_TTL_DEFAULT;
    retval.push_back(*r);
  }
  for (xip_t::iterator i = expired_rtes.begin(); i.live(); i++) {
    bool removed = _rtes.remove(i.key());
    assert(removed);
  }

  if (table_changed)
    log_route_table();  // extended logging

  if (_log)
    _log->log_end_expire_handler();

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
  for (RTIter i = n->_rtes.begin(); i.live(); i++) {
    /* because we called expire_routes() at the top of this function,
     * we know we are not propagating any route entries with ttl of 0
     * or that have timed out */

    /* if (i.value().metric_valid) */
    // have to advertise routes even if they have invalid metrics, to
    // kick-start the ping-pong link stats exchange
      rte_entries.push_back(i.value());
  }

  // make and send the packet
  n->send_routing_update(rte_entries);

  uint32_t r2 = click_random();
  double r = (double) (r2 >> 1);
  int jitter = (int) (((double) n->_jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  n->_hello_timer.schedule_after_msec(n->_period + (int) jitter);
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

  if (_frozen)
    return;

  unsigned int jiff = click_jiffies();

  Vector<RTEntry> rte_info = Vector<RTEntry>();

  /*
   * if requested by caller, calculate the ttls each route entry
   * should be sent with.  Each entry's ttl must be decremented by a
   * minimum amount.  Only send the routes with valid ttls (> 0).
   */
  for (int i = 0; i < rtes_to_send.size(); i++) {
    RTEntry &r = rtes_to_send[i];
    if (check_ttls) {
      unsigned int age = jiff_to_msec(jiff - r.last_updated_jiffies);
      unsigned int new_ttl = decr_ttl(r.ttl, (age > grid_hello::MIN_TTL_DECREMENT ? age : (unsigned int) grid_hello::MIN_TTL_DECREMENT));
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
		  name().c_str());

  /* allocate and align the packet */
  WritablePacket *p = Packet::make(psz + 2); // for alignment
  if (p == 0) {
    click_chatter("in %s: cannot make packet!", name().c_str());
    assert(0);
  }
  ASSERT_4ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  /* fill in the timestamp */
  p->set_timestamp_anno(Timestamp::now());

  /* fill in ethernet header */
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _eth.data(), 6);

  /* fill in the grid header */
  grid_hdr *gh = (grid_hdr *) (eh + 1);
  ASSERT_4ALIGNED(gh);
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

  /* extended logging */
  Timestamp now = Timestamp::now();
  _extended_logging_errh->message("sending %u %ld %ld", _seq_no, now.sec(), now.usec());
  if (_log)
    _log->log_sent_advertisement(_seq_no, now);

  /*
   * Update the sequence number for periodic updates, but not for
   * triggered updates.  originating sequence numbers are even,
   * starting at 0.  odd numbers are reserved for other nodes to
   * advertise broken routes
   */
  assert(!(_seq_no & 1));
  if (update_seq) {
    _fake_seq_no++;
    if ((_fake_seq_no % _seq_delay) == 0)
      _seq_no += 2;
  }

  _bcast_count++;
  grid_hdr::set_pad_bytes(*gh, htonl(_bcast_count));

  hlo->ttl = htonl(grid_hello::MAX_TTL_DEFAULT);

  grid_nbr_entry *curr = (grid_nbr_entry *) (hlo + 1);

  char str[80];
  for (int i = 0; i < num_rtes; i++, curr++) {

    const RTEntry &f = rte_info[i];
    snprintf(str, sizeof(str),
	     "%s %s %s %d %c %u %u\n",
	     f.dest_ip.unparse().c_str(),
	     f.loc.s().c_str(),
	     f.next_hop_ip.unparse().c_str(),
	     f.num_hops(),
	     (f.is_gateway ? 'y' : 'n'),
	     f.seq_no(),
	     f.metric);
    _extended_logging_errh->message(str);

    rte_info[i].fill_in(curr, _link_stat);
  }

  _extended_logging_errh->message("\n");

  output(0).push(p);
}


void
GridRouteTable::RTEntry::fill_in(grid_nbr_entry *nb, LinkStat *ls)
{
  check();
  nb->ip = dest_ip;
  nb->next_hop_ip = next_hop_ip;
  nb->num_hops = num_hops();
  nb->loc = loc;
  nb->loc_err = htons(loc_err);
  nb->loc_good = loc_good;
  nb->seq_no = htonl(seq_no());
  nb->metric = htonl(metric);
  nb->metric_valid = metric_valid;
  nb->is_gateway = is_gateway;
  nb->ttl = htonl(ttl);

  /* ping-pong link stats back to sender */
#ifndef SMALL_GRID_HEADERS
  nb->link_qual = 0;
  nb->link_sig = 0;
  nb->measurement_time.tv_sec = nb->measurement_time.tv_usec = 0;
  if (ls && num_hops() == 1) {
#if 0
    LinkStat::stat_t *s = ls->_stats.findp(next_hop_eth);
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
    else
      click_chatter("GridRouteTable: error!  unable to get signal strength or quality info for one-hop neighbor %s\n",
		    IPAddress(dest_ip).unparse().c_str());

    nb->num_rx = 0;
    nb->num_expected = 0;
    nb->last_bcast.tv_sec = nb->last_bcast.tv_usec = 0;
    unsigned int window = 0;
    unsigned int num_rx = 0;
    unsigned int num_expected = 0;
    bool res = ls->get_bcast_stats(next_hop_eth, nb->last_bcast, window, num_rx, num_expected);
    if (res) {
      if (num_rx > 255 || num_expected > 255) {
	click_chatter("GridRouteTable: error! overflow on broadcast loss stats for one-hop neighbor %s",
		      IPAddress(dest_ip).unparse().c_str());
	num_rx = num_expected = 255;
      }
      nb->num_rx = num_rx;
      nb->num_expected = num_expected;
      nb->last_bcast = hton(nb->last_bcast);
    }
  }
#else
  ls = 0; // supress compiler warning
#endif
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridRouteTable)
ELEMENT_PROVIDES(GridGenericRouteTable)
CLICK_ENDDECLS
