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
StrideSched::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() < 1) {
    errh->error("StrideSched must be configured with at least one ticket");
    return -1;
  }
  set_ninputs(conf.size());

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
  return 0;
}

void
StrideSched::uninitialize()
{
  Client *c;
  while ((c = _list->remove_min())) {
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

EXPORT_ELEMENT(StrideSched)
