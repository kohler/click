/*
 * paint.{cc,hh} -- element sets packets' paint annotation
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
#include <click/config.h>
#include <click/package.hh>
#include "paint.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>

Paint::Paint()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _color = 0;
}

Paint::~Paint()
{
  MOD_DEC_USE_COUNT;
}

Paint *
Paint::clone() const
{
  return new Paint();
}

int
Paint::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "color", &_color,
		     0);
}

Packet *
Paint::simple_action(Packet *p)
{
  SET_PAINT_ANNO(p, _color);
  return p;
}

EXPORT_ELEMENT(Paint)
