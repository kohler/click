// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * pokehandlers.{cc,hh} -- element runs read and write handlers
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
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

static const char * const READ_MARKER = "r";
Element * const PokeHandlers::STOP_MARKER = (Element *)1;
Element * const PokeHandlers::LOOP_MARKER = (Element *)2;

PokeHandlers::PokeHandlers()
    : _timer(timer_hook, this)
{
    MOD_INC_USE_COUNT;
}

PokeHandlers::~PokeHandlers()
{
    MOD_DEC_USE_COUNT;
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
PokeHandlers::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _h_element.clear();
    _h_handler.clear();
    _h_value.clear();
    _h_timeout.clear();
    
    uint32_t timeout;
    Element *e;
    String hname;

    int next_timeout = 0;
    for (int i = 0; i < conf.size(); i++) {
	String text = conf[i];
	String word = cp_pop_spacevec(text);
	if (!word)
	    /* ignore empty arguments */;
	else if (word == "quit" || word == "stop") {
	    if (i < conf.size() - 1 || text)
		errh->warning("arguments after `%s' directive ignored", word.cc());
	    add(STOP_MARKER, "", "", next_timeout);
	    break;
	} else if (word == "loop") {
	    if (i < conf.size() - 1 || text)
		errh->warning("arguments after `loop' directive ignored");
	    add(LOOP_MARKER, "", "", next_timeout);
	    break;
	} else if (word == "read" || word == "print") {
	    if (cp_handler(text, this, &e, &hname, errh)) {
		add(e, hname, String::stable_string(READ_MARKER), next_timeout);
		next_timeout = 0;
	    }
	} else if (word == "write") {
	    word = cp_pop_spacevec(text);
	    if (cp_handler(word, this, &e, &hname, errh)) {
		add(e, hname, text, next_timeout);
		next_timeout = 0;
	    }
	} else if (word == "wait") {
	    if (cp_seconds_as_milli(text, &timeout))
		next_timeout += timeout;
	    else
		errh->error("missing time in `wait TIME'");
	} else if (cp_seconds_as_milli(word, &timeout) && !text)
	    next_timeout += timeout;
	else
	    errh->error("unknown directive `%#s'", word.cc());
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
PokeHandlers::timer_hook(Timer *, void *thunk)
{
    PokeHandlers *poke = (PokeHandlers *)thunk;
    ErrorHandler *errh = ErrorHandler::default_handler();
    Router *router = poke->router();

    int h = poke->_pos;
    do {
	Element *he = poke->_h_element[h];
	const String &hname = poke->_h_handler[h];

	if (he == STOP_MARKER) {
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
	else if (poke->_h_value[h].data() == READ_MARKER) {
	    const Router::Handler &rh = router->handler(i);
	    if (rh.readable()) {
		ErrorHandler *errh = ErrorHandler::default_handler();
		String value = rh.call_read(he);
		errh->message("%s:\n%s\n", rh.unparse_name(he).cc(), value.cc());
	    } else
		errh->error("%s: no read handler `%s'", poke->id().cc(), Router::Handler::unparse_name(he, hname).cc());
	} else {
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
