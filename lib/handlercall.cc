// -*- mode: c++; c-basic-offset: 4 -*-
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

const char * const HandlerCall::READ_MARKER = "r";

int
HandlerCall::initialize(String what, bool write, Element *context, ErrorHandler *errh)
{
    _e = 0;
    _hi = -1;
    _value = String();
    if (write)
	return cp_va_space_parse
	    (what, context, errh,
	     cpWriteHandler, "write handler name", &_e, &_hi,
	     cpOptional,
	     cpString, "value", &_value,
	     0);
    else
	return cp_va_space_parse
	    (what, context, errh,
	     cpReadHandler, "read handler name", &_e, &_hi,
	     0);
}

String
HandlerCall::call_read(const Element *context)
{
    if (!ok() || !is_read())
	return String();
    const Router::Handler &h = context->router()->handler(_hi);
    return h.call_read(_e);
}

int
HandlerCall::call_write(const Element *context, ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    if (!ok() || is_read())
	return errh->error("not a write handler");
    const Router::Handler &h = context->router()->handler(_hi);
    return h.call_write(_value, _e, errh);
}

String
HandlerCall::call_read(Router *router, Element *e, const String &hname, ErrorHandler *errh = 0)
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
HandlerCall::call_read(Router *router, const String &ename, const String &hname, ErrorHandler *errh = 0)
{
    Element *e = 0;
    if (ename) {
	if (!(e = router->find(ename, errh)))
	    return "<no element>";
    }
    return call_read(router, e, hname, errh);
}

int
HandlerCall::call_write(Router *router, Element *e, const String &hname, const String &value, ErrorHandler *errh = 0)
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
HandlerCall::call_write(Router *router, const String &ename, const String &hname, const String &value, ErrorHandler *errh = 0)
{
    Element *e = 0;
    if (ename) {
	if (!(e = router->find(ename, errh)))
	    return -ENOENT;
    }
    return call_write(router, e, hname, value, errh);
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
