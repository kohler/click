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

void
CheckPaint::push(int, Packet *p)
{
  if (p->color_anno() == _color)
    output(1).push(p->clone());
  output(0).push(p);
}

EXPORT_ELEMENT(CheckPaint)
