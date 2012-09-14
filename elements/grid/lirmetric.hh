#ifndef LIRMETRIC_HH
#define LIRMETRIC_HH
#include <click/element.hh>
#include "gridgenericmetric.hh"
CLICK_DECLS

/*
 * =c
 * LIRMetric(GridGenericRouteTable)
 * =s Grid
 * =io
 * None
 * =d
 *
 * Child class of GridGenericMetric that implements the `Least
 * Interference Routing' metric.  The metric is the sum of the number
 * 1-hop neighbors of each node in the route.  This node's number of
 * 1-hop neighbors is obtained from the GridGenericRouteTable
 * argument.  Smaller metric values are better.
 *
 * LIR is described in `Spatial Reuse Through Dynamic Power and
 * Routing Control in Common-Channel Random-Access Packet Radio
 * Networks', James Almon Stevens, Ph.D. Thesis, University of Texas
 * at Dallas, 1988.
 *
 * =a HopcountMetric, LinkStat, ETXMetric, E2ELossMetric */

class GridGenericRouteTable;

class LIRMetric : public GridGenericMetric {

public:

  LIRMetric() CLICK_COLD;
  ~LIRMetric() CLICK_COLD;

  const char *class_name() const { return "LIRMetric"; }
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

  GridGenericRouteTable *_rt;

};

CLICK_ENDDECLS
#endif
