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

inline void
HandlerCall::assign(Element *e, int hi, const String &value, bool write)
{
    _e = e;
    _hi = hi;
    _value = (write ? value : String::stable_string(READ_MARKER));
}

int
HandlerCall::parse(const String &hdesc, bool write, Element *context, ErrorHandler *errh)
{
    String s = hdesc;
    bool ready = context->router()->nhandlers() >= 0;
    Element *e;
    int hi;
    String value;

    // parse handler name
    if (ready) {
	if (!cp_handler(cp_pop_spacevec(s), context, !write, write, &e, &hi, errh))
	    return -EINVAL;
    } else {
	String hname;
	if (!cp_handler(cp_pop_spacevec(s), context, &e, &hname, errh))
	    return -EINVAL;
    }

    // parse value for a write handler
    if (s) {
	if (!write)
	    return (errh ? errh->error("garbage after handler name") : -EINVAL);
	else if (!cp_string(s, &value))
	    return (errh ? errh->error("malformed value after handler name") : -EINVAL);
    }

    // if we get here, success
    // if ready, store data; otherwise, store descriptor
    if (ready)
	assign(e, hi, value, write);
    else
	reset(hdesc);
    return 0;
}

int
HandlerCall::reset(HandlerCall *&call, const String &text, bool write, Element *context, ErrorHandler *errh)
{
    if (!call && !(call = new HandlerCall))
	return -ENOMEM;
    int retval = call->parse(text, write, context, errh);
    if (retval < 0 && !call->ok()) {
	delete call;
	call = 0;
    }
    return retval;
}

static int
handler_error(Element *e, const String &hname, bool write, ErrorHandler *errh)
{
    if (errh)
	errh->error((write ? "no `%s' write handler" : "no `%s' read handler"), Router::Handler::unparse_name(e, hname).cc());
    return -ENOENT;
}

int
HandlerCall::reset(HandlerCall *&call, Element *e, const String &hname, const String &value, bool write, ErrorHandler *errh)
{
    int hi = Router::hindex(e, hname);
    const Router::Handler *h = Router::handler(e, hname);
    if (h && (write ? h->writable() : h->readable())) {
	if (!call && !(call = new HandlerCall))
	    return -ENOMEM;
	call->assign(e, hi, value, write);
	return 0;
    } else
	return handler_error(e, hname, write, errh);
}


String
HandlerCall::call_read() const
{
    if (ok() && is_read())
	return Router::handler(_e, _hi)->call_read(_e);
    else
	return String();
}

int
HandlerCall::call_write(ErrorHandler *errh) const
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    if (ok() && !is_read())
	return Router::handler(_e, _hi)->call_write(_value, _e, errh);
    else
	return errh->error("not a write handler");
}


String
HandlerCall::call_read(Element *e, const String &hname, ErrorHandler *errh)
{
    const Router::Handler *h = Router::handler(e, hname);
    if (h && h->readable())
	return h->call_read(e);
    else {
	if (errh)
	    handler_error(e, hname, false, errh);
	return "";
    }
}

int
HandlerCall::call_write(Element *e, const String &hname, const String &value, ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    const Router::Handler *h = Router::handler(e, hname);
    if (h && h->writable())
	return h->call_write(value, e, errh);
    else {
	handler_error(e, hname, true, errh);
	return -EACCES;
    }
}


String
HandlerCall::call_read(const String &hdesc, Router *router, ErrorHandler *errh)
{
    HandlerCall hcall(hdesc);
    if (hcall.initialize_read(router->root_element(), errh) < 0)
	return String();
    else
	return hcall.call_read();
}

int
HandlerCall::call_write(const String &hdesc, Router *router, ErrorHandler *errh)
{
    HandlerCall hcall(hdesc);
    if (hcall.initialize_write(router->root_element(), errh) < 0)
	return -EINVAL;
    else
	return hcall.call_write(errh);
}


String
HandlerCall::unparse() const
{
    if (ok()) {
	String name = Router::handler(_e, _hi)->unparse_name(_e);
	if (is_read() || !_value)
	    return name;
	else
	    return name + " " + cp_quote(_value);
    } else
	return "<bad handler>";
}

CLICK_ENDDECLS
