#ifndef ETX2METRIC_HH
#define ETX2METRIC_HH
#include <click/element.hh>
#include "gridgenericmetric.hh"
CLICK_DECLS

/*
 * =c
 * ETX2Metric(DATA_LINKSTAT, ACK_LINKSTAT)
 * =s Grid
 * =io
 * None
 * =d
 *
 * Child class of GridGenericMetric that implements the estimated
 * transmission count (`ETX') metric using measurements at two packet sizes.
 *
 * LinkStat l1 is the LinkStat element measuring at the data packet
 * size; l2 is measuring at the ACK packet size.
 *
 * =a HopcountMetric, LinkStat, ETXMetric */

class LinkStat;

class ETX2Metric : public GridGenericMetric {

public:

  ETX2Metric();
  ~ETX2Metric();

  const char *class_name() const { return "ETX2Metric"; }
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
  LinkStat *_ls_data;
  LinkStat *_ls_ack;
};

CLICK_ENDDECLS
#endif
