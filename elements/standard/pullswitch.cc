/*
 * pullswitch.{cc,hh} -- element routes packets from one input of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "pullswitch.hh"
#include <click/confparse.hh>
#include <click/error.hh>

PullSwitch::PullSwitch()
{
  MOD_INC_USE_COUNT;
  add_output();
}

PullSwitch::~PullSwitch()
{
  MOD_DEC_USE_COUNT;
}

PullSwitch *
PullSwitch::clone() const
{
  return new PullSwitch;
}

void
PullSwitch::notify_ninputs(int n)
{
  set_ninputs(n);
}

int
PullSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _input = 0;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpInteger, "active input", &_input,
		  0) < 0)
    return -1;
  if (_input >= ninputs())
    _input = -1;
  return 0;
}

Packet *
PullSwitch::pull(int)
{
  if (_input < 0)
    return 0;
  else
    return input(_input).pull();
}

String
PullSwitch::read_param(Element *e, void *)
{
  PullSwitch *sw = (PullSwitch *)e;
  return String(sw->_input) + "\n";
}

int
PullSwitch::write_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  PullSwitch *sw = (PullSwitch *)e;
  String s = cp_uncomment(in_s);
  if (!cp_integer(s, &sw->_input))
    return errh->error("PullSwitch input must be integer");
  if (sw->_input >= sw->ninputs())
    sw->_input = -1;
  sw->set_configuration(String(sw->_input));
  return 0;
}

void
PullSwitch::add_handlers()
{
  add_read_handler("switch", read_param, (void *)0);
  add_write_handler("switch", write_param, (void *)0);
}

EXPORT_ELEMENT(PullSwitch)
ELEMENT_MT_SAFE(PullSwitch)
