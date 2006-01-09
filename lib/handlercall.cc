// -*- c-basic-offset: 4; related-file-name: "../include/click/handlercall.hh" -*-
/*
 * handlercall.{cc,hh} -- abstracts a call to a handler
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2004-2006 Regents of the University of California
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

int
HandlerCall::initialize(int flags, Element* context, ErrorHandler* errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();

    Element *e;
    String hname;
    String value = _value;

    if (initialized()) {
	e = _e;
	hname = _h->name();
	value = _value;
    } else {
	// parse handler name
	if (!cp_handler_name(cp_pop_spacevec(value), &e, &hname, context, errh))
	    return -EINVAL;
	// local handler reference
	if (e->eindex() == -1 && _value[0] != '.' && Router::handler(context, hname))
	    e = context;
    }

    // exit early if handlers not yet defined
    if (!e->router()->handlers_ready())
	return (flags & ALLOW_PREINITIALIZE ? 0 : errh->error("handlers not yet defined"));

    // finish up in assign()
    return assign(e, hname, value, flags, errh);
}

static int
handler_error(Element* e, const String& hname, bool write, ErrorHandler* errh)
{
    if (errh)
	errh->error((write ? "no '%s' write handler" : "no '%s' read handler"), Handler::unparse_name(e, hname).c_str());
    return -ENOENT;
}

int
HandlerCall::assign(Element* e, const String& hname, const String& value, int flags, ErrorHandler* errh)
{
    // find handler
    const Handler* h = Router::handler(e, hname);
    if (!h
	|| ((flags & CHECK_READ) && !h->readable())
	|| ((flags & CHECK_WRITE) && !h->writable()))
	return handler_error(e, hname, flags & CHECK_WRITE /* XXX */, errh);
    else if (value && (flags & CHECK_READ) && !h->read_param()) {
	errh->error("read handler '%s' does not take parameters", Handler::unparse_name(e, hname).c_str());
	return -EINVAL;
    }

    // assign
    _e = e;
    _h = h;
    _value = value;
    return 0;
}

int
HandlerCall::reset(HandlerCall*& call, const String& hdesc, int flags, Element* context, ErrorHandler* errh)
{
    HandlerCall hcall(hdesc);
    int retval = hcall.initialize(flags, context, errh);
    if (retval >= 0) {
	if (!call && !(call = new HandlerCall))
	    return -ENOMEM;
	*call = hcall;
    }
    return retval;
}

int
HandlerCall::reset(HandlerCall*& call, Element* e, const String& hname, const String& value, int flags, ErrorHandler* errh)
{
    HandlerCall hcall;
    int retval = hcall.assign(e, hname, value, flags, errh);
    if (retval >= 0) {
	if (!call && !(call = new HandlerCall))
	    return -ENOMEM;
	*call = hcall;
    }
    return retval;
}

String
HandlerCall::call_read(Element* e, const String& hname, ErrorHandler* errh)
{
    HandlerCall hc;
    if (hc.assign(e, hname, "", CHECK_READ, errh) >= 0)
	return hc._h->call_read(hc._e);
    else
	return "";
}

int
HandlerCall::call_write(Element* e, const String& hname, const String& value, ErrorHandler* errh)
{
    HandlerCall hc;
    int rv = hc.assign(e, hname, value, CHECK_WRITE, errh);
    return (rv >= 0 ? hc.call_write(errh) : rv);
}


String
HandlerCall::call_read(const String& hdesc, Element* e, ErrorHandler* errh)
{
    HandlerCall hcall(hdesc);
    if (hcall.initialize(CHECK_READ, e, errh) >= 0)
	return hcall.call_read();
    else
	return String();
}

int
HandlerCall::call_write(const String &hdesc, Element *e, ErrorHandler *errh)
{
    HandlerCall hcall(hdesc);
    if (hcall.initialize(CHECK_WRITE, e, errh) >= 0)
	return hcall.call_write(errh);
    else
	return -EINVAL;
}

int
HandlerCall::call_write(const String &hdesc, const String &value, Element *e, ErrorHandler *errh)
{
    HandlerCall hcall(hdesc);
    if (hcall.initialize(CHECK_WRITE, e, errh) >= 0) {
	hcall.set_value(value);
	return hcall.call_write(errh);
    } else
	return -EINVAL;
}


String
HandlerCall::unparse() const
{
    if (ok()) {
	String name = _h->unparse_name(_e);
	if (!_value)
	    return name;
	else
	    return name + " " + _value;
    } else
	return "<bad handler>";
}

CLICK_ENDDECLS
