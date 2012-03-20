/*
 * compblock.{cc,hh} -- element blocks packets based on ratio of rate
 * annotations set by IPRateMonitor.
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "compblock.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

CompareBlock::CompareBlock()
  : _fwd_weight(0), _rev_weight(1)
{
}

int
CompareBlock::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _bad = 0;
    return Args(conf, this, errh)
	.read_mp("FWD_WEIGHT", _fwd_weight)
	.read_mp("REV_WEIGHT", _rev_weight)
	.read_mp("THRESH", _thresh).complete();
}

void
CompareBlock::push(int, Packet *p)
{
  // int network = *((unsigned char *)&p->ip_header()->ip_src);
  int fwd = FWD_RATE_ANNO(p);
  if (fwd < 1) fwd = 1;
  int rev = REV_RATE_ANNO(p);
  if (rev < 1) rev = 1;
  if ((fwd > _thresh || rev > _thresh) &&
      _fwd_weight * fwd > _rev_weight * rev) {
    output(1).push(p);
  } else
    output(0).push(p);
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
  if(!IntArg().parse(args[0], weight)) {
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
  if(!IntArg().parse(args[0], weight)) {
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
  if(!IntArg().parse(args[0], thresh)) {
    return errh->error("not an integer");
  }
  me->_thresh = thresh;
  return 0;
}

String
CompareBlock::fwd_weight_read_handler(Element *e, void *)
{
  CompareBlock *me = (CompareBlock *) e;
  return String(me->_fwd_weight);
}

String
CompareBlock::rev_weight_read_handler(Element *e, void *)
{
  CompareBlock *me = (CompareBlock *) e;
  return String(me->_rev_weight);
}

String
CompareBlock::thresh_read_handler(Element *e, void *)
{
  CompareBlock *me = (CompareBlock *) e;
  return String(me->_thresh);
}

void
CompareBlock::add_handlers()
{
  add_read_handler("fwd_weight", fwd_weight_read_handler, 0);
  add_write_handler("fwd_weight", fwd_weight_write_handler, 0);
  add_read_handler("rev_weight", rev_weight_read_handler, 0);
  add_write_handler("rev_weight", rev_weight_write_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CompareBlock)
ELEMENT_MT_SAFE(CompareBlock)
