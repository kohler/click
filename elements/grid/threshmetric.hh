#ifndef THRESHMETRIC_HH
#define THRESHMETRIC_HH
#include <click/element.hh>
#include "gridgenericmetric.hh"
CLICK_DECLS

/*
 * =c
 * ThresholdMetric(LINKSTAT, [, I<KEYWORDS>])
 * =s Grid
 * =io
 * None
 * =d
 *
 * Child class of GridGenericMetric that implements the delivery ratio
 * threshold metric.  Links with a delivery ratio larger than a given
 * threshold (specified by the THRESH keyword argument) are considered
 * good; links with a smaller delivery ratio are considered bad.  The
 * metric of a route with all good links is the route's hopcount; a
 * route with at least one bad link is given the special `bad' invalid
 * metric.  Between two good routes with valid metrics, the route with
 * the smaller hopcount is preferred; between two bad routes with
 * invalid metrics, neither route is preferred and an arbitrary choice
 * is made; between a good and bad route, the good route is always
 * preferred.
 *
 * LinkStat is this node's LinkStat element, which is needed to obtain
 * the link delivery ratios used to calculate the metric.
 *
 * Keywords arguments are:
 *
 * =over 8
 *
 * =item TWOWAY
 *
 * Boolean.  If true, both directions of a link must have a delivery
 * ratio greater than the threshold for the link to be good.  If
 * false, only require the forward direction to have a delivery ratio
 * greater than the threshold.  Defaults to false.
 *
 * =item THRESH
 *
 * Unsigned.  Delivery ratio threshold as a percentage (0--100).
 * Defaults to 63%, which is roughly 5/8, the threshold used in the
 * DARPA PRNet.
 *
 * =back
 *
 * =a HopcountMetric, LinkStat, ETXMetric */

class LinkStat;

class ThresholdMetric : public GridGenericMetric {

public:

  ThresholdMetric() CLICK_COLD;
  ~ThresholdMetric() CLICK_COLD;

  const char *class_name() const { return "ThresholdMetric"; }
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
  unsigned _thresh;
  bool _twoway;
};

CLICK_ENDDECLS
#endif
