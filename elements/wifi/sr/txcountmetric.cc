/*
 * txcountmetric.{cc,hh} -- estimated transmission count (`TXCount') metric
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
#include "txcountmetric.hh"
#include "ettstat.hh"
#include <elements/wifi/linktable.hh>
CLICK_DECLS 

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

TXCountMetric::TXCountMetric()
  : LinkMetric(), 
    _link_table(0)
{
}

TXCountMetric::~TXCountMetric()
{
}

void *
TXCountMetric::cast(const char *n) 
{
  if (strcmp(n, "TXCountMetric") == 0)
    return (TXCountMetric *) this;
  else if (strcmp(n, "LinkMetric") == 0)
    return (LinkMetric *) this;
  else
    return 0;
}

int
TXCountMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"LT", cpElement, "LinkTable element", &_link_table, 
			cpEnd);
  if (res < 0)
    return res;
  if (_link_table == 0) {
    click_chatter("%{element}: no LTelement specified",
		  this);
  }
  if (_link_table && _link_table->cast("LinkTable") == 0) {
    return errh->error("LinkTable element is not a LinkTable");
  }
  return 0;
}



void
TXCountMetric::update_link(IPAddress from, IPAddress to, 
		       Vector<RateSize>, 
		       Vector<int> fwd, Vector<int> rev, 
		       uint32_t seq)
{
  int metric = 9999;
  if (fwd.size() && rev.size() &&
      fwd[0] && rev[0]) {
    metric = 100 * 100 * 100 / (fwd[0] * rev[0]);
  }

  /* update linktable */
  if (metric && 
      _link_table && 
      !_link_table->update_link(from, to, seq, 0, metric)) {
    click_chatter("%{element} couldn't update link %s > %d > %s\n",
		  this,
		  from.s().c_str(),
		  metric,
		  to.s().c_str());
  }
  if (metric && 
      _link_table && 
      !_link_table->update_link(to, from, seq, 0, metric)){
    click_chatter("%{element} couldn't update link %s < %d < %s\n",
		  this,
		  from.s().c_str(),
		  metric,
		  to.s().c_str());
  }
}


EXPORT_ELEMENT(TXCountMetric)
ELEMENT_REQUIRES(bitrate)
CLICK_ENDDECLS
