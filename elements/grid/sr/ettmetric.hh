#ifndef ETTMETRIC_HH
#define ETTMETRIC_HH
#include <click/element.hh>
#include "linkmetric.hh"
CLICK_DECLS

/*
 * =c
 * ETTMetric(LinkStat, LinkStat)
 * =s Grid
 * =io
 * None
 * =d
 *
 */

class SrcrStat;

class ETTMetric : public LinkMetric {
  
public:

  ETTMetric();
  ~ETTMetric();

  const char *class_name() const { return "ETTMetric"; }
  const char *processing() const { return AGNOSTIC; }
  ETTMetric *clone()  const { return new ETTMetric; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void add_handlers();

  void *cast(const char *);

  int get_fwd_metric(const IPAddress &ip) const;
  int get_rev_metric(const IPAddress &ip) const;

private:
  SrcrStat *_ss_small;
  SrcrStat *_ss_big;
};

CLICK_ENDDECLS
#endif
