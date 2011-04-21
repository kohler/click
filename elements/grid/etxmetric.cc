/*
 * etxmetric.{cc,hh} -- estimated transmission count (`ETX') metric
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
#include "elements/grid/etxmetric.hh"
#include "elements/grid/linkstat.hh"
CLICK_DECLS

ETXMetric::ETXMetric()
  : _ls(0)
{
}

ETXMetric::~ETXMetric()
{
}

void *
ETXMetric::cast(const char *n)
{
  if (strcmp(n, "ETXMetric") == 0)
    return (ETXMetric *) this;
  else if (strcmp(n, "GridGenericMetric") == 0)
    return (GridGenericMetric *) this;
  else
    return 0;
}

int
ETXMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("LINKSTAT", ElementCastArg("LinkStat"), _ls)
	.complete();
}


bool
ETXMetric::metric_val_lt(const metric_t &m1, const metric_t &m2) const
{
  return m1.val() < m2.val();
}

GridGenericMetric::metric_t
ETXMetric::get_link_metric(const EtherAddress &e, bool) const
{
  // ETX ix currently symmetric: it is the same in both directions, so
  // we ignore the bool parameter.  This would change if the ETX
  // computation tried to correct for ACK sizes.

  unsigned tau_fwd, tau_rev;
  unsigned r_fwd, r_rev;
  Timestamp t_fwd;

  bool res_fwd = _ls->get_forward_rate(e, &r_fwd, &tau_fwd, &t_fwd);
  bool res_rev = _ls->get_reverse_rate(e, &r_rev, &tau_rev);

  if (!res_fwd || !res_rev)
    return _bad_metric;
  if (r_fwd == 0 || r_rev == 0)
    return _bad_metric;

  if (r_fwd > 100)
    r_fwd = 100;
  if (r_rev > 100)
    r_rev = 100;

  unsigned val = (100 * 100 * 100) / (r_fwd * r_rev);
  assert(val >= 100);

  return metric_t(val);
}

GridGenericMetric::metric_t
ETXMetric::append_metric(const metric_t &r, const metric_t &l) const
{
  if (!r.good() || !l.good())
    return _bad_metric;

  if (r.val() < 100)
    click_chatter("ETXMetric %s: append_metric WARNING: metric %u%% transmissions is too low for route metric",
		  name().c_str(), r.val());
  if (l.val() < 100)
    click_chatter("ETXMetric %s: append_metric WARNING: metric %u%% transmissions is too low for link metric",
		  name().c_str(), r.val());

  return metric_t(r.val() + l.val());
}

unsigned char
ETXMetric::scale_to_char(const metric_t &m) const
{
  if (!m.good() || m.val() > (0xff * 10))
    return 0xff;
  else
    return m.val() / 10;
}

GridGenericMetric::metric_t
ETXMetric::unscale_from_char(unsigned char c) const
{
  return metric_t(c * 10);
}

ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(ETXMetric)

CLICK_ENDDECLS
