/*
 * randomlossage.{cc,hh} -- element probabilistically drops packets
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

#include <click/config.h>
#include <click/package.hh>
#include "randomlossage.hh"
#include <click/confparse.hh>
#include <click/error.hh>

RandomLossage::RandomLossage()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

RandomLossage::~RandomLossage()
{
  MOD_DEC_USE_COUNT;
}

RandomLossage *
RandomLossage::clone() const
{
  return new RandomLossage;
}

void
RandomLossage::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
RandomLossage::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int p_drop;
  bool on = true;
  if (cp_va_parse(conf, this, errh,
		  cpNonnegReal2, "max_p drop probability", 16, &p_drop,
		  cpOptional,
		  cpBool, "active?", &on,
		  0) < 0)
    return -1;
  if (_p_drop > 0x10000)
    return errh->error("drop probability must be between 0 and 1");

  // OK: set variables
  _p_drop = p_drop;
  _on = on;
  
  return 0;
}

int
RandomLossage::initialize(ErrorHandler *)
{
  _drops = 0;
  return 0;
}

void
RandomLossage::push(int, Packet *p)
{
  if (!_on || ((random()>>5) & 0xFFFF) >= _p_drop)
    output(0).push(p);
  else if (noutputs() == 2) {
    output(1).push(p);
    _drops++;
  } else {
    p->kill();
    _drops++;
  }
}

Packet *
RandomLossage::pull(int)
{
  Packet *p = input(0).pull();
  if (!p)
    return 0;
  else if (!_on || ((random()>>5) & 0xFFFF) >= _p_drop)
    return p;
  else if (noutputs() == 2) {
    output(1).push(p);
    _drops++;
    return 0;
  } else {
    p->kill();
    _drops++;
    return 0;
  }
}

static String
random_lossage_read(Element *f, void *vwhich)
{
  int which = (int)vwhich;
  RandomLossage *lossage = (RandomLossage *)f;
  if (which == 0)
    return cp_unparse_real2(lossage->p_drop(), 16) + "\n";
  else if (which == 1)
    return (lossage->on() ? "true\n" : "false\n");
  else
    return String(lossage->drops()) + "\n";
}

void
RandomLossage::add_handlers()
{
  add_read_handler("p_drop", random_lossage_read, (void *)0);
  add_write_handler("p_drop", reconfigure_positional_handler, (void *)0);
  add_read_handler("active", random_lossage_read, (void *)1);
  add_write_handler("active", reconfigure_positional_handler, (void *)1);
  add_read_handler("drops", random_lossage_read, (void *)2);
}

EXPORT_ELEMENT(RandomLossage)
ELEMENT_MT_SAFE(RandomLossage)
