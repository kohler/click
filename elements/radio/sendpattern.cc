#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "sendpattern.hh"
#include "confparse.hh"
#include "error.hh"

SendPattern::SendPattern()
{
  add_output();
  _len = 1;
}

SendPattern *
SendPattern::clone() const
{
  return new SendPattern();
}

int
SendPattern::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, router, errh,
		     cpUnsigned, "packet length", &_len,
		     0);
}

Packet *
SendPattern::pull(int)
{
  Packet *p = Packet::make(_len);
  int i;
  for(i = 0; i < _len; i++)
    p->data()[i] = i & 0xff;
  return(p);
}

EXPORT_ELEMENT(SendPattern)
