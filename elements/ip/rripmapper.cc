/*
 * rripmapper.{cc,hh} -- round robin IPMapper
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "rripmapper.hh"
#include <click/confparse.hh>
#include <click/error.hh>

RoundRobinIPMapper::RoundRobinIPMapper()
{
  MOD_INC_USE_COUNT;
}

RoundRobinIPMapper::~RoundRobinIPMapper()
{
  MOD_DEC_USE_COUNT;
}

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
RoundRobinIPMapper::configure(Vector<String> &conf, ErrorHandler *errh)
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
RoundRobinIPMapper::cleanup(CleanupStage)
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
    rw->notify_pattern(_patterns[i], errh);
  }
}

IPRw::Mapping *
RoundRobinIPMapper::get_map(IPRw *rw, int ip_p, const IPFlowID &flow, Packet *)
{
  int first_pattern = _last_pattern;
  do {
    IPRw::Pattern *pat = _patterns[_last_pattern];
    int fport = _forward_outputs[_last_pattern];
    int rport = _reverse_outputs[_last_pattern];
    _last_pattern++;
    if (IPRw::Mapping *m = rw->apply_pattern(pat, ip_p, flow, fport, rport))
      return m;
  } while (_last_pattern != first_pattern);
  return 0;
}


ELEMENT_REQUIRES(IPRw)
EXPORT_ELEMENT(RoundRobinIPMapper)
