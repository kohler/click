/*
 * pullswitch.{cc,hh} -- element routes packets from one input of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "staticpullswitch.hh"
#include <click/confparse.hh>
CLICK_DECLS

StaticPullSwitch::StaticPullSwitch()
{
  MOD_INC_USE_COUNT;
  add_output();
}

StaticPullSwitch::~StaticPullSwitch()
{
  MOD_DEC_USE_COUNT;
}

StaticPullSwitch *
StaticPullSwitch::clone() const
{
  return new StaticPullSwitch;
}

void
StaticPullSwitch::notify_ninputs(int n)
{
  set_ninputs(n);
}

int
StaticPullSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _input = 0;
  if (cp_va_parse(conf, this, errh,
		  cpInteger, "active input", &_input,
		  0) < 0)
    return -1;
  if (_input >= ninputs())
    _input = -1;
  return 0;
}

Packet *
StaticPullSwitch::pull(int)
{
  if (_input < 0)
    return 0;
  else
    return input(_input).pull();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StaticPullSwitch)
ELEMENT_MT_SAFE(StaticPullSwitch)
