/*
 * paint.{cc,hh} -- element sets packets' paint annotation
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
Paint::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpByte, "color", &_color,
		     0);
}

Packet *
Paint::simple_action(Packet *p)
{
  SET_PAINT_ANNO(p, _color);
  return p;
}

String
Paint::color_read_handler(Element *e, void *)
{
  Paint *the_paint = (Paint *)e;
  return String(the_paint->color()) + "\n";
}

void
Paint::add_handlers()
{
  add_read_handler("color", color_read_handler, 0);
  add_write_handler("color", reconfigure_positional_handler, (void *)0);
}

EXPORT_ELEMENT(Paint)
ELEMENT_MT_SAFE(Paint)
