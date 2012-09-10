#ifndef YARVISMETRIC_HH
#define YARVISMETRIC_HH
#include <click/element.hh>
#include "gridgenericmetric.hh"
CLICK_DECLS

/*
 * =c
 * YarvisMetric(LINKSTAT)
 * =s Grid
 * =io
 * None
 * =d
 *
 * Child class of GridGenericMetric that implements the end-to-end
 * cumulative link loss ratio metric.  The metric is the sum of the
 * quantized logs of link loss ratios, and is described in `Real-World
 * Experiences with an Interactive Ad Hoc Sensor Network', Yarvis et
 * al., in Proceedings of the International Workshup on Ad Hoc
 * Networking, August 2002.
 *
 * LinkStat is this node's LinkStat element, which is needed to obtain
 * the link delivery ratios used to calculate the metric.
 *
 * =a HopcountMetric, LinkStat, ETXMetric, E2ELossMetric */

class LinkStat;

class YarvisMetric : public GridGenericMetric {

public:

  YarvisMetric() CLICK_COLD;
  ~YarvisMetric() CLICK_COLD;

  const char *class_name() const { return "YarvisMetric"; }
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
