#ifndef IPLBMAPPER_HH
#define IPLBMAPPER_HH
#include "elements/ip/iprewriter.hh"

class IPLoadBalancingMapper : public Element, public IPMapper {

  Vector<IPRewriter::Pattern *> _patterns;
  Vector<int> _forward_outputs;
  Vector<int> _reverse_outputs;
  int _last_pattern;
  
 public:

  IPLoadBalancingMapper()		{ }

  const char *class_name() const	{ return "IPLoadBalancingMapper"; }
  void *cast(const char *);
  
  IPLoadBalancingMapper *clone() const	{ return new IPLoadBalancingMapper; }
  //int configure_phase() const		{ return CONFIGURE_PHASE_IPMAPPER; }
  int configure(const Vector<String> &, ErrorHandler *);
  void uninitialize();
  
  void mapper_patterns(Vector<IPRewriter::Pattern *> &, IPRewriter *) const;
  IPRewriter::Mapping *get_map(bool, const IPFlowID &, IPRewriter *);
  
};

#endif
