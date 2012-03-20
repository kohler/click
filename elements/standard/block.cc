/*
 * block.{cc,hh} -- element blocks packets
 * Thomer M. Gil
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
#include "block.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

Block::Block()
{
}

int
Block::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("THRESH", _thresh).complete();
}

void
Block::push(int, Packet *packet)
{
  if(_thresh == 0 || FWD_RATE_ANNO(packet) <= _thresh)
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
  if(!IntArg().parse(args[0], thresh)) {
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
  return String(me->_thresh);
}

void
Block::add_handlers()
{
  add_read_handler("thresh", thresh_read_handler, 0);
  add_write_handler("thresh", thresh_write_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Block)
ELEMENT_MT_SAFE(Block)
