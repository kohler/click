/*
 * compblock.{cc,hh} -- element blocks packets based on ratio of rate
 * annotations set by IPRateMonitor.
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
#include "compblock.hh"
#include "error.hh"
#include "confparse.hh"

CompareBlock::CompareBlock()
  : Element(1, 2), _fwd_weight(0), _rev_weight(1)
{
}

CompareBlock *
CompareBlock::clone() const
{
  return new CompareBlock;
}

int
CompareBlock::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpInteger, "forward weight", &_fwd_weight,
		     cpInteger, "reverse weight", &_rev_weight,
		     cpInteger, "threshold", &_thresh,
		     0);
}

void
CompareBlock::push(int, Packet *p)
{
  if (_fwd_weight == 0 || 
      (p->fwd_rate_anno() < _thresh && p->rev_rate_anno() < _thresh) ||
      _fwd_weight * p->fwd_rate_anno() >= _rev_weight * p->rev_rate_anno())
    output(0).push(p);
  else
    output(1).push(p);
}


// HANDLERS
int
CompareBlock::fwd_weight_write_handler(const String &conf, Element *e, 
    				       void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  CompareBlock* me = (CompareBlock *) e;

  if(args.size() != 1) {
    return errh->error("expecting one integer");
  }
  int weight;
  if(!cp_integer(args[0], weight)) {
    return errh->error("not an integer");
  }
  me->_fwd_weight = weight;
  return 0;
}

int
CompareBlock::rev_weight_write_handler(const String &conf, Element *e, 
    				       void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  CompareBlock* me = (CompareBlock *) e;

  if(args.size() != 1) {
    return errh->error("expecting one integer");
  }
  int weight;
  if(!cp_integer(args[0], weight)) {
    return errh->error("not an integer");
  }
  me->_rev_weight = weight;
  return 0;
}

int
CompareBlock::thresh_write_handler(const String &conf, Element *e, 
    				   void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  CompareBlock* me = (CompareBlock *) e;

  if(args.size() != 1) {
    return errh->error("expecting one integer");
  }
  int thresh;
  if(!cp_integer(args[0], thresh)) {
    return errh->error("not an integer");
  }
  me->_thresh = thresh;
  return 0;
}

String
CompareBlock::fwd_weight_read_handler(Element *e, void *)
{
  CompareBlock *me = (CompareBlock *) e;
  return String(me->_fwd_weight) + "\n";
}

String
CompareBlock::rev_weight_read_handler(Element *e, void *)
{
  CompareBlock *me = (CompareBlock *) e;
  return String(me->_rev_weight) + "\n";
}

String
CompareBlock::thresh_read_handler(Element *e, void *)
{
  CompareBlock *me = (CompareBlock *) e;
  return String(me->_thresh) + "\n";
}

void
CompareBlock::add_handlers()
{
  add_read_handler("fwd_weight", fwd_weight_read_handler, 0);
  add_write_handler("fwd_weight", fwd_weight_write_handler, 0);
  add_read_handler("rev_weight", rev_weight_read_handler, 0);
  add_write_handler("rev_weight", rev_weight_write_handler, 0);
}

EXPORT_ELEMENT(CompareBlock)


