/*
 * discardnofree.{cc,hh} -- element pulls packets, doesn't throw them away
 * Robert Morris
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
#include "discardnofree.hh"

DiscardNoFree::DiscardNoFree()
  : Element(1, 0)
{
}

void
DiscardNoFree::push(int, Packet *)
{
  // Don't kill().
}

bool
DiscardNoFree::wants_packet_upstream() const
{
  return input_is_pull(0);
}

bool
DiscardNoFree::run_scheduled()
{
  if (Packet *p = input(0).pull())
    /* Don't kill() */
    return true;
  else
    return false;
}

EXPORT_ELEMENT(DiscardNoFree)
