/*
 * rripmapper.{cc,hh} -- round robin IPMapper
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
#include "rripmapper.hh"
#include "confparse.hh"
#include "error.hh"

void *
RoundRobinIPMapper::cast(const char *name)
{
  if (name && strcmp("RoundRobinIPMapper", name) == 0)
    return (Element *)this;
  else if (name && strcmp("IPMapper", name) == 0)
    return (IPMapper *)this;
  else
    return 0;
}

int
RoundRobinIPMapper::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() == 0)
    return errh->error("no patterns given");
  else if (conf.size() == 1)
    errh->warning("only one pattern given");

  int before = errh->nerrors();
  
  for (int i = 0; i < conf.size(); i++) {
    IPRw::Pattern *p;
    int f, r;
    if (IPRw::Pattern::parse_with_ports(conf[i], &p, &f, &r, this, errh) >= 0) {
      p->use();
      _patterns.push_back(p);
      _forward_outputs.push_back(f);
      _reverse_outputs.push_back(r);
    }
  }
  
  return (errh->nerrors() == before ? 0 : -1);
}

void
RoundRobinIPMapper::uninitialize()
{
  for (int i = 0; i < _patterns.size(); i++)
    _patterns[i]->unuse();
}

void
RoundRobinIPMapper::notify_rewriter(IPRw *rw, ErrorHandler *errh)
{
  int no = rw->noutputs();
  for (int i = 0; i < _patterns.size(); i++) {
    if (_forward_outputs[i] >= no || _reverse_outputs[i] >= no)
      errh->error("port in `%s' out of range for `%s'", declaration().cc(), rw->declaration().cc());
    rw->notify_pattern(_patterns[i]);
  }
}

IPRw::Mapping *
RoundRobinIPMapper::get_map(IPRw *rw, bool tcp, const IPFlowID &flow)
{
  int first_pattern = _last_pattern;
  do {
    IPRw::Pattern *p = _patterns[_last_pattern];
    int fport = _forward_outputs[_last_pattern];
    int rport = _reverse_outputs[_last_pattern];
    _last_pattern++;
    if (IPRw::Mapping *m = rw->apply_pattern(p, fport, rport, tcp, flow))
      return m;
  } while (_last_pattern != first_pattern);
  return 0;
}


ELEMENT_REQUIRES(IPRw)
EXPORT_ELEMENT(RoundRobinIPMapper)
