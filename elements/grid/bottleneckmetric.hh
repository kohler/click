#ifndef BNECKMETRIC_HH
#define BNECKMETRIC_HH
#include <click/element.hh>
#include "gridgenericmetric.hh"
CLICK_DECLS

/*
 * =c
 * BottleneckMetric(LINKSTAT)
 * =s Grid
 * =io
 * None
 * =d
 *
 * Child class of GridGenericMetric that implements the bottleneck
 * delivery ratio threshold metric.  The bottleneck delivery ratio of
 * a route is the minimum delivery ratio of all links in the route.
 * Routes with larger bottleneck delivery ratios are preferred to
 * those with smaller.  The metric value is the bottleneck delivery
 * ratio as a percentage 0--100%.
 *
 * LinkStat is this node's LinkStat element, which is needed to obtain
 * the link delivery ratios used to calculate the metric.
 *
 * =a HopcountMetric, LinkStat, ETXMetric, ThresholdMetric */

class LinkStat;

class BottleneckMetric : public GridGenericMetric {

public:

  BottleneckMetric() CLICK_COLD;
  ~BottleneckMetric() CLICK_COLD;

  const char *class_name() const { return "BottleneckMetric"; }
  const char *port_count() const { return PORTS_0_0; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const { return false; }

  void *cast(const char *);

  // generic metric methods
  bool metric_val_lt(const metric_t &, const metric_t &) const;
  metric_t get_link_metric(const EtherAddress &, bool) const;
  metric_t append_metric(const metric_t &, const metric_t &) const;
  metric_t prepend_metric(const metric_t &r, const metric_t &l) const
  { return append_metric(r, l); }

  unsigned char scale_to_char(const metric_t &m) const { return (unsigned char) m.val(); }
  metric_t unscale_from_char(unsigned char c)    const { return metric_t(c);             }

private:
  LinkStat *_ls;
};

CLICK_ENDDECLS
#endif
