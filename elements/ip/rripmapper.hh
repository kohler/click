#ifndef RRIPMAPPER_HH
#define RRIPMAPPER_HH
#include "elements/ip/iprw.hh"

/*
 * =c
 * RoundRobinIPMapper(PATTERN1, ..., PATTERNn)
 * =s TCP
 * round-robin mapper for IPRewriter(n)
 * =io
 * None
 * =d
 *
 * Works in tandem with IPRewriter to provide round-robin rewriting. This is
 * useful, for example, in load-balancing applications. Implements the
 * IPMapper interface.
 *
 * Responds to mapping requests from an IPRewriter by trying the PATTERNs in
 * round-robin order and returning the first successfully created mapping.
 *
 * =a IPRewriter, TCPRewriter, IPRewriterPatterns */

class RoundRobinIPMapper : public Element, public IPMapper {

  Vector<IPRw::Pattern *> _patterns;
  Vector<int> _forward_outputs;
  Vector<int> _reverse_outputs;
  int _last_pattern;
  
 public:

  RoundRobinIPMapper();
  ~RoundRobinIPMapper();

  const char *class_name() const	{ return "RoundRobinIPMapper"; }
  void *cast(const char *);
  
  RoundRobinIPMapper *clone() const	{ return new RoundRobinIPMapper; }
  int configure_phase() const		{ return IPRw::CONFIGURE_PHASE_MAPPER;}
  int configure(const Vector<String> &, ErrorHandler *);
  void uninitialize();
  
  void notify_rewriter(IPRw *, ErrorHandler *);
  IPRw::Mapping *get_map(IPRw *, int ip_p, const IPFlowID &);
  
};

#endif
