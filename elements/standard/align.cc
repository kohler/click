/*
 * align.{cc,hh} -- element aligns passing packets
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
#include "align.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"

Align::Align()
  : Element(1, 1)
{
}

int
Align::configure(const String &conf, ErrorHandler *errh)
{
  int count;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "alignment modulus", &count,
		  cpUnsigned, "alignment offset", &_offset,
		  0) < 0)
    return -1;
  if (count != 2 && count != 4 && count != 8)
    return errh->error("align modulus must be 2, 4, or 8");
  if (_offset >= count)
    return errh->error("align offset must be smaller than modulus");
  _mask = count - 1;
  return 0;
}

Packet *
Align::smaction(Packet *p)
{
  int delta = _offset - (((unsigned long)p->data()) & _mask);
  if (delta == 0)
    return p;
  else if (delta < 0)
    delta += _mask + 1;
  if (!p->shared() && p->tailroom() >= (unsigned)delta) {
    memmove(p->data() + delta, p->data(), p->length());
    p->pull(delta);
    p->put(delta);
    return p;
  } else {
    Packet *q = Packet::make(p->headroom() + delta, p->data(), p->length(), p->tailroom());
    q->copy_annotations(p);
    p->kill();
    return q;
  }
}

void
Align::push(int, Packet *p)
{
  output(0).push(smaction(p));
}

Packet *
Align::pull(int)
{
  Packet *p = input(0).pull();
  return (p ? smaction(p) : 0);
}

EXPORT_ELEMENT(Align)
