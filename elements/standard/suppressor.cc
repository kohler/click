/*
 * suppressor.{cc,hh} -- element for Ethernet switch
 * John Jannotti
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
#include "suppressor.hh"
#include "bitvector.hh"

Suppressor::Suppressor()
{
}

Suppressor::~Suppressor()
{
}

Bitvector
Suppressor::forward_flow(int i) const
{
  Bitvector bv(noutputs(), false);
  if (i >= 0 && i < noutputs()) bv[i] = true;
  return bv;
}

Bitvector
Suppressor::backward_flow(int o) const
{
  Bitvector bv(ninputs(), false);
  if (o >= 0 && o < ninputs()) bv[o] = true;
  return bv;
}

Suppressor *
Suppressor::clone() const
{
  return new Suppressor;
}

int
Suppressor::initialize(ErrorHandler *)
{
  allow_all();
  return 0;
}

void
Suppressor::push(int source, Packet *p)
{
  if (suppressed(source)) {
    p->kill();
  } else {			// forward	
    output(source).push(p);
  }
}

Packet *
Suppressor::pull(int source)
{
  if (suppressed(source)) {
    return 0;
  } else {
    return input(source).pull();
  }
}

String
Suppressor::read_status(Element* f, void *) {
  Suppressor* sup = (Suppressor*)f;
  String s;
  for (int i = 0; i < sup->noutputs(); i++) {
    s += String(i) + (sup->suppressed(i) ? "x " : "o ");
  }
  return s + "\n";
}

void
Suppressor::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read("status", read_status, 0);
}

bool
Suppressor::set(int output, bool sup) {
  // Need to change anything?
  if (sup == suppressed(output))
    return false;
  
  if (sup)
    suppress(output);
  else
    allow(output);
  
  return true;
}

EXPORT_ELEMENT(Suppressor)
