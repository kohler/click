#ifndef LINKMETRIC_HH
#define LINKMETRIC_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS


class LinkMetric : public Element {
public:
  LinkMetric() { }
  LinkMetric(int ninputs, int noutputs) : Element(ninputs, noutputs) { }
  
  virtual ~LinkMetric() { }

  virtual int get_fwd_metric(IPAddress) = 0;
  virtual int get_rev_metric(IPAddress) = 0;
};

CLICK_ENDDECLS
#endif
