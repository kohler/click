/*
 * stridesched.{cc,hh} -- stride scheduler
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "stridesched.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

StrideSched::StrideSched()
{
  MOD_INC_USE_COUNT;
  _list = new Client;
  _list->make_head();
}

StrideSched::~StrideSched()
{
  MOD_DEC_USE_COUNT;
  delete _list;
}

int
StrideSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() < 1) {
    errh->error("%s must be configured with at least one ticket", class_name());
    return -1;
  }
  
  set_ninputs(conf.size());
  set_noutputs(1);

  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    int v;
    if (!cp_integer(conf[i], &v))
      errh->error("argument %d should be number of tickets (integer)", i);
    else if (v < 0)
      errh->error("argument %d (number of tickets) must be >= 0", i);
    else if (v == 0)
      /* do not ever schedule it */;
    else {
      if (v > MAX_TICKETS) {
	errh->warning("input %d's tickets reduced to %d", i, MAX_TICKETS);
	v = MAX_TICKETS;
      }
      _list->insert(new Client(i, v));
    }
  }
  return (errh->nerrors() == before ? 0 : -1);
}

void
StrideSched::cleanup(CleanupStage)
{
  while (_list->_n != _list) {
    Client *c = _list->_n;
    _list->_n->remove();
    delete c;
  }
}

Packet *
StrideSched::pull(int)
{
  // go over list until we find a packet, striding as we go
  Client *stridden = _list->_n;
  Client *c = stridden;
  Packet *p = 0;
  while (c != _list && !p) {
    p = input(c->id()).pull();
    c->stride();
    c = c->_n;
  }

  // remove stridden portion from list
  _list->_n = c;
  c->_p = _list;

  // reinsert stridden portion into list
  while (stridden != c) {
    Client *next = stridden->_n;
    _list->insert(stridden);	// `insert' is OK even when `stridden's next
				// and prev pointers are garbage
    stridden = next;
  }

  return p;
}

int
StrideSched::tickets(int port) const
{
  for (Client *c = _list->_n; c != _list; c = c->_n)
    if (c->_id == port)
      return c->_tickets;
  if (port >= 0 && port < ninputs())
    return 0;
  return -1;
}

int
StrideSched::set_tickets(int port, int tickets, ErrorHandler *errh)
{
  if (port < 0 || port >= ninputs())
    return errh->error("port %d out of range", port);
  else if (tickets < 0)
    return errh->error("number of tickets must be >= 0");
  else if (tickets > MAX_TICKETS) {
    errh->warning("port %d's tickets reduced to %d", port, MAX_TICKETS);
    tickets = MAX_TICKETS;
  }

  if (tickets == 0) {
    // delete Client
    for (Client *c = _list; c != _list; c = c->_n)
      if (c->_id == port) {
	c->remove();
	delete c;
	return 0;
      }
    return 0;
  }

  for (Client *c = _list->_n; c != _list; c = c->_n)
    if (c->_id == port) {
      c->set_tickets(tickets);
      return 0;
    }
  Client *c = new Client(port, tickets);
  c->_pass = _list->_n->_pass;
  _list->insert(c);
  return 0;
}

static String
read_tickets_handler(Element *e, void *thunk)
{
  StrideSched *ss = (StrideSched *)e;
  int port = (intptr_t)thunk;
  return String(ss->tickets(port)) + "\n";
}

static int
write_tickets_handler(const String &in_s, Element *e, void *thunk, ErrorHandler *errh)
{
  StrideSched *ss = (StrideSched *)e;
  int port = (intptr_t)thunk;
  String s = cp_uncomment(in_s);
  int tickets;
  if (!cp_integer(s, &tickets))
    return errh->error("tickets value must be integer");
  else
    return ss->set_tickets(port, tickets, errh);
}

void
StrideSched::add_handlers()
{
  for (int i = 0; i < ninputs(); i++) {
    String s = "tickets" + String(i);
    add_read_handler(s, read_tickets_handler, (void *)i);
    add_write_handler(s, write_tickets_handler, (void *)i);
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StrideSched)
