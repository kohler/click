/*
 * rrswitch.{cc,hh} -- round robin switch element
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
#include "rrswitch.hh"

RoundRobinSwitch::RoundRobinSwitch()
{
  add_output();
  _next = 0;
}

RoundRobinSwitch::~RoundRobinSwitch()
{
}

void
RoundRobinSwitch::notify_noutputs(int i)
{
  set_noutputs(i < 1 ? 0 : i);
}

void
RoundRobinSwitch::push(int, Packet *p)
{
  int i = _next;
  _next++;
  if (_next >= noutputs())
    _next = 0;
  output(i).push(p);
}

EXPORT_ELEMENT(RoundRobinSwitch)
