/*
 * block.{cc,hh} -- element blocks packets
 * Thomer M. Gil
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
#include "block.hh"
#include "error.hh"
#include "confparse.hh"

Block::Block()
  : Element(1, 2)
{
}

Block *
Block::clone() const
{
  return new Block;
}

int
Block::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpInteger, "threshold", &_thresh,
		     0);
}

void
Block::push(int, Packet *packet)
{
  if(_thresh == 0 || packet->fwd_rate_anno() <= _thresh)
    output(0).push(packet);
  else
    output(1).push(packet);
}

// HANDLERS
int
Block::thresh_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  Block* me = (Block *) e;

  if(args.size() != 1) {
    errh->error("expecting 1 integer");
    return -1;
  }
  int thresh;
  if(!cp_integer(args[0], &thresh)) {
    errh->error("not an integer");
    return -1;
  }
  me->_thresh = thresh;
  return 0;
}

String
Block::thresh_read_handler(Element *e, void *)
{
  Block *me = (Block *) e;
  return String(me->_thresh) + "\n";
}

void
Block::add_handlers()
{
  add_read_handler("thresh", thresh_read_handler, 0);
  add_write_handler("thresh", thresh_write_handler, 0);
}

EXPORT_ELEMENT(Block)
