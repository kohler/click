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

ETTMetric::ETTMetric()
  : LinkMetric(), 
    _ett_stat(0)
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
			cpElement, "ETTStat element", &_ett_stat,
			cpKeywords,
			"LT", cpElement, "LinkTable element", &_link_table, 
			0);
  if (res < 0)
    return res;
  if (_ett_stat == 0) 
    errh->error("no ETTStat element specified");
  if (_ett_stat->cast("ETTStat") == 0)
    return errh->error("ETTStat argument is wrong element type (should be ETTStat)");
  if (_link_table && _link_table->cast("LinkTable") == 0) {
    return errh->error("LinkTable element is not a LinkTable");
  }
  return 0;
}

void
ETTMetric::update_link(IPAddress from, IPAddress to, int fwd, int rev) 
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
    nfo->_fwd = 0;
    nfo->_rev = 0;
  }


  struct timeval now;
  click_gettimeofday(&now);
  

  
  nfo->_fwd = fwd;
  nfo->_rev = rev;


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
  IPOrderedPair p = IPOrderedPair(_ip, ip);
  LinkInfo *nfo = _links.findp(p);
  if (nfo) {
    return nfo->_fwd;
  }
  return 7777;
}

int 
ETTMetric::get_rev_metric(IPAddress ip)
{
  IPOrderedPair p = IPOrderedPair(_ip, ip);
  LinkInfo *nfo = _links.findp(p);
  if (nfo) {
    return nfo->_rev;
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
    sa << " rev " << nfo._rev;
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
