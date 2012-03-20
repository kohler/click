/*
 * hashswitch.{cc,hh} -- element demultiplexes packets based on hash of
 * specified packet fields
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
#include "hashswitch.hh"
#include <click/error.hh>
#include <click/args.hh>
CLICK_DECLS

HashSwitch::HashSwitch()
  : _offset(-1)
{
}

int
HashSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read_mp("OFFSET", _offset)
	.read_mp("LENGTH", _length).complete() < 0)
	return -1;
    if (_length == 0)
	return errh->error("length must be > 0");
    return 0;
}

void
HashSwitch::push(int, Packet *p)
{
  const unsigned char *data = p->data();
  int o = _offset, l = _length;
  if ((int)p->length() < o + l)
    output(0).push(p);
  else {
    int d = 0;
    for (int i = o; i < o + l; i++)
      d += data[i];
    int n = noutputs();
    if (n == 2 || n == 4 || n == 8)
      output((d ^ (d>>4)) & (n-1)).push(p);
    else
      output(d % n).push(p);
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(HashSwitch)
ELEMENT_MT_SAFE(HashSwitch)
