#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "strip.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

Strip::Strip(unsigned nbytes)
  : Element(1, 1), _nbytes(nbytes)
{
}

Strip::~Strip()
{
}

Strip *
Strip::clone() const
{
  return new Strip(_nbytes);
}

int
Strip::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, router, errh,
		     cpUnsigned, "number of bytes to strip", &_nbytes,
		     0);
}

inline void
Strip::smaction(Packet *p)
{
  p->pull(_nbytes);
}

void
Strip::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
Strip::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    smaction(p);
  return(p);
}

EXPORT_ELEMENT(Strip)
