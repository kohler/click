/*
 * lirmetric.{cc,hh} -- end-to-end delivery ratio metric
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
#include "elements/grid/lirmetric.hh"
#include "elements/grid/linkstat.hh"
#include "elements/grid/gridgenericrt.hh"
CLICK_DECLS

LIRMetric::LIRMetric()
  : _rt(0)
{
}

LIRMetric::~LIRMetric()
{
}

void *
LIRMetric::cast(const char *n)
{
  if (strcmp(n, "LIRMetric") == 0)
    return (LIRMetric *) this;
  else if (strcmp(n, "GridGenericMetric") == 0)
    return (GridGenericMetric *) this;
  else
    return 0;
}

int
LIRMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("GRIDROUTES", ElementCastArg("GridGenericRouteTable"), _rt)
	.complete();
}


bool
LIRMetric::metric_val_lt(const metric_t &m1, const metric_t &m2) const
{
  return m1.val() < m2.val();
}

GridGenericMetric::metric_t
LIRMetric::get_link_metric(const EtherAddress &, bool) const
{
  // XXX number of nbrs of senders, or number of neighbors of
  // receivers.  that is, for a an n+1 node route with n hops, do we
  // care about all n+1 nodes, or do we ignore the number of neighbors
  // of either the first or last node?
  return metric_t(_rt->get_number_direct_neigbors());
}

GridGenericMetric::metric_t
LIRMetric::append_metric(const metric_t &r, const metric_t &l) const
{
  if (!r.good() || !l.good())
    return _bad_metric;

  // every node must have at least one 1-hop neighbor, or it wouldn't
  // be part of the network!
  if (r.val() < 1)
    click_chatter("LIRMetric %s: append_metric WARNING: metric %u%% neighbors is too low for route metric",
		  name().c_str(), r.val());
  if (l.val() < 1)
    click_chatter("LIRMetric %s: append_metric WARNING: metric %u%% neighbors is too low for link metric",
		  name().c_str(), r.val());

  return metric_t(r.val() + l.val());
}


ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(LIRMetric)

CLICK_ENDDECLS
