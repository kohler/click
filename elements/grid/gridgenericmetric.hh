#ifndef GRIDGENERICMETRIC_HH
#define GRIDGENERICMETRIC_HH
#include <click/element.hh>
CLICK_DECLS

class EtherAddress;

// Public interface to Grid route metric elements.

class GridGenericMetric : public Element {

public:

  GridGenericMetric() { }

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

  // Return 1-hop link metric for the link between this node and radio
  // neighbor N.  May be an invalid metric, indicating no such
  // neighbor is known, or that there is not enough data to calculate
  // the metric.  DATA_SENDER should true if this node will be
  // transmitting data to N over the link; if this node is receiving
  // data from N, DATA_SENDER should be false.  This parameter is
  // important for when metrics can be different for each direction of
  // a link.
  virtual metric_t get_link_metric(const EtherAddress &n, bool data_sender) const = 0;

  // Return the route metric resulting from appending the link with
  // metric L to the _end_ of route with metric R.  Either L or R may
  // be invalid, which will result in an invalid combined metric.
  virtual metric_t append_metric(const metric_t &r, const metric_t &l) const = 0;

  // Return the route metric resulting from prepending the link with
  // metric L to the _beginning_ of route with metric R.  Otherwise
  // the same as append_metric().
  virtual metric_t prepend_metric(const metric_t &r, const metric_t &l) const = 0;

  // Most route metrics are commutative.  In this case, they can
  // implement prepend_metric by calling append_metric, since they
  // don't care in which order link metrics are combined.

  // XXX I may be excessively zealous here.  Can you think of any
  // non-commutative route metric computation?  What about a
  // non-commutative route metric computation that can be done
  // incrementally, without having all link metrics available at the
  // same time (as this API requires)?


  // Convert the metric M to a scaled value that fits into one byte;
  // some routing protocols only use one byte for the metric value.
  // The metric's valid bit is ignored.
  virtual unsigned char scale_to_char(const metric_t &m) const = 0;

  // Convert the char value C into a valid, unscaled metric value
  virtual metric_t unscale_from_char(unsigned char c) const = 0;

protected:
  static const metric_t _bad_metric; // defined in hopcountmetric.cc
};

CLICK_ENDDECLS
#endif
