#ifndef IPLBMAPPER_HH
#define IPLBMAPPER_HH
#include "elements/ip/iprewriter.hh"

class IPLoadBalancingMapper : public Element, public IPMapper {

  Vector<IPRewriter::Pattern *> _patterns;
  int _last_pattern;
  
 public:

  IPLoadBalancingMapper()		{ }

  const char *class_name() const	{ return "IPLoadBalancingMapper"; }
  void *cast(const char *);
  
  IPLoadBalancingMapper *clone() const	{ return new IPLoadBalancingMapper; }
  bool configure_first() const;
  int configure(const String &, ErrorHandler *);
  
  void mapper_patterns(Vector<IPRewriter::Pattern *> &, IPRewriter *) const;
  IPRewriter::Mapping *get_map(bool, const IPFlowID &, IPRewriter *);
  
};

#endif
