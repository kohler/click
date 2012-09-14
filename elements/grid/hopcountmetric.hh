#ifndef HCMETRIC_HH
#define HCMETRIC_HH
#include <click/element.hh>
#include "gridgenericmetric.hh"
CLICK_DECLS

/*
 * =c
 * HopcountMetric
 * =s Grid
 * =io
 * None
 * =d
 *
 * Child class of GridGenericMetric that implements the minimum hop-count metric.
 * =a ETXMetric
 */
class HopcountMetric : public GridGenericMetric {

public:

  HopcountMetric() CLICK_COLD;
  ~HopcountMetric() CLICK_COLD;

  const char *class_name() const { return "HopcountMetric"; }
  const char *port_count() const { return PORTS_0_0; }
  const char *processing() const { return AGNOSTIC; }

  bool can_live_reconfigure() const { return false; }

  void *cast(const char *);

  // generic metric methods
  bool metric_val_lt(const metric_t &, const metric_t &) const;
  metric_t get_link_metric(const EtherAddress &n, bool) const;
  metric_t append_metric(const metric_t &, const metric_t &) const;
  metric_t prepend_metric(const metric_t &r, const metric_t &l) const
  { return append_metric(r, l); }

  unsigned char scale_to_char(const metric_t &m) const { return (unsigned char) m.val(); }
  metric_t unscale_from_char(unsigned char c)    const { return metric_t(c);             }


};

CLICK_ENDDECLS
#endif
