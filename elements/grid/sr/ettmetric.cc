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
#include "srcrstat.hh"
#include <elements/grid/linktable.hh>
CLICK_DECLS 

ETTMetric::ETTMetric()
  : LinkMetric(), 
    _ss_small(0), 
    _ss_big(0)
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
			cpElement, "Small SrcrStat element", &_ss_small,
			cpElement, "Big SrcrStat element", &_ss_big,
			cpKeywords,
			"LT", cpElement, "LinkTable element", &_link_table, 
			0);
  if (res < 0)
    return res;
  if (_ss_small == 0) 
    errh->error("no Small SrcrStat element specified");
  if (_ss_big == 0) 
    errh->error("no Big SrcrStat element specified");
  if (_ss_small->cast("SrcrStat") == 0)
    return errh->error("Small SrcrStat argument is wrong element type (should be SrcrStat)");
  if (_ss_big->cast("SrcrStat") == 0)
    return errh->error("Big SrcrStat argument is wrong element type (should be SrcrStat)");
  if (_link_table && _link_table->cast("LinkTable") == 0) {
    return errh->error("LinkTable element is not a LinkTable");
  }
  return 0;
}

void
ETTMetric::update_link(SrcrStat *ss, IPAddress from, IPAddress to, int fwd, int rev) 
{
  IPOrderedPair p = IPOrderedPair(from, to);

  if (!p.first(from)) {
    /* switch */
    IPAddress t = from;
    from = to;
    to = t;

    int t2 = fwd;
    fwd = rev;
    rev = t2;
  }

  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    _links.insert(p, LinkInfo(p));
    nfo = _links.findp(p);
    nfo->update_small(0, 0);
    nfo->update_big(0, 0);
  }


  struct timeval now;
  click_gettimeofday(&now);
  

  
  if (ss == _ss_small) {
    nfo->update_small(fwd, rev);
    nfo->_last_small = now;
  } else if (ss == _ss_big) {
    nfo->update_big(fwd, rev);
    nfo->_last_big = now;
  } else {
    click_chatter("%{element} called with weird SrcrStat\n",
		  this);
  }

  if (now.tv_sec - nfo->_last_small.tv_sec < 30 &&
      now.tv_sec - nfo->_last_big.tv_sec < 30) {
    /* update linktable */
    int fwd = nfo->fwd_metric();
    int rev = nfo->rev_metric();
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
  int small_fwd = 7777;
  int small_rev = 7777;
  int big_fwd = 7777;
  int big_rev = 7777;
  
  if (_ss_big) {
    big_fwd = _ss_big->get_fwd(ip);
    big_rev = _ss_big->get_rev(ip);
  }
  if (_ss_small) {
    small_fwd = _ss_small->get_fwd(ip);
    small_rev = _ss_small->get_rev(ip);
  }
  IPAddress _ip = _ss_big->_ip;
  update_link(_ss_big, ip, _ip, big_fwd, big_rev);
  update_link(_ss_small, ip, _ip, small_fwd, small_rev);

  IPOrderedPair p = IPOrderedPair(_ip, ip);
  LinkInfo *nfo = _links.findp(p);
  if (nfo) {
    return nfo->fwd_metric();
  }
  return 7777;
}

int 
ETTMetric::get_rev_metric(IPAddress ip)
{
  int small_fwd = 7777;
  int small_rev = 7777;
  int big_fwd = 7777;
  int big_rev = 7777;

  if (_ss_big) {
    big_fwd = _ss_big->get_fwd(ip);
    big_rev = _ss_big->get_rev(ip);
  }
  if (_ss_small) {
    small_fwd = _ss_small->get_fwd(ip);
    small_rev = _ss_small->get_rev(ip);
  }
  IPAddress _ip = _ss_big->_ip;
  IPOrderedPair p = IPOrderedPair(_ip, ip);
  LinkInfo *nfo = _links.findp(p);
  if (nfo) {
    return nfo->rev_metric();
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
    sa << " fwd_metric " << nfo.fwd_metric();
    sa << " rev_metric " << nfo.rev_metric();
    sa << " small_fwd " << nfo._small_fwd;
    sa << " small_rev " << nfo._small_rev;
    sa << " last_small " << now - nfo._last_small;
    sa << " big_fwd " << nfo._big_fwd;
    sa << " big_rev " << nfo._big_rev;
    sa << " last_big " << now - nfo._last_big;
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
