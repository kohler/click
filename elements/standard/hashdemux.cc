#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "hashdemux.hh"
#include "error.hh"
#include "confparse.hh"

HashDemux::HashDemux()
  : UnlimitedElement(1, 0), _offset(-1)
{
}

HashDemux::HashDemux(const HashDemux &o)
  : UnlimitedElement(1, 0), _offset(o._offset), _length(o._length)
{
}

HashDemux *
HashDemux::clone() const
{
  return new HashDemux(*this);
}

int
HashDemux::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, router, errh,
		  cpUnsigned, "byte offset", &_offset,
		  cpUnsigned, "number of bytes", &_length,
		  0) < 0)
    return -1;
  if (_length == 0)
    return errh->error("length must be > 0");
  return 0;
}

int
HashDemux::initialize(Router *, ErrorHandler *errh)
{
  if (_offset < 0)
    return errh->error("not configured");
  if (noutputs() == 0)
    return errh->error("at least one output required");
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
