/*
 * shaper.{cc,hh} -- element limits number of successful pulls
 * per second to a given rate (packets/s)
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "shaper.hh"

Shaper::Shaper()
{
}

Shaper *
Shaper::clone() const
{
  return new Shaper;
}

Packet *
Shaper::pull(int)
{
  _rate.update_time();
  
  unsigned r = _rate.average();
  if (r >= _meter1)
    return 0;
  else {
    Packet *p = input(0).pull();
    if (p) {
      _rate.update_now(1);	// packets, not bytes
    }
    return p;
  }
}

ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(Shaper)
