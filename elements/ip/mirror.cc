/*
 * mirror.{cc,hh} -- rewrites packet a->b to b->a
 * Max Poletto
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
#include "mirror.hh"

void
Mirror::push(int, Packet *p)
{
  p = p->uniqueify();
  Rewriter::Connection c(p);
  c.rev().apply(p);
  output(0).push(p);
}

EXPORT_ELEMENT(Mirror)
