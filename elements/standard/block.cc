/*
 * block.{cc,hh} -- element blocks packets
 * Thomer M. Gil
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "block.hh"
#include <click/error.hh>
#include <click/confparse.hh>

Block::Block()
  : Element(1, 2)
{
  MOD_INC_USE_COUNT;
}

Block::~Block()
{
  MOD_DEC_USE_COUNT;
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
