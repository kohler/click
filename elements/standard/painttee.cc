/*
 * painttee.{cc,hh} -- element checks paint annotation
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
#include <click/package.hh>
#include "painttee.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>

PaintTee::PaintTee()
  : Element(1, 2)
{
  MOD_INC_USE_COUNT;
}

PaintTee::~PaintTee()
{
  MOD_DEC_USE_COUNT;
}

PaintTee *
PaintTee::clone() const
{
  return new PaintTee();
}

int
PaintTee::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "color", &_color,
		  0) < 0)
    return -1;
  return 0;
}

Packet *
PaintTee::simple_action(Packet *p)
{
  if (PAINT_ANNO(p) == _color)
    output(1).push(p->clone());
  return(p);
}

EXPORT_ELEMENT(PaintTee)
ELEMENT_MT_SAFE(PaintTee)
