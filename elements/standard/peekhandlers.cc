/*
 * peekhandlers.{cc,hh} -- element runs read handlers
 * Eddie Kohler
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
#include "peekhandlers.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"

PeekHandlers::PeekHandlers()
  : _timer(timer_hook, (unsigned long)this)
{
}

PeekHandlers::~PeekHandlers()
{
  uninitialize();
}

int
PeekHandlers::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _h_element.clear();
  _h_handler.clear();
  _h_timeout.clear();
  
  int next_timeout = 0;
  for (int i = 0; i < conf.size(); i++) {
    if (!conf[i])
      continue;
    
    String first;
    cp_word(conf[i], &first);
    int gap;
    if (cp_milliseconds(first, &gap)) {
      next_timeout += gap;
      continue;
    } else if (first == "quit") {
      _h_element.push_back(0);
      _h_handler.push_back("");
      _h_timeout.push_back(next_timeout);
      if (i < conf.size() - 1)
	errh->warning("arguments after `quit' directive ignored");
      break;
    } else {
      int dot = first.find_left('.');
      if (dot >= 0) {
	if (Element *e = router()->find(this, first.substring(0, dot), errh)) {
	  _h_element.push_back(e);
	  _h_handler.push_back(first.substring(dot + 1));
	  _h_timeout.push_back(next_timeout);
	  next_timeout = 0;
	}
	continue;
      }
    }
    
    errh->error("argument %d: expected `TIMEOUT' or `ELEMENT.HANDLER'",
		i + 1);
  }

  return 0;
}

int
PeekHandlers::initialize(ErrorHandler *)
{
  _pos = 0;
  _timer.attach(this);
  if (_h_timeout.size() != 0)
    _timer.schedule_after_ms(_h_timeout[0] + 1);
  return 0;
}

void
PeekHandlers::uninitialize()
{
  _timer.unschedule();
}


void
PeekHandlers::timer_hook(unsigned long thunk)
{
  PeekHandlers *peek = (PeekHandlers *)thunk;
  ErrorHandler *errh = ErrorHandler::default_handler();
  Router *router = peek->router();

  int h = peek->_pos;
  do {
    Element *he = peek->_h_element[h];
    String hname = peek->_h_handler[h];

    if (he == 0) {		// `quit' directive
      router->please_stop_driver();
      h++;
      break;
    }
      
    int i = router->find_handler(he, hname);
    if (i < 0)
      errh->error("%s: no handler `%s.%s'", peek->id().cc(), he->id().cc(), hname.cc());
    else {
      const Router::Handler &rh = router->handler(i);
      if (rh.read) {
	String value = rh.read(he, rh.read_thunk);
	errh->message("%s.%s:", he->id().cc(), hname.cc());
	errh->message(value);
      } else
	errh->error("%s: no read handler `%s.%s'", peek->id().cc(), he->id().cc(), hname.cc());
    }
    h++;
  } while (h < peek->_h_timeout.size() && peek->_h_timeout[h] == 0);

  if (h < peek->_h_timeout.size())
    peek->_timer.schedule_after_ms(peek->_h_timeout[h]);
  peek->_pos = h;
}

EXPORT_ELEMENT(PeekHandlers)
