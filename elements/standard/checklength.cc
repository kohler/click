/*
 * checklength.{cc,hh} -- element checks packet length
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
#include "checklength.hh"
#include "confparse.hh"
#include "error.hh"

void
CheckLength::notify_noutputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  set_noutputs(n < 2 ? 1 : 2);
}

void
CheckLength::processing_vector(Vector<int> &in_v, int in_offset,
			      Vector<int> &out_v, int out_offset) const
{
  in_v[in_offset+0] = out_v[out_offset+0] = AGNOSTIC;
  if (noutputs() == 2)
    out_v[out_offset+1] = PUSH;
}

int
CheckLength::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "max length", &_max,
		     0);
}

void
CheckLength::push(int, Packet *p)
{
  if (p->length() > _max)
    checked_push_output(1, p);
  else
    output(0).push(p);
}

Packet *
CheckLength::pull(int)
{
  Packet *p = input(0).pull();
  if (p && p->length() > _max) {
    checked_push_output(1, p);
    return 0;
  } else
    return p;
}

EXPORT_ELEMENT(CheckLength)
