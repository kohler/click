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
#include <elements/wifi/linktable.hh>
CLICK_DECLS 

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

ETTMetric::ETTMetric()
  : LinkMetric(), 
    _ett_stat(0),
    _link_table(0)
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
			"LT", cpElement, "LinkTable element", &_link_table, 
			cpEnd);
  if (res < 0)
    return res;
  if (_ett_stat == 0) 
    return errh->error("no ETTStat element specified");
  if (_link_table == 0) {
    click_chatter("%{element}: no LTelement specified",
		  this);
  }
  if (_ett_stat->cast("ETTStat") == 0)
    return errh->error("ETTStat argument is wrong element type (should be ETTStat)");
  if (_link_table && _link_table->cast("LinkTable") == 0) {
    return errh->error("LinkTable element is not a LinkTable");
  }
  return 0;
}

Vector<IPAddress>
ETTMetric::get_neighbors() {
  return _ett_stat->get_neighbors();
}
int 
ETTMetric::get_tx_rate(EtherAddress) 
{

  return 2;
}

void
ETTMetric::update_link(IPAddress from, IPAddress to, 
		       unsigned fwd, unsigned rev,
		       int , int,
		       uint32_t seq)
{

  if (!from || !to) {
    click_chatter("%{element}::update_link called with %s %s\n",
		  this,
		  from.s().cc(),
		  to.s().cc());
    return;
  }

  if (!fwd && !rev) {
    return;
  }

  /* update linktable */
  if (fwd && _link_table && !_link_table->update_link(from, to, seq, 0,fwd)) {
    click_chatter("%{element} couldn't update link %s > %d > %s\n",
		  this,
		  from.s().cc(),
		  fwd,
		  to.s().cc());
  }
  if (rev && _link_table && !_link_table->update_link(to, from, seq, 0, rev)){
    click_chatter("%{element} couldn't update link %s < %d < %s\n",
		  this,
		  from.s().cc(),
		  rev,
		  to.s().cc());
  }
}

void
ETTMetric::add_handlers()
{
  add_default_handlers(true);
}


ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(ETTMetric)
CLICK_ENDDECLS
