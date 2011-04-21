/*
 * etx2metric.{cc,hh} -- estimated transmission count (`ETX') metric using two packet sizes
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
#include "elements/grid/etx2metric.hh"
#include "elements/grid/linkstat.hh"
CLICK_DECLS

ETX2Metric::ETX2Metric()
  : _ls_data(0), _ls_ack(0)
{
}

ETX2Metric::~ETX2Metric()
{
}

void *
ETX2Metric::cast(const char *n)
{
  if (strcmp(n, "ETX2Metric") == 0)
    return (ETX2Metric *) this;
  else if (strcmp(n, "GridGenericMetric") == 0)
    return (GridGenericMetric *) this;
  else
    return 0;
}

int
ETX2Metric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = Args(conf, this, errh)
      .read_mp("DATA_LINKSTAT", ElementCastArg("LinkStat"), _ls_data)
      .read_mp("ACK_LINKSTAT", ElementCastArg("LinkStat"), _ls_ack)
      .complete();
  if (res < 0)
    return res;
  return 0;
}


bool
ETX2Metric::metric_val_lt(const metric_t &m1, const metric_t &m2) const
{
  return m1.val() < m2.val();
}

GridGenericMetric::metric_t
ETX2Metric::get_link_metric(const EtherAddress &e, bool data_sender) const
{
  unsigned tau_foo;
  Timestamp t_foo;
  unsigned r_data, r_ack;


  bool res_data = data_sender ?
    _ls_data->get_forward_rate(e, &r_data, &tau_foo, &t_foo) :
    _ls_data->get_reverse_rate(e, &r_data, &tau_foo);

  bool res_ack  = data_sender ?
    _ls_ack->get_reverse_rate(e, &r_ack, &tau_foo) :
    _ls_ack->get_forward_rate(e, &r_ack, &tau_foo, &t_foo);

  if (!res_data || !res_ack)
    return _bad_metric;
  if (r_data == 0 || r_ack == 0)
    return _bad_metric;

  if (r_data > 100)
    r_data = 100;
  if (r_ack > 100)
    r_ack = 100;

  unsigned val = (100 * 100 * 100) / (r_data * r_ack);
  assert(val >= 100);

  return metric_t(val);
}

GridGenericMetric::metric_t
ETX2Metric::append_metric(const metric_t &r, const metric_t &l) const
{
  if (!r.good() || !l.good())
    return _bad_metric;

  if (r.val() < 100)
    click_chatter("ETX2Metric %s: append_metric WARNING: metric %u%% transmissions is too low for route metric",
		  name().c_str(), r.val());
  if (l.val() < 100)
    click_chatter("ETX2Metric %s: append_metric WARNING: metric %u%% transmissions is too low for link metric",
		  name().c_str(), r.val());

  return metric_t(r.val() + l.val());
}

unsigned char
ETX2Metric::scale_to_char(const metric_t &m) const
{
  if (!m.good() || m.val() > (0xff * 10))
    return 0xff;
  else
    return m.val() / 10;
}

GridGenericMetric::metric_t
ETX2Metric::unscale_from_char(unsigned char c) const
{
  return metric_t(c * 10);
}

ELEMENT_PROVIDES(GridGenericMetric)
EXPORT_ELEMENT(ETX2Metric)

CLICK_ENDDECLS
