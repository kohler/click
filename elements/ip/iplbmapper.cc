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
#include "click_ip.h"
#include "click_tcp.h"
#include "click_udp.h"
#include "elemfilter.hh"
#include "router.hh"
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

bool
IPLoadBalancingMapper::configure_first() const
{
  return true;
}

int
IPLoadBalancingMapper::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> patspecs;
  cp_argvec(conf, patspecs);
  if (patspecs.size() == 0)
    return errh->error("no patterns given");
  else if (patspecs.size() == 1)
    errh->warning("only one pattern given");

  int before = errh->nerrors();
  
  for (int i = 0; i < patspecs.size(); i++) {
    IPRewriter::Pattern *p = IPRewriter::Pattern::make(patspecs[i], 0, errh);
    if (p)
      _patterns.push_back(p);
  }
  
  return (errh->nerrors() == before ? 0 : -1);
}

void
IPLoadBalancingMapper::mapper_patterns(Vector<IPRewriter::Pattern *> &v) const
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
    IPRewriter::Pattern *pat = _patterns[_last_pattern];
    _last_pattern++;
    if (pat->create_mapping(flow, forward, reverse)) {
      rw->install(tcp, forward, reverse);
      return forward;
    }
  } while (_last_pattern != first_pattern);
  return 0;
}


ELEMENT_REQUIRES(IPRewriter)
EXPORT_ELEMENT(IPLoadBalancingMapper)
