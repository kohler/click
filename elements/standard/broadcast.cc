/*
 * broadcast.{cc,hh} -- element sends each packet to all outputs
 * John Jannotti, Eddie Kohler
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
#include "broadcast.hh"
#include "glue.hh"

Broadcast *
Broadcast::clone() const
{
  return new Broadcast;
}

void
Broadcast::push(int, Packet *p)
{
  int n = noutputs();
  for (int i = 0; i < n - 1; i++)
    output(i).push(p->clone());
  if (n > 0)
    output(n - 1).push(p);
  else
    p->kill();
}

EXPORT_ELEMENT(Broadcast)

