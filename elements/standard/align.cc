/*
 * align.{cc,hh} -- element aligns passing packets
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
#include "align.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

Align::Align()
{
}

int
Align::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned modulus;
    if (Args(conf, this, errh)
	.read_mp("MODULUS", modulus)
	.read_mp("OFFSET", _offset)
	.complete() < 0)
	return -1;
    if (modulus != 2 && modulus != 4 && modulus != 8)
	return errh->error("MODULUS must be 2, 4, or 8");
    if (_offset >= (int)modulus)
	return errh->error("OFFSET must be smaller than MODULUS");
    _mask = modulus - 1;
    return 0;
}

Packet *
Align::smaction(Packet *p)
{
  int delta = _offset - (reinterpret_cast<uintptr_t>(p->data()) & _mask);
  if (delta == 0)
    return p;
  if (delta < 0)
    delta += _mask + 1;
  return p->shift_data(delta);
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

CLICK_ENDDECLS
EXPORT_ELEMENT(Align)
ELEMENT_MT_SAFE(Align)
