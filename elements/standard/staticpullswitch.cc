/*
 * pullswitch.{cc,hh} -- element routes packets from one input of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "staticpullswitch.hh"
#include "confparse.hh"

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
StaticPullSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
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

EXPORT_ELEMENT(StaticPullSwitch)
