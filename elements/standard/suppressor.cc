/*
 * suppressor.{cc,hh} -- element for Ethernet switch
 * John Jannotti
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
#include "suppressor.hh"
#include "bitvector.hh"
#include "confparse.hh"
#include "error.hh"

Suppressor::Suppressor()
{
}

Suppressor::~Suppressor()
{
}

void
Suppressor::notify_ninputs(int i)
{
  set_ninputs(i);
  set_noutputs(i);
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

bool
Suppressor::set(int output, bool sup)
{
  // Need to change anything?
  if (sup == suppressed(output))
    return false;
  
  if (sup)
    suppress(output);
  else
    allow(output);
  
  return true;
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

static String
read_active(Element *e, void *thunk)
{
  Suppressor *sup = static_cast<Suppressor *>(e);
  int port = (int) reinterpret_cast<long>(thunk);
  return (sup->suppressed(port) ? "false\n" : "true\n");
}

static int
write_active(const String &in_s, Element *e, void *thunk, ErrorHandler *errh)
{
  Suppressor *sup = static_cast<Suppressor *>(e);
  int port = (int) reinterpret_cast<long>(thunk);
  bool active;
  if (!cp_bool(cp_subst(in_s), &active))
    return errh->error("active value must be boolean");
  else {
    sup->set(port, active);
    return 0;
  }
}

static int
write_reset(const String &, Element *e, void *, ErrorHandler *)
{
  Suppressor *sup = static_cast<Suppressor *>(e);
  sup->allow_all();
  return 0;
}

void
Suppressor::add_handlers()
{
  for (int i = 0; i < ninputs(); i++) {
    String s = "active" + String(i);
    add_read_handler(s, read_active, (void *)i);
    add_write_handler(s, write_active, (void *)i);
  }
  add_write_handler("reset", write_reset, 0);
}

EXPORT_ELEMENT(Suppressor)
