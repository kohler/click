#ifndef ETXMETRIC_HH
#define ETXMETRIC_HH
#include <click/element.hh>
#include "gridgenericmetric.hh"
CLICK_DECLS

/*
 * =c
 * ETXMetric(LINKSTAT)
 * =s Grid
 * =io
 * None
 * =d
 *
 * Child class of GridGenericMetric that implements the estimated
 * transmission count (`ETX') metric.
 *
 * LinkStat is this node's LinkStat element, which is needed to obtain
 * the link delivery ratios used to calculate the metric.
 *
 * =a HopcountMetric, LinkStat */

class LinkStat;

class ETXMetric : public GridGenericMetric {

public:

  ETXMetric() CLICK_COLD;
  ~ETXMetric() CLICK_COLD;

  const char *class_name() const { return "ETXMetric"; }
  const char *port_count() const { return PORTS_0_0; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const { return false; }

  void *cast(const char *);

  // generic metric methods
  bool metric_val_lt(const metric_t &, const metric_t &) const;
  metric_t get_link_metric(const EtherAddress &n, bool) const;
  metric_t append_metric(const metric_t &, const metric_t &) const;
  metric_t prepend_metric(const metric_t &r, const metric_t &l) const
  { return append_metric(r, l); }

  unsigned char scale_to_char(const metric_t &) const;
  metric_t unscale_from_char(unsigned char) const;

private:
  LinkStat *_ls;
};

CLICK_ENDDECLS
#endif
