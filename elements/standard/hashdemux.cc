/*
 * hashdemux.{cc,hh} -- element demultiplexes packets based on hash of
 * specified packet fields
 * Eddie Kohler
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
#include "hashdemux.hh"
#include <click/error.hh>
#include <click/confparse.hh>

HashDemux::HashDemux()
  : _offset(-1)
{
  set_ninputs(1);
}

HashDemux *
HashDemux::clone() const
{
  return new HashDemux;
}

void
HashDemux::notify_noutputs(int i)
{
  set_noutputs(i < 1 ? 1 : i);
}

int
HashDemux::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  errh->error("HashDemux has been renamed; use HashSwitch instead\n(HashDemux will be removed entirely in the next release.)");
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "byte offset", &_offset,
		  cpUnsigned, "number of bytes", &_length,
		  0) < 0)
    return -1;
  if (_length == 0)
    return errh->error("length must be > 0");
  return 0;
}

void
HashDemux::push(int, Packet *p)
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

EXPORT_ELEMENT(HashDemux)
