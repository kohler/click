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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "checkpaint.hh"
#include "confparse.hh"
#include "error.hh"

CheckPaint *
CheckPaint::clone() const
{
  return new CheckPaint();
}

int
CheckPaint::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  errh->error("CheckPaint has been renamed; use PaintTee instead\n(CheckPaint will be removed entirely in the next release.)");
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "color", &_color,
		  0) < 0)
    return -1;
  return 0;
}

Packet *
CheckPaint::simple_action(Packet *p)
{
  if (p->color_anno() == _color)
    output(1).push(p->clone());
  return(p);
}

EXPORT_ELEMENT(CheckPaint)
