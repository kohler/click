/*
 * yarvismetric.{cc,hh} -- end-to-end delivery ratio metric
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
#include "elements/grid/yarvismetric.hh"
#include "elements/grid/linkstat.hh"
CLICK_DECLS

YarvisMetric::YarvisMetric()
  : _ls(0)
{
}

YarvisMetric::~YarvisMetric()
{
}

void *
YarvisMetric::cast(const char *n)
{
  if (strcmp(n, "YarvisMetric") == 0)
    return (YarvisMetric *) this;
  else if (strcmp(n, "GridGenericMetric") == 0)
    return (GridGenericMetric *) this;
  else
    return 0;
}

int
YarvisMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("LINKSTAT", ElementCastArg("LinkStat"), _ls)
	.complete();
}


bool
YarvisMetric::metric_val_lt(const metric_t &m1, const metric_t &m2) const
{
  return m1.val() < m2.val();
}

GridGenericMetric::metric_t
YarvisMetric::get_link_metric(const EtherAddress &e, bool data_sender) const
{
  unsigned tau_fwd, tau_rev;
  unsigned r_fwd, r_rev;
  Timestamp t_fwd;

  bool res_fwd = _ls->get_forward_rate(e, &r_fwd, &tau_fwd, &t_fwd);
  bool res_rev = _ls->get_reverse_rate(e, &r_rev, &tau_rev);

  // Translate LinkStat forward and reverse rates to data path's
  // forward and reverse rates.  If data_sender is true, this node is
  // sending over the link, and the data's forward direction is the
  // same as LinkStat's forward direction; if not, this node is
  // receiving data over the link, and the data's forward direction is
  // actually LinkStat's reverse direction.
  if (!data_sender) {
    grid_swap(res_fwd, res_rev);
    grid_swap(r_fwd, r_rev);
  }

  if (!res_fwd)
    return _bad_metric;

  unsigned m;

  if      (r_fwd >= 90) m = 1;  // `Q3'
  else if (r_fwd >= 79) m = 3;  // `Q2'
  else if (r_fwd >= 47) m = 6;  // `Q1'
  else                  m = 15; // `Q0'

  return metric_t(m);
}

GridGenericMetric::metric_t
YarvisMetric::append_metric(const metric_t &r, const metric_t &l) const
{
  if (!r.good() || !l.good())
    return _bad_metric;

  if (l.val() > 15)
    click_chatter("YarvisMetric %s: append_metric WARNING: metric %u%% is too large (> 15!) for link metric",
		  name().c_str(), r.val());

  return metric_t(r.val() + l.val());
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(YarvisMetric)
