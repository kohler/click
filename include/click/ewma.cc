/*
 * ewma.{cc,hh} -- Exponential Weighted Moving Averages
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/ewma.hh>

template <unsigned stability_shift, unsigned scale>
void
DirectEWMAX<stability_shift, scale>::update_zero_period(unsigned period)
{
  // XXX use table lookup
  if (period >= 100)
    _avg = 0;
  else {
    int compensation = 1 << (stability_shift - 1); // round off
    for (; period > 0; period--)
      _avg += static_cast<int>(-_avg + compensation) >> stability_shift;
  }
}
