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
#include "mapcreator.hh"

int
MappingCreator::initialize(ErrorHandler *errh)
{
  Vector<Element *> rewriters;
  IsaElementFilter filter("Rewriter");
  int ok = router()->upstream_elements(this, 0, &filter, rewriters);
  if (ok < 0)
    return errh->error("upstream_elements failure");
  filter.filter(rewriters);
  if (rewriters.size() != 1)
    return errh->error("each MappingCreator must be downstream of 1 Rewriter");
  _rw = (Rewriter *)rewriters[0];
  _npats = _rw->npatterns();
  _curpat = 0;
  return 0;
}

void
MappingCreator::push(int port, Packet *p)
{
  assert(port == 0);
  _rw->establish_mapping(p, _curpat++);
  if (_curpat == _npats)
    _curpat = 0;
  output(0).push(p);
}

EXPORT_ELEMENT(MappingCreator)
