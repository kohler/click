/*
 * stridesched.{cc,hh} -- stride scheduler
 * Max Poletto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "stridesched.hh"
#include "confparse.hh"
#include "error.hh"

StrideSched::StrideSched()
{
  add_output();
  _list = new Client;
  _list->make_head();
}

StrideSched::~StrideSched()
{
  delete _list;
}

int
StrideSched::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  set_ninputs(args.size());

  for (int i = 0; i < args.size(); i++) {
    const char *s;
    char *e;
    s = args[i].data();
    long int v = strtol(s, &e, 10);
    if (*s == '\0' || s == e) {
      errh->error("argument %d is not a number", i);
      return -1;
    }
    _list->insert(new Client(i, v));
  }
  return 0;
}

void
StrideSched::uninitialize(void)
{
  Client *c;
  while ((c = _list->remove_min())) {
    delete c;
  }
}

Packet *
StrideSched::pull(int)
{
  int j, n = ninputs();
  Client *tmp = new Client();
  tmp->make_head();

  for (j = 0; j < n; j++) {
    // If an input does not produce a packet, it is added to the tmp
    // list and is not checked again in this pass even if its stride 
    // is such that it would be checked in a normal stride scheduler.
    Client *c = _list->remove_min();
    Packet *p = input(c->id()).pull();
    tmp->insert(c);
    if (p) {
      // If an input does produce a packet, all inputs on the tmp list
      // are incremented with a stride and added back to the main _list
      // before the packet is returned.
      Client *x;
      while ((x = tmp->remove_min())) {
	x->stride();
	_list->insert(x);
      }
      delete tmp;
      return p;
    }
  }
  if (j == n) {
    Client *x;
    while ((x = tmp->remove_min())) {
      x->stride();
      _list->insert(x);
    }
  }
  delete tmp;
  return 0;
}

EXPORT_ELEMENT(StrideSched)
