/*
 * monitor.{cc,hh} -- counts packets clustered by src/dst addr.
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
#include "monitor.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "click_ip.h"
#include "error.hh"
#include "glue.hh"

Monitor::Monitor()
{
  add_output();
}

Monitor::~Monitor()
{
}

int
Monitor::configure(const String &conf, ErrorHandler *errh)
{
#if IPVERSION == 6
  click_chatter("Monitor doesn't know how to handle IPv6!");
  return -1;
#elif IPVERSION == 4
  Vector<String> args;
  cp_argvec(conf, args);

  // Enough args?
  if(args.size() < 2) {
    errh->error("too few arguments supplied");
    return -1;
  }

  // SRC|DST
  String src_dst;
  String arg = args[0];
  if(!cp_word(arg, src_dst)) {
    errh->error("first argument expected \"SRC\" or \"DST\". Found neither.");
    return -1;
  }

  // MAX
  int max;
  arg = args[1];
  if(!cp_integer(arg, max)) {
    errh->error("second argument expected MAX. Not found.");
    return -1;
  }

  // VAL1, ..., VALn
  int change;
  for (int i = 2; i < args.size(); i++) {
    String arg = args[i];
    if(cp_integer(arg, change, &arg) && cp_eat_space(arg))
      createbase(change);
    else {
      errh->error("expects \"SRC\"|\"DST\", MAX [, VAL1, VAL2, ..., VALn]");
      return -1;
    }
  }

  // Add default if not supplied.
  if(bases.size() == 0)
    createbase();
  return 0;
#endif
}


void
Monitor::createbase(int change = 1)
{
  struct _base *b = new struct _base;
  b->change = change;
  b->stats = new struct _stats;
  clean(b->stats);
  bases.push_back(b);

  add_input();
}



Monitor *
Monitor::clone() const
{
  return new Monitor;
}

void
Monitor::push(int, Packet *p)
{
  IPAddress a = p->dst_ip_anno();
  update(a);
  output(0).push(p);
}


void
Monitor::update(IPAddress a)
{
#if IPVERSION == 6
    return;
#elif IPVERSION == 4

  unsigned int saddr = a.saddr();

  // For each pattern.
  for(int i=0; i<bases.size(); i++) {
    struct _base *b = bases[i];
    // click_chatter("Looking at base with change %d", b->change);

    // Dive in tables until non-split entry is found. Most likely that's
    // immediately.
    struct _stats *s = b->stats;
    struct _counter *c = NULL;
    int bitshift;
    for(bitshift = 24; bitshift >= 0; bitshift -= 8) {
      unsigned char byte = ((saddr >> bitshift) & 0x000000ff);
      // click_chatter("byte is %d", byte);
      c = &(s->counter[byte]);
      if(c->flags & SPLIT)
        s = (struct _stats *) c->value;
      else
        break;
    }

    assert(bitshift >= 0);
    assert(c != NULL);
    assert(!(c->flags & SPLIT));
    c->value += b->change;
  }
#endif
}


void
Monitor::clean(_stats *s)
{
  for(int i=0; i<256; i++) {
    s->counter[i].flags = 0;
    s->counter[i].value = 0;
  }
}

EXPORT_ELEMENT(Monitor)

#include "vector.cc"
template class Vector<Monitor::_base>;
