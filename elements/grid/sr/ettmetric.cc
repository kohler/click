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
#include "ettmetric.hh"
#include "srcrstat.hh"
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
  return 0;
}

int 
ETTMetric::get_fwd_metric(const IPAddress &ip) const
{
  int small_fwd = 9999;
  int small_rev = 9999;
  int big_fwd = 9999;
  int big_rev = 9999;

  if (_ss_big) {
    big_fwd = _ss_big->get_fwd(ip);
    big_rev = _ss_big->get_rev(ip);
  }
  if (_ss_small) {
    small_fwd = _ss_small->get_fwd(ip);
    small_rev = _ss_small->get_rev(ip);
  }

  if (big_fwd > 0 && small_rev > 0) {
    return (100 * 100 * 100) / (big_fwd * small_rev);
  }

  return 7777;
}

int 
ETTMetric::get_rev_metric(const IPAddress &ip) const
{
  int small_fwd = 9999;
  int small_rev = 9999;
  int big_fwd = 9999;
  int big_rev = 9999;

  if (_ss_big) {
    big_fwd = _ss_big->get_fwd(ip);
    big_rev = _ss_big->get_rev(ip);
  }
  if (_ss_small) {
    small_fwd = _ss_small->get_fwd(ip);
    small_rev = _ss_small->get_rev(ip);
  }
  if (big_rev > 0 && small_fwd > 0) {
    return (100 * 100 * 100) / (big_rev * small_fwd);
  }
  return 7777;
}

void
ETTMetric::add_handlers()
{
  add_default_handlers(true);
}


ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(ETTMetric)

CLICK_ENDDECLS
