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

EWMA::EWMA()
  : _stability_shift(4)
{
}

int
EWMA::set_stability_shift(int shift)
{
  if (shift > METER_SCALE-1)
    return -1;
  else {
    _stability_shift = shift;
    return 0;
  }
}

void
EWMA::update_zero_period(int period)
{
  // XXX use table lookup
  if (period >= 100)
    _avg = 0;
  else {
    int compensation = 1 << (_stability_shift - 1); // round off
    for (; period > 0; period--)
      _avg += (-_avg + compensation) >> _stability_shift;
  }
}
