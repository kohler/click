/*
 * delayshaper.{cc,hh} -- element pulls packets from input, delays returnign
 * the packet to output port.
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include <click/package.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include "delayshaper.hh"
#include <click/standard/scheduleinfo.hh>

DelayShaper::DelayShaper()
  : Element(1,1)
{
  MOD_INC_USE_COUNT;
}

DelayShaper::~DelayShaper()
{
  MOD_DEC_USE_COUNT;
}

int
DelayShaper::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpSecondsAsMicro, "delay", &_delay, 0);
}

int
DelayShaper::initialize(ErrorHandler *)
{
  _p = 0;
  return 0;
}

void
DelayShaper::uninitialize()
{
  if (_p) {
    _p->kill();
    _p = 0;
  }
}

static inline unsigned
elapsed_us(struct timeval tv)
{
  struct timeval t;
  unsigned e = 0;
  click_gettimeofday(&t);
  e = (t.tv_sec - tv.tv_sec)*1000000;
  if (t.tv_usec < tv.tv_usec) {
    t.tv_usec += 1000000;
    e -= 1000000;
  }
  e += t.tv_usec-tv.tv_usec;
  return e;
}

Packet *
DelayShaper::pull(int)
{
  if (!_p) 
    _p = input(0).pull(); 
  if (_p) {
    unsigned t = elapsed_us(_p->timestamp_anno());
    if (t >= _delay) { 
      Packet *p = _p;
      _p = 0;
      return p;
    }
  }
  return 0;
}

String
DelayShaper::read_param(Element *e, void *)
{
  DelayShaper *u = (DelayShaper *)e;
  return cp_unparse_microseconds(u->_delay) + "\n";
}

void
DelayShaper::add_handlers()
{
  add_read_handler("delay", read_param, (void *)0);
}

EXPORT_ELEMENT(DelayShaper)
ELEMENT_MT_SAFE(DelayShaper)
