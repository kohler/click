/*
 * checkpaint.{cc,hh} -- element checks paint annotation
 * Robert Morris
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
#include "checkpaint.hh"
#include "confparse.hh"
#include "error.hh"

CheckPaint *
CheckPaint::clone() const
{
  return new CheckPaint();
}

int
CheckPaint::configure(const String &conf, ErrorHandler *errh)
{
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
