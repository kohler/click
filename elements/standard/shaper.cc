/*
 * shaper.{cc,hh} -- element limits number of successful pulls
 * per second to a given rate (bytes/s)
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
#include "shaper.hh"
#include "confparse.hh"
#include "error.hh"

Shaper::Shaper()
{
  add_input();
  add_output();
}

Shaper::~Shaper()
{
}

Shaper *
Shaper::clone() const
{
  return new Shaper;
}

int
Shaper::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned rate;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "max allowable rate", &rate,
		  0) < 0)
    return -1;
  
  unsigned max_value = 0xFFFFFFFF >> _rate.scale;
  if (rate > max_value)
    return errh->error("rate too large (max %u)", max_value);
  
  _meter1 = (rate << _rate.scale) / _rate.freq();
  return 0;
}

int
Shaper::initialize(ErrorHandler *)
{
  _rate.initialize();

  return 0;
}

Packet *
Shaper::pull(int)
{
  _rate.update_time();
  
  int r = _rate.average();
  if (r >= _meter1)
    return 0;
  else {
    Packet *p = input(0).pull();
    if (p) _rate.update_now(p->length());
    return p;
  }
}

static String
read_rate_handler(Element *f, void *)
{
  Shaper *c = (Shaper *)f;
  return cp_unparse_real(c->rate()*c->rate_freq(), c->rate_scale()) + "\n";
}

void
Shaper::add_handlers()
{
  add_read_handler("rate", read_rate_handler, 0);
}

EXPORT_ELEMENT(Shaper)
