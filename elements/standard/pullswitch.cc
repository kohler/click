/*
 * pullswitch.{cc,hh} -- element routes packets from one input of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "pullswitch.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"

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
  String s = cp_subst(in_s);
  if (!cp_integer(s, &sw->_input))
    return errh->error("PullSwitch input must be integer");
  if (sw->_input >= sw->ninputs())
    sw->_input = -1;
  sw->router()->set_configuration(sw->number(), String(sw->_input));
  return 0;
}

void
PullSwitch::add_handlers()
{
  add_read_handler("switch", read_param, (void *)0);
  add_write_handler("switch", write_param, (void *)0);
}

EXPORT_ELEMENT(PullSwitch)
