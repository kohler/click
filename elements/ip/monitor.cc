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
#include "confparse.hh"
#include "click_ip.h"
#include "error.hh"
#include "glue.hh"

Monitor::Monitor()
  : _base(NULL)
{
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
    errh->error("too few arguments");
    return -1;
  }

  // SRC|DST
  if(args[0] == "SRC")
    _src_dst = SRC;
  else if(args[0] == "DST")
    _src_dst = DST;
  else {
    errh->error("first argument expected \"SRC\" or \"DST\". Found neither.");
    return -1;
  }

  // MAX
  if(!cp_integer(args[1], _max)) {
    errh->error("second argument expected MAX. Not found.");
    return -1;
  }

  _base = new struct _stats;
  if(!_base) {
    errh->error("oops");
    return -1;
  }
  clean(_base);

  _base->counter[2].flags = SPLIT;
  _base->counter[2].next_level = new struct _stats;
  clean(_base->counter[2].next_level);

  _base->counter[2].next_level->counter[0].flags = SPLIT;
  _base->counter[2].next_level->counter[0].next_level = new struct _stats;
  clean(_base->counter[2].next_level->counter[0].next_level);

  // VAL1, ..., VALn
  int change;
  for (int i = 2; i < args.size(); i++) {
    String arg = args[i];
    if(cp_integer(arg, change, &arg) && cp_eat_space(arg)) {
      _inputs.push_back(change);
      add_input();
      add_output();
    } else {
      errh->error("expects \"SRC\"|\"DST\", MAX [, VAL1, VAL2, ..., VALn]");
      return -1;
    }
  }

  // Add default if not supplied.
  if(_inputs.size() == 0) {
    _inputs.push_back(1);
    add_input();
    add_output();
  }

  return 0;
#endif
}


Monitor *
Monitor::clone() const
{
  return new Monitor;
}

void
Monitor::push(int port, Packet *p)
{
  IPAddress a = p->dst_ip_anno();
  update(a, _inputs[port]);
  output(port).push(p);
}


void
Monitor::update(IPAddress a, int val)
{
#if IPVERSION == 6
    return;
#elif IPVERSION == 4

  unsigned int saddr = a.saddr();

  // Dive in tables until non-split entry is found. Most likely that's
  // immediately.
  struct _stats *s = _base;
  struct _counter *c = NULL;
  int bitshift;
  for(bitshift = 24; bitshift >= 0; bitshift -= 8) {
    unsigned char byte = ((saddr >> bitshift) & 0x000000ff);
    // click_chatter("byte is %d", byte);
    c = &(s->counter[byte]);
    if(c->flags & SPLIT)
      s = c->next_level;
    else
      break;
  }

  assert(bitshift >= 0);
  assert(c != NULL);
  assert(!(c->flags & SPLIT));
  c->value += val;
#endif
}


void
Monitor::clean(_stats *s, int value = 0, bool recurse = false)
{
  for(int i = 0; i < 256; i++) {
    if(recurse && (s->counter[i].flags & SPLIT == SPLIT)) {
      clean(s->counter[i].next_level, value, true);
      delete s->counter[i].next_level;
    }
    s->counter[i].flags = 0;
    s->counter[i].value = value;
  }
}


String
Monitor::print(_stats *s, String ip = "")
{
  String ret = "";

  for(int i = 0; i < 256; i++) {
    String this_ip = String(i);
    if(ip)
      this_ip = ip + "." + this_ip;


    if(s->counter[i].flags & SPLIT == SPLIT) {
      ret += this_ip + "\t*\n";
      ret += print(s->counter[i].next_level, "\t" + this_ip);
    }
    else
      ret += this_ip + "\t" + String(s->counter[i].value) + "\n";
  }

  return ret;
}



String
Monitor::look_handler(Element *e, void *)
{
  Monitor *me;
  me = (Monitor*) e;

  return me->print(me->_base);
}

String
Monitor::srcdst_rhandler(Element *e, void *)
{
  Monitor *me = (Monitor *) e;
  return (me->_src_dst == SRC ? String("SRC\n") : String("DST\n"));
}


int
Monitor::srcdst_whandler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);

  Monitor* me = (Monitor *) e;

  if(args.size() != 1) {
    errh->error("expecting \"SRC\" or \"DST\" and nothing more or less");
    return -1;
  } else if(args[0] == "SRC")
    me->_src_dst = SRC;
  else if(args[0] == "DST")
    me->_src_dst = DST;
  else {
    errh->error("expected \"SRC\" or \"DST\". Found neither.");
    return -1;
  }

  return 0;
}


String
Monitor::max_rhandler(Element *e, void *)
{
  Monitor *me = (Monitor *) e;
  return String(me->_max) + "\n";
}


int
Monitor::max_whandler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);

  Monitor* me = (Monitor *) e;

  if(args.size() != 1) {
    errh->error("expecting 1 integer");
    return -1;
  }

  int max;
  if(!cp_integer(args[0], max)) {
    errh->error("not an integer");
    return -1;
  }

  me->_max = max;
  return 0;
}

int
Monitor::reset_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);

  Monitor* me = (Monitor *) e;

  if(args.size() != 1) {
    errh->error("expecting 1 integer");
    return -1;
  }

  int init;
  if(!cp_integer(args[0], init)) {
    errh->error("not an integer");
    return -1;
  }

  me->clean(me->_base, init, true);
  return 0;
}

void
Monitor::add_handlers()
{
  add_read_handler("max", max_rhandler, 0);
  add_write_handler("max", max_whandler, 0);

  add_read_handler("srcdst", srcdst_rhandler, 0);
  add_write_handler("srcdst", srcdst_whandler, 0);

  add_write_handler("reset", reset_handler, 0);
  add_read_handler("look", look_handler, 0);
}

EXPORT_ELEMENT(Monitor)

/* #include "vector.cc"                   */
/* template class Vector<Monitor::_base>; */
