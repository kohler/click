// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * pokehandlers.{cc,hh} -- element runs write handlers
 * Eddie Kohler
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

void
PokeHandlers::add(Element *e, const String &hname, const String &value, int timeout)
{
    _h_element.push_back(e);
    _h_handler.push_back(hname);
    _h_value.push_back(value);
    _h_timeout.push_back(timeout);
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
		add(QUIT_MARKER, "", "", next_timeout);
		if (i < conf.size() - 1)
		    errh->warning("arguments after `quit' directive ignored");
		break;
	    } else if (first == "loop") {
		add(LOOP_MARKER, "", "", next_timeout);
		if (i < conf.size() - 1)
		    errh->warning("arguments after `loop' directive ignored");
		break;
	    } else if (cp_seconds_as_milli(first, reinterpret_cast<uint32_t *>(&gap))) {
		next_timeout += gap;
		continue;
	    }
	}

	Element *e;
	String hname;
	if (cp_handler(first, this, &e, &hname, errh)) {
	    add(e, hname, rest, next_timeout);
	    next_timeout = 0;
	    continue;
	}

	errh->error("argument %d: expected `TIMEOUT' or `ELEMENT.HANDLER VALUE'", i + 1);
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
	const String &hname = poke->_h_handler[h];

	if (he == QUIT_MARKER) {
	    router->please_stop_driver();
	    h++;
	    break;
	} else if (he == LOOP_MARKER) {
	    h = 0;
	    break;
	}

	int i = router->find_handler(he, hname);
	if (i < 0)
	    errh->error("%s: no handler `%s'", poke->id().cc(), Router::Handler::unparse_name(he, hname).cc());
	else {
	    const Router::Handler &rh = router->handler(i);
	    if (rh.writable()) {
		ContextErrorHandler cerrh
		    (errh, "In write handler `" + rh.unparse_name(he) + "':");
		rh.call_write(poke->_h_value[h], he, &cerrh);
	    } else
		errh->error("%s: no write handler `%s'", poke->id().cc(), Router::Handler::unparse_name(he, hname).cc());
	}
	h++;
    } while (h < poke->_h_timeout.size() && poke->_h_timeout[h] == 0);

    if (h < poke->_h_timeout.size())
	poke->_timer.schedule_after_ms(poke->_h_timeout[h]);
    poke->_pos = h;
}

EXPORT_ELEMENT(PokeHandlers)
