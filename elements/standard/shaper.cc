/*
 * shaper.{cc,hh} -- element limits number of successful pulls
 * per second to a given rate (bytes/s)
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
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
#include "elemfilter.hh"
#include "router.hh"

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
Shaper::configure(const String &conf, ErrorHandler *errh)
{
  int rate;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "max allowable rate", &rate,
		  0) < 0)
    return -1;
  
  int max_value = ((0xFFFFFFFF<<_rate.scale()) & ~0x80000000);
  if (rate > max_value)
    return errh->error("rate too large (max %d)", max_value);
  
  _meter1 = (rate<<_rate.scale()) / CLICK_HZ;
  return 0;
}

int
Shaper::initialize(ErrorHandler *)
{
  _rate.initialize();
  
  _pullers.clear();
  _puller1 = 0;
  
  WantsPacketUpstreamElementFilter ppff;
  if (router()->downstream_elements(this, 0, &ppff, _pullers) < 0)
    return -1;
  ppff.filter(_pullers);

  if (_pullers.size() == 1)
    _puller1 = _pullers[0];
  
  return 0;
}

Packet *
Shaper::pull(int)
{
  _rate.update_time();
  
  int r = _rate.average();
  if (r >= _meter1) {
    if (_puller1)
      _puller1->run_scheduled();
    else {
      int n = _pullers.size();
      for (int i = 0; i < n; i++)
	_pullers[i]->run_scheduled();
    }
    return 0;
  } else {
    Packet *p = input(0).pull();
    if (p) _rate.update_now(p->length());
    return p;
  }
}

static String
read_rate_handler(Element *f, void *)
{
  Shaper *c = (Shaper *)f;
  return cp_unparse_real(c->rate()*CLICK_HZ, c->rate_scale()) + "\n";
}

void
Shaper::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("rate", read_rate_handler, 0);
}

EXPORT_ELEMENT(Shaper)
