#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "paint.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

Paint::Paint()
{
  _color = 0;
  add_input();
  add_output();
}

Paint::~Paint()
{
}

Paint *
Paint::clone() const
{
  return new Paint();
}

int
Paint::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, router, errh,
		     cpUnsigned, "color", &_color,
		     0);
}

inline void
Paint::smaction(Packet *p)
{
  p->set_color_anno(_color);
}

void
Paint::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
Paint::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    smaction(p);
  return(p);
}

EXPORT_ELEMENT(Paint)
