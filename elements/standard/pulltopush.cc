/*
 * pulltopush.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
 * Eddie Kohler
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
#include "pulltopush.hh"


bool
PullToPush::wants_packet_upstream() const
{
  return true;
}

void
PullToPush::run_scheduled()
{
  /* puts itself on run queue if not scheduled already: can do this because
   * run_scheduled unschedules first before calling run_scheduled */
  if (!scheduled()) schedule_tail();
  while (Packet *p = input(0).pull())
    output(0).push(p);
}

EXPORT_ELEMENT(PullToPush)
