// -*- c-basic-offset: 4; related-file-name: "../include/click/handlercall.hh" -*-
/*
 * handlercall.{cc,hh} -- abstracts a call to a handler
 * Eddie Kohler
 *
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
#include <click/handlercall.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

const char * const HandlerCall::READ_MARKER = "r";

int
HandlerCall::initialize(String what, bool write, Element *context, ErrorHandler *errh)
{
    String original_what = what;
    _e = 0;
    _hi = -1;
    
    if (context->router()->nhandlers() < 0) {
	Element *e;
	String hname, value;
	if (!cp_handler(cp_pop_spacevec(what), context, &e, &hname, errh))
	    return -1;
    } else {
	if (!cp_handler(cp_pop_spacevec(what), context, !write, write, &_e, &_hi, errh))
	    return -1;
    }

    if (what) {
	if (write && !cp_string(what, &_value))
	    return (errh ? errh->error("malformed value after handler name") : -1);
	else if (!write)
	    return (errh ? errh->error("garbage after handler name") : -1);
    }

    if (context->router()->nhandlers() < 0)
    	_value = original_what;
    return 0;
}

int
HandlerCall::initialize(HandlerCall *&call, const String &text, bool write, Element *context, ErrorHandler *errh)
{
    if (!call && !(call = new HandlerCall))
	return -ENOMEM;
    int retval = call->initialize(text, write, context, errh);
    if (retval < 0) {
	delete call;
	call = 0;
    }
    return retval;
}

String
HandlerCall::call_read(Router *router) const
{
    if (!ok() || !is_read())
	return String();
    const Router::Handler &h = router->handler(_hi);
    return h.call_read(_e);
}

int
HandlerCall::call_write(Router *router, ErrorHandler *errh) const
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    if (!ok() || is_read())
	return errh->error("not a write handler");
    const Router::Handler &h = router->handler(_hi);
    return h.call_write(_value, _e, errh);
}

String
HandlerCall::call_read(Router *router, Element *e, const String &hname, ErrorHandler *errh)
{
    const Router::Handler *h = 0;
    int hi = router->find_handler(e, hname);
    if (hi >= 0)
	h = &router->handler(hi);
    if (!h || !h->readable()) {
	if (errh && e)
	    errh->error("`%s' has no `%s' read handler", e->declaration().cc(), String(hname).cc());
	else if (errh)
	    errh->error("no `%s' global read handler", String(hname).cc());
	return "<no handler>";
    }
    return h->call_read(e);
}

String
HandlerCall::call_read(Router *router, const String &ename, const String &hname, ErrorHandler *errh)
{
    Element *e = 0;
    if (ename) {
	if (!(e = router->find(ename, errh)))
	    return "<no element>";
    }
    return call_read(router, e, hname, errh);
}

int
HandlerCall::call_write(Router *router, Element *e, const String &hname, const String &value, ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    const Router::Handler *h = 0;
    int hi = router->find_handler(e, hname);
    if (hi >= 0)
	h = &router->handler(hi);
    if (!h || !h->writable()) {
	if (e)
	    errh->error("`%s' has no `%s' write handler", e->declaration().cc(), String(hname).cc());
	else
	    errh->error("no `%s' global write handler", String(hname).cc());
	return -EACCES;
    }
    return h->call_write(value, e, errh);
}

int
HandlerCall::call_write(Router *router, const String &ename, const String &hname, const String &value, ErrorHandler *errh)
{
    Element *e = 0;
    if (ename) {
	if (!(e = router->find(ename, errh)))
	    return -ENOENT;
    }
    return call_write(router, e, hname, value, errh);
}

String
HandlerCall::call_read(Router *router, const String &hcall, ErrorHandler *errh)
{
    HandlerCall hc;
    if (hc.initialize(hcall, false, router->root_element(), errh) >= 0)
	return hc.call_read(router);
    else
	return String();
}

int
HandlerCall::call_write(Router *router, const String &hcall, ErrorHandler *errh)
{
    HandlerCall hc;
    if (hc.initialize(hcall, true, router->root_element(), errh) >= 0)
	return hc.call_write(router, errh);
    else
	return -1;
}

String
HandlerCall::unparse(const Element *context) const
{
    if (!ok())
	return "<bad handler>";
    else {
	String name = context->router()->handler(_hi).unparse_name(_e);
	if (is_read() || !_value)
	    return name;
	else
	    return name + " " + cp_quote(_value);
    }
}

CLICK_ENDDECLS
