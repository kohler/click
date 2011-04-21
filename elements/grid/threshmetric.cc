/*
 * threshmetric.{cc,hh} -- delivery ratio threshold metric
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
#include <click/args.hh>
#include <click/error.hh>
#include "elements/grid/threshmetric.hh"
#include "elements/grid/linkstat.hh"
CLICK_DECLS

ThresholdMetric::ThresholdMetric()
  : _ls(0), _thresh(63), _twoway(false)
{
}

ThresholdMetric::~ThresholdMetric()
{
}

void *
ThresholdMetric::cast(const char *n)
{
  if (strcmp(n, "ThresholdMetric") == 0)
    return (ThresholdMetric *) this;
  else if (strcmp(n, "GridGenericMetric") == 0)
    return (GridGenericMetric *) this;
  else
    return 0;
}

int
ThresholdMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = Args(conf, this, errh)
      .read_mp("LINKSTAT", ElementCastArg("LinkStat"), _ls)
      .read("THRESH", _thresh)
      .read("TWOWAY", _twoway)
      .complete();
  if (res < 0)
    return res;
  if (_thresh > 100)
    return errh->error("THRESH keyword argument is too large, it must be <= 100 percent");
  return 0;
}


bool
ThresholdMetric::metric_val_lt(const metric_t &m1, const metric_t &m2) const
{
  if (m1.good() && m2.good())
    return m1.val() < m2.val();
  else if (m2.good())
    return false;
  else
    return true;
}

GridGenericMetric::metric_t
ThresholdMetric::get_link_metric(const EtherAddress &e, bool data_sender) const
{
  unsigned tau_fwd, tau_rev;
  unsigned r_fwd, r_rev;
  Timestamp t_fwd;

  bool res_fwd = _ls->get_forward_rate(e, &r_fwd, &tau_fwd, &t_fwd);
  bool res_rev = _ls->get_reverse_rate(e, &r_rev, &tau_rev);

  // Translate LinkStat fwd/rev data path fwd/rev
  // reverse.
  if (!data_sender) {
    grid_swap(res_fwd, res_rev);
    grid_swap(r_fwd, r_rev);
  }

  if (!res_fwd || (_twoway && !res_rev))
    return _bad_metric;
  if (r_rev == 0 || r_fwd == 0)
    return _bad_metric;

  if (r_fwd >= _thresh && (!_twoway || r_rev >= _thresh))
      return metric_t(1);
  else
    return _bad_metric;
}

GridGenericMetric::metric_t
ThresholdMetric::append_metric(const metric_t &r, const metric_t &l) const
{
  if (!r.good() || !l.good())
    return _bad_metric;

  if (r.val() < 1)
    click_chatter("ThresholdMetric %s: append_metric WARNING: metric %u hops is too low for route metric",
		  name().c_str(), r.val());
  if (l.val() != 1)
    click_chatter("ThresholdMetric %s: append_metric WARNING: metric %u hops should be 1 for link metric",
		  name().c_str(), r.val());

  return metric_t(r.val() + l.val());
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(ThresholdMetric)
