#ifndef GRIDGENERICMETRIC_HH
#define GRIDGENERICMETRIC_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

// public interface to Grid route metric elements

class GridGenericMetric : public Element {

public:
  
  GridGenericMetric() { }
  GridGenericMetric(int ninputs, int noutputs) : Element(ninputs, noutputs) { }
  
  virtual ~GridGenericMetric() { }

  // either a link *or* route metric
  class metric_t {
    bool _good;
    unsigned _val;
  public:
    metric_t() : _good(false), _val(77777) { }
    metric_t(unsigned v, bool g = true) : _good(g), _val(v) { }

    unsigned val()  const { return _val;  }
    bool     good() const { return _good; } 
  };

  // Return true iff M1's metric value is `less than' M2's metric
  // value.  `Smaller' metrics are better.  It only makes sense to
  // call this function if m1.good() && m2.good();
  virtual bool metric_val_lt(const metric_t &m1, const metric_t &m2) const = 0;

  // Return 1-hop link metric to radio neighbor N.  May be an invalid
  // metric, indicating no such neighbor is known, or that there is
  // not enough data to calculate the metric.
  virtual metric_t get_link_metric(const EtherAddress &n) const = 0;

  // Return the route metric resulting from appending the link with
  // metric L to the end of route with metric R.  Either L or R may be
  // invalid.
  virtual metric_t append_metric(const metric_t &r, const metric_t &l) const = 0;
};

CLICK_ENDDECLS
#endif
