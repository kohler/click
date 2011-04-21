/*
 * bottlneckmetric.{cc,hh} -- bottleneck delivery ratio metric
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
#include "elements/grid/bottleneckmetric.hh"
#include "elements/grid/linkstat.hh"
CLICK_DECLS

BottleneckMetric::BottleneckMetric()
  : _ls(0)
{
}

BottleneckMetric::~BottleneckMetric()
{
}

void *
BottleneckMetric::cast(const char *n)
{
  if (strcmp(n, "BottleneckMetric") == 0)
    return (BottleneckMetric *) this;
  else if (strcmp(n, "GridGenericMetric") == 0)
    return (GridGenericMetric *) this;
  else
    return 0;
}

int
BottleneckMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = Args(conf, this, errh)
      .read_mp("LINKSTAT", reinterpret_cast<Element *&>(_ls))
      .complete();
  if (res < 0)
    return res;
  if (_ls == 0)
    errh->error("no LinkStat element specified");
  if (_ls->cast("LinkStat") == 0)
    return errh->error("LinkStat argument is wrong element type (should be LinkStat)");
  return 0;
}


bool
BottleneckMetric::metric_val_lt(const metric_t &m1, const metric_t &m2) const
{
  if (m1.good() && m2.good())
    return m1.val() > m2.val(); // larger bottlenecks are better, or `less than' in metric world.
  else if (m2.good())
    return false;
  else
    return true;
}

GridGenericMetric::metric_t
BottleneckMetric::get_link_metric(const EtherAddress &e, bool data_sender) const
{
  unsigned tau;
  Timestamp t;
  bool res;
  unsigned r;

  if (data_sender)
    res = _ls->get_forward_rate(e, &r, &tau, &t);
  else
    res = _ls->get_reverse_rate(e, &r, &tau);

  if (!res || r == 0)
    return _bad_metric;

  return metric_t(r);
}

GridGenericMetric::metric_t
BottleneckMetric::append_metric(const metric_t &r, const metric_t &l) const
{
  if (!r.good() || !l.good())
    return _bad_metric;

  if (r.val() > 100)
    click_chatter("BottleneckMetric %s: append_metric WARNING: metric %u%% delivery ratio is too large for route metric",
		  name().c_str(), r.val());
  if (l.val() > 100)
    click_chatter("BottleneckMetric %s: append_metric WARNING: metric %u%% delivery ratio is too large for link metric",
		  name().c_str(), r.val());

  // return min metric (bottleneck delivery ratio)
  if (r.val() < l.val())
    return r;
  else
    return l;
}


ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(BottleneckMetric)

CLICK_ENDDECLS
