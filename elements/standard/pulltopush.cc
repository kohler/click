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


void
PullToPush::run_scheduled()
{
  Packet *p;
  int i=0;

  _idle++;
  while ((p = input(0).pull()) && i<8) {
    _idle = 0;
    output(0).push(p);
    i++;
  } 

  if (_idle <= 32)
    reschedule();
}

EXPORT_ELEMENT(PullToPush)
