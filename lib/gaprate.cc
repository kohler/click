/*
 * gaprate.{cc,hh} -- measure rates through moving gaps
 * Eddie Kohler, Benjie Chen
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
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
