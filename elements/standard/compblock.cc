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
  : Element(1, 2), _dst_weight(0), _src_weight(1)
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
  Vector<String> args;
  cp_argvec(conf, args);

  if(args.size() != 2) {
    return errh->error("Two arguments expexted");
  }

  if(!cp_integer(args[0], _dst_weight)) {
    return errh->error("DST_WEIGHT must be an integer");
  }
  
  if(!cp_integer(args[1], _src_weight)) {
    return errh->error("SRC_WEIGHT must be an integer");
  }

  return 0;
}

int
CompareBlock::initialize(ErrorHandler *)
{
  return 0;
}

void
CompareBlock::push(int, Packet *packet)
{
  if(_dst_weight == 0 || 
     _dst_weight*packet->dst_rate_anno() < _src_weight*packet->src_rate_anno())
    output(0).push(packet);
  else
    output(1).push(packet);
}


// HANDLERS
int
CompareBlock::dst_weight_write_handler(const String &conf, Element *e, 
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
  me->_dst_weight = weight;
  return 0;
}

int
CompareBlock::src_weight_write_handler(const String &conf, Element *e, 
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
  me->_src_weight = weight;
  return 0;
}

String
CompareBlock::dst_weight_read_handler(Element *e, void *)
{
  CompareBlock *me = (CompareBlock *) e;
  return String(me->_dst_weight) + "\n";
}

String
CompareBlock::src_weight_read_handler(Element *e, void *)
{
  CompareBlock *me = (CompareBlock *) e;
  return String(me->_src_weight) + "\n";
}

void
CompareBlock::add_handlers()
{
  add_read_handler("dst_weight", dst_weight_read_handler, 0);
  add_write_handler("dst_weight", dst_weight_write_handler, 0);
  add_read_handler("src_weight", src_weight_read_handler, 0);
  add_write_handler("src_weight", src_weight_write_handler, 0);
}

EXPORT_ELEMENT(CompareBlock)


