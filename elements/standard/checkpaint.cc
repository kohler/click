/*
 * checkpaint.{cc,hh} -- element checks paint annotation
 * Robert Morris
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

#include <click/config.h>
#include "checkpaint.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>

CheckPaint::CheckPaint()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

CheckPaint::~CheckPaint()
{
  MOD_DEC_USE_COUNT;
}

void
CheckPaint::notify_noutputs(int n)
{
  set_noutputs(n <= 1 ? 1 : 2);
}

int
CheckPaint::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpByte, "color", &_color,
		     0);
}

void
CheckPaint::push(int, Packet *p)
{
  if (PAINT_ANNO(p) != _color)
    checked_output_push(1, p);
  else
    output(0).push(p);
}

Packet *
CheckPaint::pull(int)
{
  Packet *p = input(0).pull();
  if (p && PAINT_ANNO(p) != _color) {
    checked_output_push(1, p);
    p = 0;
  }
  return p;
}

EXPORT_ELEMENT(CheckPaint)
