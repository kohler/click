// -*- c-basic-offset: 2; related-file-name: "ewma.hh" -*-
/*
 * ewma.{cc,hh} -- Exponential Weighted Moving Averages
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
#include <click/ewma.hh>
CLICK_DECLS

template <unsigned stability_shift, unsigned scale>
void
DirectEWMAX<stability_shift, scale>::update_zero_period(unsigned period)
{
  // XXX use table lookup
  if (period >= 100)
    _avg = 0;
  else
    for (; period > 0; period--)
      _avg += static_cast<int>(-_avg + compensation) >> stability_shift;
}

CLICK_ENDDECLS
