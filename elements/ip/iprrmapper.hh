#ifndef IPRRMAPPER_HH
#define IPRRMAPPER_HH
#include "elements/ip/iprw.hh"

/*
 * =c
 * IPRoundRobinMapper(PATTERN1, ..., PATTERNn)
 * =s round-robin mapper for IPRewriter(n)
 * V<modifies>
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

class IPRoundRobinMapper : public Element, public IPMapper {

  Vector<IPRw::Pattern *> _patterns;
  Vector<int> _forward_outputs;
  Vector<int> _reverse_outputs;
  int _last_pattern;
  
 public:

  IPRoundRobinMapper()			{ }

  const char *class_name() const	{ return "IPRoundRobinMapper"; }
  void *cast(const char *);
  
  IPRoundRobinMapper *clone() const	{ return new IPRoundRobinMapper; }
  int configure_phase() const		{ return IPRw::CONFIGURE_PHASE_MAPPER;}
  int configure(const Vector<String> &, ErrorHandler *);
  void uninitialize();
  
  void notify_rewriter(IPRw *, ErrorHandler *);
  IPRw::Mapping *get_map(IPRw *, bool, const IPFlowID &);
  
};

#endif
