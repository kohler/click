// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * pokehandlers.{cc,hh} -- element runs read and write handlers
 * Eddie Kohler
 * Pausing idea & initial implementation from Daniel Aguayo.
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
CLICK_DECLS

static const char READ_MARKER[] = "r";
Element * const PokeHandlers::STOP_MARKER = (Element *)1;
Element * const PokeHandlers::LOOP_MARKER = (Element *)2;
Element * const PokeHandlers::PAUSE_MARKER = (Element *)3;

PokeHandlers::PokeHandlers()
    : _timer(timer_hook, this)
{
}

PokeHandlers::~PokeHandlers()
{
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

    errh->warning("The PokeHandlers element is deprecated. Use Script instead.");

    int next_timeout = 0;
    for (int i = 0; i < conf.size(); i++) {
	String text = conf[i];
	String word = cp_pop_spacevec(text);

	if (!word)
	    /* ignore empty arguments */;
	else if (word == "quit" || word == "stop") {
	    if (i < conf.size() - 1 || text)
		errh->warning("arguments after '%s' directive ignored", word.c_str());
	    add(STOP_MARKER, "", "", next_timeout);
	    break;
	} else if (word == "loop") {
	    if (i < conf.size() - 1 || text)
		errh->warning("arguments after 'loop' directive ignored");
	    add(LOOP_MARKER, "", "", next_timeout);
	    break;
	} else if (word == "pause") {
	    add(PAUSE_MARKER, "", "", next_timeout);
	    next_timeout = 0;
	} else if (word == "read" || word == "print") {
	    if (cp_handler_name(text, &e, &hname, this, errh)) {
		add(e, hname, String::stable_string(READ_MARKER), next_timeout);
		next_timeout = 0;
	    }
	} else if (word == "write") {
	    word = cp_pop_spacevec(text);
	    if (cp_handler_name(word, &e, &hname, this, errh)) {
		add(e, hname, text, next_timeout);
		next_timeout = 0;
	    }
	} else if (word == "wait") {
	    if (cp_seconds_as_milli(text, &timeout))
		next_timeout += timeout;
	    else
		errh->error("missing time in 'wait TIME'");
	} else if (cp_seconds_as_milli(word, &timeout) && !text)
	    next_timeout += timeout;
	else
	    errh->error("unknown directive '%#s'", word.c_str());
    }

    if (_timer.initialized()) {
	// we must be live reconfiguring
	_pos = 0;
	_paused = false;
	_timer.unschedule();
	if (_h_timeout.size() != 0)
	    _timer.schedule_after_msec(_h_timeout[0] + 1);
    }

    return 0;
}

int
PokeHandlers::initialize(ErrorHandler *)
{
    _pos = 0;
    _paused = false;
    _timer.initialize(this);
    if (_h_timeout.size() != 0)
	_timer.schedule_after_msec(_h_timeout[0] + 1);
    return 0;
}

void
PokeHandlers::add_handlers()
{
    add_write_handler("unpause", write_param, (void *)0);
    add_read_handler("paused", read_param, (void *)0);
}

String
PokeHandlers::read_param(Element *e, void *)
{
    PokeHandlers *p = (PokeHandlers *)e;
    return cp_unparse_bool(p->_paused);
}

int
PokeHandlers::write_param(const String &, Element *e, void *, ErrorHandler *)
{
    PokeHandlers *p = (PokeHandlers *)e;
    p->unpause();
    return 0;
}

void
PokeHandlers::unpause() {
    if (!_paused)
	return;
    _paused = false;
    if (_pos < _h_timeout.size())
	_timer.schedule_after_msec(_h_timeout[_pos]); // XXX +1 ms? 
}

void
PokeHandlers::timer_hook(Timer *, void *thunk)
{
    PokeHandlers *poke = (PokeHandlers *)thunk;
    ErrorHandler *errh = ErrorHandler::default_handler();
    PrefixErrorHandler perrh(errh, poke->name() + ": ");
    Router *router = poke->router();

    int hpos = poke->_pos;
    do {
	Element *he = poke->_h_element[hpos];
	const String &hname = poke->_h_handler[hpos];

	if (he == STOP_MARKER) {
	    router->please_stop_driver();
	    hpos++;
	    break;
	} else if (he == LOOP_MARKER) {
	    hpos = 0;
	    break;
	} else if (he == PAUSE_MARKER) {
	    poke->_paused = true;
	    hpos++;
	    break;
	}
	
	const Handler *h = Router::handler(he, hname);
	int before = perrh.nerrors();
	if (!h)
	    perrh.error("no handler '%s'", Handler::unparse_name(he, hname).c_str());
	else if (poke->_h_value[hpos].data() == READ_MARKER) {
	    String value = h->call_read(he, &perrh);
	    if (perrh.nerrors() == before)
		errh->message("%s:\n%s\n", h->unparse_name(he).c_str(), value.c_str());
	} else {
	    if (h->writable()) {
		ContextErrorHandler cerrh
		    (errh, "In write handler '" + h->unparse_name(he) + "':");
		h->call_write(poke->_h_value[hpos], he, false, &cerrh);
	    } else
		perrh.error("no write handler '%s'", h->unparse_name(he).c_str());
	}
	hpos++;
    } while (hpos < poke->_h_timeout.size() && poke->_h_timeout[hpos] == 0);

    if (hpos < poke->_h_timeout.size() && !poke->_paused)
	poke->_timer.schedule_after_msec(poke->_h_timeout[hpos]);
    poke->_pos = hpos;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PokeHandlers)
