/*
 * ettmetric.{cc,hh} -- estimated transmission count (`ETT') metric
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.  */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include "ettmetric.hh"
#include "ettstat.hh"
#include <elements/grid/linktable.hh>
CLICK_DECLS 

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

ETTMetric::ETTMetric()
  : LinkMetric(), 
    _ett_stat(0),
    _link_table(0),
    _ip()
{
  MOD_INC_USE_COUNT;
}

ETTMetric::~ETTMetric()
{
  MOD_DEC_USE_COUNT;
}

void *
ETTMetric::cast(const char *n) 
{
  if (strcmp(n, "ETTMetric") == 0)
    return (ETTMetric *) this;
  else if (strcmp(n, "LinkMetric") == 0)
    return (LinkMetric *) this;
  else
    return 0;
}

int
ETTMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"ETT",cpElement, "ETTStat element", &_ett_stat,
			"IP", cpIPAddress, "IP address", &_ip,
			"LT", cpElement, "LinkTable element", &_link_table, 
			0);
  if (res < 0)
    return res;
  if (_ett_stat == 0) 
    errh->error("no ETTStat element specified");
  if (!_ip) 
    errh->error("no IP specififed\n");
  if (_link_table == 0) 
    errh->error("no LTelement specified\n");
  if (_ett_stat->cast("ETTStat") == 0)
    return errh->error("ETTStat argument is wrong element type (should be ETTStat)");
  if (_link_table && _link_table->cast("LinkTable") == 0) {
    return errh->error("LinkTable element is not a LinkTable");
  }
  return 0;
}

void
ETTMetric::take_state(Element *e, ErrorHandler *)
{
  ETTMetric *q = (ETTMetric *)e->cast("ETTMetric");
  if (!q) return;
  _links = q->_links;
}
int 
ETTMetric::get_tx_rate(EtherAddress eth) 
{
  if (_ett_stat) {
    IPAddress ip = _ett_stat->reverse_arp(eth);
    if (ip) {
      _ett_stat->update_links(ip);
      struct timeval now;
      click_gettimeofday(&now);
      IPOrderedPair p = IPOrderedPair(_ip, ip);
      LinkInfo *nfo = _links.findp(p);
      if (nfo && (now.tv_sec - nfo->_last.tv_sec < 30)) {
      return  p.first(_ip) ? nfo->_fwd_rate : nfo->_rev_rate;
      }
    }
  }
  return 1;
}

void get_rate_and_tput(int *tput, int *rate, 
		       int fwd_1,
		       int fwd_2, int fwd_5, 
		       int fwd_11,  int rev_small)
{
  if (!rate || !tput) {
    click_chatter("get_rate_and_tput called with %d, %d\n", rate, tput);
    return;
  }
  *rate = 1;
  *tput = rev_small * fwd_1;

  if (*tput < rev_small * (3*fwd_2)/2) {
    *tput = rev_small * (3*fwd_2)/2;
    *rate = 2;
  }
  if (*tput < rev_small * fwd_5 * 3) {
    *tput = rev_small * fwd_5 * 3;
    *rate = 5;
  }

  if (*tput < rev_small * fwd_11 * 5) {
    *tput = rev_small * fwd_11 * 5;
    *rate = 11;
  }


}
void
ETTMetric::update_link(IPAddress from, IPAddress to, 
		       int fwd_small, int rev_small,
		       int fwd_1, int rev_1,
		       int fwd_2, int rev_2,
		       int fwd_5, int rev_5,
		       int fwd_11, int rev_11
		       ) 
{
  IPOrderedPair p = IPOrderedPair(from, to);

  if (!p.first(from)) {
    return update_link(to, from, 
		       rev_small, fwd_small,
		       rev_1, fwd_1,
		       rev_2, fwd_2,
		       rev_5, fwd_5, 
		       rev_11, fwd_11);
  }

  int fwd = 0;
  int fwd_rate = 1;
  get_rate_and_tput(&fwd, &fwd_rate,
		    fwd_1,
		    fwd_2, fwd_5,
		    fwd_11, rev_small);

  if (fwd == 0) {
    fwd = 7777;
  } else {
    fwd = (100*100*100)/fwd;
  }
  int rev = 0;
  int rev_rate = 1;
  get_rate_and_tput(&rev, &rev_rate,
		    rev_1,
		    rev_2, rev_5,
		    rev_11, fwd_small);

  if (rev == 0) {
    rev = 7777;
  } else {
    rev = (100*100*100)/rev;
  }
  
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    _links.insert(p, LinkInfo(p));
    nfo = _links.findp(p);
    nfo->_fwd = 0;
    nfo->_rev = 0;
  }


  struct timeval now;
  click_gettimeofday(&now);
  

  
  nfo->_fwd = fwd;
  nfo->_rev = rev;
  nfo->_last = now;
  nfo->_fwd_rate = fwd_rate;
  nfo->_rev_rate = rev_rate;
  
  if (now.tv_sec - nfo->_last.tv_sec < 30) {
    /* update linktable */
    int fwd = nfo->_fwd;
    int rev = nfo->_rev;
    if (!_link_table->update_link(from, to, fwd)) {
      click_chatter("%{element} couldn't update link %s > %d > %s\n",
		    this,
		    from.s().cc(),
		    fwd,
		    to.s().cc());
    }
    if (!_link_table->update_link(to, from, rev)){
      click_chatter("%{element} couldn't update link %s < %d < %s\n",
		    this,
		    from.s().cc(),
		    rev,
		    to.s().cc());
    }
  }
}
int 
ETTMetric::get_fwd_metric(IPAddress ip)
{
  if (_ett_stat) {
    _ett_stat->update_links(ip);
  }

  struct timeval now;
  click_gettimeofday(&now);
  IPOrderedPair p = IPOrderedPair(_ip, ip);
  LinkInfo *nfo = _links.findp(p);
  if (nfo && (now.tv_sec - nfo->_last.tv_sec < 30)) {
    return  p.first(_ip) ? nfo->_fwd : nfo->_rev;
  }
  return 7777;
}

int 
ETTMetric::get_rev_metric(IPAddress ip)
{
  if (_ett_stat) {
    _ett_stat->update_links(ip);
  }
  struct timeval now;
  click_gettimeofday(&now);
  IPOrderedPair p = IPOrderedPair(_ip, ip);
  LinkInfo *nfo = _links.findp(p);
    if (nfo && (now.tv_sec - nfo->_last.tv_sec < 30)) {
      return  p.first(_ip) ? nfo->_rev : nfo->_fwd;
  }
  return 7777;
}

String
ETTMetric::read_stats(Element *xf, void *)
{
  ETTMetric *e = (ETTMetric *) xf;
  StringAccum sa;
  struct timeval now;
  click_gettimeofday(&now);
  for(LTable::const_iterator i = e->_links.begin(); i; i++) {
    LinkInfo nfo = i.value();
    sa << nfo._p._a << " " << nfo._p._b;
    sa << " fwd " << nfo._fwd;
    sa << " fwd_rate " << nfo._fwd_rate;
    sa << " rev " << nfo._rev;
    sa << " rev_rate " << nfo._rev_rate;
    sa << " last " << now - nfo._last;
    sa << "\n";
  }
  return sa.take_string();
}
void
ETTMetric::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("stats", read_stats, 0);
}


ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(ETTMetric)
#include <click/hashmap.cc>
#include <click/bighashmap.cc>
template class HashMap<IPOrderedPair, ETTMetric::LinkInfo>;
CLICK_ENDDECLS
