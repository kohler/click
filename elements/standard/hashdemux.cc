/*
 * hashdemux.{cc,hh} -- element demultiplexes packets based on hash of
 * specified packet fields
 * Eddie Kohler
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
#include "hashdemux.hh"
#include "error.hh"
#include "confparse.hh"

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
