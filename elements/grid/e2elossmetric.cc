/*
 * e2elossmetric.{cc,hh} -- end-to-end delivery ratio metric
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
#include "elements/grid/e2elossmetric.hh"
#include "elements/grid/linkstat.hh"
CLICK_DECLS

E2ELossMetric::E2ELossMetric()
  : _ls(0), _twoway(false)
{
}

E2ELossMetric::~E2ELossMetric()
{
}

void *
E2ELossMetric::cast(const char *n)
{
  if (strcmp(n, "E2ELossMetric") == 0)
    return (E2ELossMetric *) this;
  else if (strcmp(n, "GridGenericMetric") == 0)
    return (GridGenericMetric *) this;
  else
    return 0;
}

int
E2ELossMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = Args(conf, this, errh)
      .read_mp("LINKSTAT", reinterpret_cast<Element *&>(_ls))
      .read("TWOWAY", _twoway)
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
E2ELossMetric::metric_val_lt(const metric_t &m1, const metric_t &m2) const
{
  return m1.val() > m2.val();
}

GridGenericMetric::metric_t
E2ELossMetric::get_link_metric(const EtherAddress &e, bool data_sender) const
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

  if (!res_fwd || (_twoway && !res_rev))
    return _bad_metric;

  if (r_fwd > 100)
    r_fwd = 100;
  if (_twoway && r_rev > 100)
    r_rev = 100;

  unsigned val = r_fwd;
  if (_twoway)
    val = val * r_rev / 100;

  assert(val <= 100);

  return metric_t(val);
}

GridGenericMetric::metric_t
E2ELossMetric::append_metric(const metric_t &r, const metric_t &l) const
{
  if (!r.good() || !l.good())
    return _bad_metric;

  if (r.val() > 100)
    click_chatter("E2ELossMetric %s: append_metric WARNING: metric %u%% transmissions is too large for route metric",
		  name().c_str(), r.val());
  if (l.val() > 100)
    click_chatter("E2ELossMetric %s: append_metric WARNING: metric %u%% transmissions is too large for link metric",
		  name().c_str(), r.val());

  return metric_t(r.val() * l.val() / 100);
}


ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(E2ELossMetric)

CLICK_ENDDECLS
