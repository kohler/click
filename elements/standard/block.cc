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
#include "router.hh"
#include "confparse.hh"

Block::Block()
  : Element(1, 1)
{
}

Block *
Block::clone() const
{
  return new Block;
}

int
Block::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);

  // Enough args?
  if(args.size() != 1) {
    errh->error("One argument expexted");
    return -1;
  }

  // THRESH
  if(!cp_integer(args[0], _block)) {
    errh->error("Not an integer");
    return -1;
  }

  return 0;
}

int
Block::initialize(ErrorHandler *)
{
  return 0;
}

void
Block::push(int, Packet *packet)
{
  if(packet->siblings_anno() > _block)
    packet->kill();
  else
    output(0).push(packet);
}

/* Packet *                              */
/* Block::pull(int)                      */
/* {                                     */
/*   while (true) {                      */
/*     Packet *packet = input(0).pull(); */
/*     return packet;                    */
/*   }                                   */
/* }                                     */


// HANDLERS

static String
block_read_drops(Element *, void *)
{
  return "\n";
}

void
Block::add_handlers()
{
  add_read_handler("drops", block_read_drops, 0);
}

EXPORT_ELEMENT(Block)
