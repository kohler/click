/*
 * pokehandlers.{cc,hh} -- element runs write handlers
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

#include <click/config.h>
#include <click/package.hh>
#include "pokehandlers.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>

PokeHandlers::PokeHandlers()
  : _timer(timer_hook, this)
{
  MOD_INC_USE_COUNT;
}

PokeHandlers::~PokeHandlers()
{
  MOD_DEC_USE_COUNT;
  uninitialize();
}

int
PokeHandlers::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _h_element.clear();
  _h_handler.clear();
  _h_value.clear();
  _h_timeout.clear();
  
  int next_timeout = 0;
  for (int i = 0; i < conf.size(); i++) {
    if (!conf[i])
      continue;
    
    String first, rest;
    cp_word(conf[i], &first, &rest);
    cp_eat_space(rest);
    if (!rest) {
      int gap;
      if (first == "quit") {
	_h_element.push_back(0);
	_h_handler.push_back("");
	_h_value.push_back("");
	_h_timeout.push_back(next_timeout);
	if (i < conf.size() - 1)
	  errh->warning("arguments after `quit' directive ignored");
	break;
      } else if (cp_milliseconds(first, &gap))
	next_timeout += gap;
      continue;
    } else {
      int dot = first.find_left('.');
      if (dot >= 0) {
	if (Element *e = router()->find(this, first.substring(0, dot), errh)) {
	  _h_element.push_back(e);
	  _h_handler.push_back(first.substring(dot + 1));
	  _h_value.push_back(rest);
	  _h_timeout.push_back(next_timeout);
	  next_timeout = 0;
	}
	continue;
      }
    }
    
    errh->error("argument %d: expected `TIMEOUT' or `ELEMENT.HANDLER VALUE'",
		i + 1);
  }

  return 0;
}

int
PokeHandlers::initialize(ErrorHandler *)
{
  _pos = 0;
  _timer.initialize(this);
  if (_h_timeout.size() != 0)
    _timer.schedule_after_ms(_h_timeout[0] + 1);
  return 0;
}

void
PokeHandlers::uninitialize()
{
  _timer.unschedule();
}


void
PokeHandlers::timer_hook(Timer *, void *thunk)
{
  PokeHandlers *poke = (PokeHandlers *)thunk;
  ErrorHandler *errh = ErrorHandler::default_handler();
  Router *router = poke->router();

  int h = poke->_pos;
  do {
    Element *he = poke->_h_element[h];
    String hname = poke->_h_handler[h];

    if (he == 0) {		// `quit' directive
      router->please_stop_driver();
      h++;
      break;
    }
      
    int i = router->find_handler(he, hname);
    if (i < 0)
      errh->error("%s: no handler `%s.%s'", poke->id().cc(), he->id().cc(), hname.cc());
    else {
      const Router::Handler &rh = router->handler(i);
      if (rh.write) {
	ContextErrorHandler cerrh
	  (errh, "In write handler `" + hname + "' for `" + he->declaration() + "':");
	rh.write(poke->_h_value[h], he, rh.write_thunk, &cerrh);
      } else
	errh->error("%s: no write handler `%s.%s'", poke->id().cc(), he->id().cc(), hname.cc());
    }
    h++;
  } while (h < poke->_h_timeout.size() && poke->_h_timeout[h] == 0);

  if (h < poke->_h_timeout.size())
    poke->_timer.schedule_after_ms(poke->_h_timeout[h]);
  poke->_pos = h;
}

EXPORT_ELEMENT(PokeHandlers)
