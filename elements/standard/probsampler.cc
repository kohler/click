/*
 * probsampler.{cc,hh} -- samples packets with certain probability.
 *
 * Benjie Chen
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
#include "probsampler.hh"
#include "error.hh"
#include "confparse.hh"

int
ProbSampler::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh, 
	          cpNonnegFixed, "sample probability", 16, &_p, 0) < 0) 
    return -1;
  if (_p > 0x10000)
    return errh->error("sample probability must be between 0 and 1");
  return 0;
}

void
ProbSampler::push(int, Packet *p)
{
  unsigned r = (random() >> 2) % 0xFFFF;
  if (r < _p)
    output(1).push(p->clone());
  output(0).push(p);
}


// HANDLERS

static int
p_write_handler(const String &conf, Element *e, 
                             void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  ProbSampler* me = (ProbSampler *) e;

  if(args.size() != 1) {
    return errh->error("expecting one number");
  }
  int p;
  if (!cp_real2(args[0], 16, &p))
    return errh->error("not a fraction");
  if (p > 0x10000 || p < 0)
    return errh->error("sample probability must be between 0 and 1");
  me->set_p(p);
  return 0;
}

static String
p_read_handler(Element *e, void *)
{
  ProbSampler *me = (ProbSampler *) e;
  return String(me->get_p()) + "\n";
}

void
ProbSampler::add_handlers()
{
  add_read_handler("prob", p_read_handler, 0);
  add_write_handler("prob", p_write_handler, 0);
}

EXPORT_ELEMENT(ProbSampler)


