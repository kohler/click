/*
 * mapcreator.{cc,hh} -- create mappings for packet rewriting
 * Max Poletto
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "elemfilter.hh"
#include "router.hh"
#include "error.hh"
#include "confparse.hh"
#include "mapcreator.hh"

MappingCreator::RWInfo::RWInfo(Rewriter *rw, int np, int cp) 
  : _rw(rw), _npat(np), _cpat(cp)
{
}

MappingCreator::RWInfo::~RWInfo()
{
}

int
MappingCreator::initialize(ErrorHandler *errh)
{
  if (ninputs() != noutputs())
    return errh->error("MappingCreator must have as many inputs as outputs");

  for (int i = 0; i < ninputs(); i++) {
    Vector<Element *> rewriters;
    CastElementFilter filter("Rewriter");
    int ok = router()->upstream_elements(this, 0, &filter, rewriters);
    if (ok < 0)
      return errh->error("upstream_elements failure");
    filter.filter(rewriters);
    if (rewriters.size() != 1)
      return errh->error("each MappingCreator must be downstream of "
			 "one Rewriter");
    Rewriter *rw = (Rewriter *)rewriters[0];
    _rwi.push_back(RWInfo(rw, rw->npatterns(), 0));
  }
  for (int i = 0; i < noutputs(); i++) {
    Vector<Element *> rewriters;
    CastElementFilter filter("Rewriter");
    int ok = router()->downstream_elements(this, 0, &filter, rewriters);
    if (ok < 0)
      return errh->error("downstream_elements failure");
    filter.filter(rewriters);
    if (rewriters.size() != 1)
      return errh->error("each MappingCreator must be upstream of "
			 "one Rewriter");
    Rewriter *rw = (Rewriter *)rewriters[0];
    if (rw != _rwi[i]._rw)
      return errh->error("Rewriter on input %d must be the same as Rewriter "
			 "on output %d", i, i);
  }
  return 0;
}

void
MappingCreator::push(int port, Packet *p)
{
  RWInfo &rwi = _rwi[port];
  Rewriter *rw = rwi._rw;
  IPFlowID2 c1(p);
  IPFlowID c2;

  if (ninputs() > 1 && (c2 = _eqmap[c1])) {
    IPFlowID c3 = _rwmap[c2];
    rw->establish_mapping(c1, c3, 1);
    _rwmap.insert(c1, c3);
  } else {
    int cpat = rwi._cpat;
    rw->establish_mapping(p, cpat, cpat);
    rwi._cpat++;
    rwi._cpat %= rwi._npat;
    _rwmap.insert(c1, rw->get_mapping(c1).flow_id());
  }
  output(port).push(p);
}

void
MappingCreator::add_handlers()
{
  add_write_handler("add-equiv", equiv_handler, (void *)0);
  add_write_handler("del-equiv", equiv_handler, (void *)1);
}

int
MappingCreator::equiv_handler(const String &s, Element *e, void *thunk,
			      ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(s, args);

  MappingCreator *mc = (MappingCreator *)e;
  int del = (int)thunk;

  String info = cp_subst(s);
  click_chatter("equiv info is %s", info.cc());
  unsigned int s1a, d1a, s2a, d2a;
  int s1p, d1p, s2p, d2p;
  if (cp_ip_address(info, (unsigned char *)&s1a, &info)
      && cp_eat_space(info)
      && cp_integer(info, 10, s1p, &info)
      && cp_eat_space(info)
      && cp_ip_address(info, (unsigned char *)&d1a, &info)
      && cp_eat_space(info)
      && cp_integer(info, 10, d1p, &info)
      && cp_eat_space(info)
      && cp_ip_address(info, (unsigned char *)&s2a, &info)
      && cp_eat_space(info)
      && cp_integer(info, 10, s2p, &info)
      && cp_eat_space(info)
      && cp_ip_address(info, (unsigned char *)&d2a, &info)
      && cp_eat_space(info)
      && cp_integer(info, 10, d2p, &info))
    {
      if (del) {
      } else {
	IPFlowID c1(s1a, (short)s1p, d1a, (short)d1p);
	IPFlowID c2(s2a, (short)s2p, d2a, (short)d2p);
	mc->_eqmap.insert(c1, c2);
      }
    } else
      return errh->error("expecting SADDR1 SPORT1 DADDR1 DPORT1 "
			 "SADDR2 SPORT2 DADDR2 DPORT2");
  return 0;
}

EXPORT_ELEMENT(MappingCreator)

#include "vector.cc"
template class Vector<MappingCreator::RWInfo>;
