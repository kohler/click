/*
 * idle.{cc,hh} -- element just sits there and kills packets
 * Robert Morris, Eddie Kohler
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
#include "idle.hh"
#include "bitvector.hh"

Idle::Idle() 
{
  _idle = 0;
  add_output();
}

Idle::~Idle()
{
}

void
Idle::notify_ninputs(int n)
{
  set_ninputs(n);
}

void
Idle::notify_noutputs(int n)
{
  set_noutputs(n);
}

Bitvector
Idle::forward_flow(int) const
{
  return Bitvector(noutputs(), false);
}

Bitvector
Idle::backward_flow(int) const
{
  return Bitvector(ninputs(), false);
}

void
Idle::push(int, Packet *p)
{
  p->kill();
}

Packet *
Idle::pull(int)
{
  return 0;
}

bool
Idle::wants_packet_upstream() const
{
  for (int i = 0; i < ninputs(); i++)
    if (input_is_pull(i))
      return true;
  return false;
}

bool
Idle::run_scheduled()
{
  _idle++;
  for (int i = 0; i < ninputs(); i++)
    if (input_is_pull(i))
      if (Packet *p = input(i).pull()) {
	p->kill();
	_idle = 0;
      }
  if (_idle > 128) return false;
  return true;
}

EXPORT_ELEMENT(Idle)
