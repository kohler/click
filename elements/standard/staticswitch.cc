/*
 * staticswitch.{cc,hh} -- element routes packets to one output of several
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
#include "staticswitch.hh"
#include "confparse.hh"

StaticSwitch *
StaticSwitch::clone() const
{
  return new StaticSwitch;
}

void
StaticSwitch::notify_noutputs(int n)
{
  set_noutputs(n);
}

int
StaticSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _output = 0;
  if (cp_va_parse(conf, this, errh,
		  cpInteger, "active output", &_output,
		  0) < 0)
    return -1;
  if (_output >= noutputs())
    _output = -1;
  return 0;
}

void
StaticSwitch::push(int, Packet *p)
{
  if (_output < 0)
    p->kill();
  else
    output(_output).push(p);
}

EXPORT_ELEMENT(StaticSwitch)
