/*
 * hopcountmetric.{cc,hh} -- minimum hop-count metric
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
#include "elements/grid/hopcountmetric.hh"
CLICK_DECLS 

const GridGenericMetric::metric_t GridGenericMetric::_bad_metric(777777, false);

HopcountMetric::HopcountMetric()
  : GridGenericMetric(0, 0)
{
  MOD_INC_USE_COUNT;
}

HopcountMetric::~HopcountMetric()
{
  MOD_DEC_USE_COUNT;
}

void *
HopcountMetric::cast(const char *n) 
{
  if (strcmp(n, "HopcountMetric") == 0)
    return (HopcountMetric *) this;
  else if (strcmp(n, "GridGenericMetric") == 0)
    return (GridGenericMetric *) this;
  else
    return 0;
}

int
HopcountMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			0);
  return res;
}


bool
HopcountMetric::metric_val_lt(const metric_t &m1, const metric_t &m2) const
{
  return m1.val() < m2.val();
}

GridGenericMetric::metric_t 
HopcountMetric::get_link_metric(const EtherAddress &) const
{
  // XXX verify that the specified destination is actually a neighbor?
  return metric_t(1);
}

GridGenericMetric::metric_t
HopcountMetric::append_metric(const metric_t &r, const metric_t &l) const
{
  if (!r.good() || !l.good())
    return _bad_metric;
  else
    return metric_t(r.val() + l.val());
}

void
HopcountMetric::add_handlers()
{
  add_default_handlers(true);
}


ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(HopcountMetric)

CLICK_ENDDECLS
