/*
 * ewma.{cc,hh} -- Exponential Weighted Moving Averages
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ewma.hh"

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
