/*
 * painttee.{cc,hh} -- element checks paint annotation
 * Robert Morris
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
