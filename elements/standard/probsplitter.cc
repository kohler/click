/*
 * probsplitter.{cc,hh} -- split packets onto different ports.
 *
 * Benjie Chen
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
#include "probsplitter.hh"
#include <click/error.hh>
#include <click/confparse.hh>

ProbSplitter::ProbSplitter()
  : Element(1, 2)
{
  MOD_INC_USE_COUNT;
}

ProbSplitter::~ProbSplitter()
{
  MOD_DEC_USE_COUNT;
}

int
ProbSplitter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh, 
	          cpNonnegReal2, "split probability", 16, &_p, 0) < 0) 
    return -1;
  if (_p > 0x10000)
    return errh->error("split probability must be between 0 and 1");
  return 0;
}

void
ProbSplitter::push(int, Packet *p)
{
  unsigned r = (random() >> 2) % 0xFFFF;
  if (r < _p)
    output(1).push(p);
  else 
    output(0).push(p);
}


// HANDLERS

static int
p_write_handler(const String &conf, Element *e, 
                             void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  ProbSplitter* me = (ProbSplitter *) e;

  if(args.size() != 1) {
    return errh->error("expecting one number");
  }
  int p;
  if (!cp_real2(args[0], 16, &p))
    return errh->error("not a fraction");
  if (p > 0x10000 || p < 0)
    return errh->error("split probability must be between 0 and 1");
  me->set_p(p);
  return 0;
}

static String
p_read_handler(Element *e, void *)
{
  ProbSplitter *me = (ProbSplitter *) e;
  return String(me->get_p()) + "\n";
}

void
ProbSplitter::add_handlers()
{
  add_read_handler("prob", p_read_handler, 0);
  add_write_handler("prob", p_write_handler, 0);
}

EXPORT_ELEMENT(ProbSplitter)
ELEMENT_MT_SAFE(ProbSplitter)
