/*
 * pokehandlers.{cc,hh} -- element runs write handlers
 * Eddie Kohler
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
#include "pokehandlers.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"

PokeHandlers::PokeHandlers()
  : _timer(timer_hook, (unsigned long)this)
{
}

PokeHandlers::~PokeHandlers()
{
  uninitialize();
}

int
PokeHandlers::configure(const String &conf, ErrorHandler *errh)
{
  _h_element.clear();
  _h_handler.clear();
  _h_value.clear();
  _h_timeout.clear();
  
  Vector<String> args;
  cp_argvec(conf, args);
  int next_timeout = 0;
  for (int i = 0; i < args.size(); i++) {
    String first, rest;
    cp_word(args[i], first, &rest);
    cp_eat_space(rest);
    if (!rest) {
      int gap;
      if (cp_va_parse(first, this, errh,
		      cpInterval, "timeout interval", &gap, 0) >= 0)
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
PokeHandlers::timer_hook(unsigned long thunk)
{
  PokeHandlers *poke = (PokeHandlers *)thunk;
  ErrorHandler *errh = ErrorHandler::default_handler();
  Router *router = poke->router();

  int h = poke->_pos;
  do {
    Element *he = poke->_h_element[h];
    String hname = poke->_h_handler[h];
    int i = router->find_handler(he, hname);
    if (i < 0)
      errh->error("%s: no handler `%s.%s'", poke->id().cc(), he->id().cc(), hname.cc());
    else {
      const Router::Handler &rh = router->handler(i);
      if (rh.write) {
	ContextErrorHandler cerrh
	  (errh, "In write handler `" + he->id() + "." + hname + "':");
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
