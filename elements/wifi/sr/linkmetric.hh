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


};

CLICK_ENDDECLS
#endif
