/*
 * gaprate.{cc,hh} -- measure rates through moving gaps
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "gaprate.hh"
#include "error.hh"

void
GapRate::set_rate(unsigned r, ErrorHandler *errh)
{
  if (r > GapRate::MAX_RATE && errh)
    errh->error("rate too large; lowered to %u", GapRate::MAX_RATE);
  set_rate(r);
}
