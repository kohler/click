/*
 * iplbmapper.{cc,hh} -- load balancing IPMapper
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "iplbmapper.hh"
#include "confparse.hh"
#include "error.hh"

void *
IPLoadBalancingMapper::cast(const char *name)
{
  if (name && strcmp("IPLoadBalancingMapper", name) == 0)
    return (Element *)this;
  else if (name && strcmp("IPMapper", name) == 0)
    return (IPMapper *)this;
  else
    return 0;
}

int
IPLoadBalancingMapper::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() == 0)
    return errh->error("no patterns given");
  else if (conf.size() == 1)
    errh->warning("only one pattern given");

  int before = errh->nerrors();
  
  for (int i = 0; i < conf.size(); i++) {
    IPRewriter::Pattern *p;
    int f, r;
    if (IPRewriter::Pattern::parse_with_ports(conf[i], &p, &f, &r, this, errh) >= 0) {
      p->use();
      _patterns.push_back(p);
      _forward_outputs.push_back(f);
      _reverse_outputs.push_back(r);
    }
  }
  
  return (errh->nerrors() == before ? 0 : -1);
}

void
IPLoadBalancingMapper::uninitialize()
{
  for (int i = 0; i < _patterns.size(); i++)
    _patterns[i]->unuse();
}

void
IPLoadBalancingMapper::mapper_patterns(Vector<IPRewriter::Pattern *> &v,
				       IPRewriter *) const
{
  for (int i = 0; i < _patterns.size(); i++)
    v.push_back(_patterns[i]);
}

IPRewriter::Mapping *
IPLoadBalancingMapper::get_map(bool tcp, const IPFlowID &flow, IPRewriter *rw)
{
  IPRewriter::Mapping *forward, *reverse;
  int first_pattern = _last_pattern;
  do {
    IPRewriter::Pattern *p = _patterns[_last_pattern];
    int fport = _forward_outputs[_last_pattern];
    int rport = _reverse_outputs[_last_pattern];
    _last_pattern++;
    if (p->create_mapping(flow, fport, rport, &forward, &reverse)) {
      rw->install(tcp, forward, reverse);
      return forward;
    }
  } while (_last_pattern != first_pattern);
  return 0;
}


ELEMENT_REQUIRES(IPRewriter)
EXPORT_ELEMENT(IPLoadBalancingMapper)
