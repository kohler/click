/*
 * bigewma.{cc,hh} -- Exponential Weighted Moving Averages using ulonglong
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include "bigewma.hh"

template <unsigned stability_shift, unsigned scale>
void
DirectBigEWMAX<stability_shift, scale>::update_zero_period(unsigned period)
{
  // XXX use table lookup
  if (period >= 100)
    _avg = 0;
  else {
    int compensation = 1 << (stability_shift - 1); // round off
    for (; period > 0; period--)
      _avg += static_cast<int64_t>(-_avg + compensation) >> stability_shift;
  }
}
