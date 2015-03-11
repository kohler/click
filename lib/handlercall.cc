// -*- c-basic-offset: 4; related-file-name: "../include/click/handlercall.hh" -*-
/*
 * handlercall.{cc,hh} -- abstracts a call to a handler
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2004-2011 Regents of the University of California
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
#include <click/args.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

/** @file handlercall.hh
 * @brief The HandlerCall helper class for simplifying access to handlers.
 */

int
HandlerCall::initialize(int flags, const Element* context, ErrorHandler* errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();

    Element *e = _e;
    String hname;
    String value = _value;
    _e = 0;			// "initialization attempted"

    if (!initialized()) {
	// parse handler name
	if (!cp_handler_name(cp_shift_spacevec(value), &e, &hname, context, errh))
	    return -EINVAL;
	// unquote if required
	if (flags & f_unquote_param)
	    value = cp_unquote(value);
    } else
	hname = _h->name();

    // exit early if handlers not yet defined
    if (!e->router()->handlers_ready()) {
	_e = reinterpret_cast<Element *>(4); // "initialization not attempted"
	if (flags & f_preinitialize)
	    return 0;
	else
	    return errh->error("handlers not yet defined");
    }

    // finish up in assign()
    return assign(e, hname, value, flags, errh);
}

static int
handler_error(Element* e, const String &hname, bool write, ErrorHandler* errh)
{
    if (errh)
	errh->error((write ? "no %<%s%> write handler" : "no %<%s%> read handler"), Handler::unparse_name(e, hname).c_str());
    return -ENOENT;
}

int
HandlerCall::assign(Element *e, const String &hname, const String &value, int flags, ErrorHandler* errh)
{
    // find handler
    const Handler* h = Router::handler(e, hname);
    if (!h
	|| ((flags & OP_WRITE) && !h->writable()))
	return handler_error(e, hname, flags & OP_WRITE, errh);
    else if ((flags & OP_READ) && !h->readable())
	return handler_error(e, hname, false, errh);
    else if (value && (flags & OP_READ) && !h->read_param()) {
	errh->error("read handler %<%s%> does not take parameters", Handler::unparse_name(e, hname).c_str());
	return -EINVAL;
    }

    // assign
    _e = e;
    _h = h;
    _value = value;
    return 0;
}

int
HandlerCall::reset(HandlerCall*& call, const String& hdesc, int flags, const Element* context, ErrorHandler* errh)
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

/** @brief  Call a read handler specified by element and handler name.
 *  @param  e      relevant element, if any
 *  @param  hname  handler name
 *  @param  errh   optional error handler
 *  @return  handler result, or empty string on error
 *
 *  Searches for a read handler named @a hname on element @a e.  If the
 *  handler exists, calls it (with no parameters) and returns the result.  If
 *  @a errh is nonnull, then errors, such as a missing handler or a write-only
 *  handler, are reported there.  If @a e is some router's @link
 *  Router::root_element() root element@endlink, calls the global handler
 *  named @a hname on that router. */
String
HandlerCall::call_read(Element *e, const String &hname, ErrorHandler *errh)
{
    HandlerCall hc;
    String empty;
    if (hc.assign(e, hname, empty, OP_READ, errh) >= 0)
	return hc._h->call_read(hc._e, empty, errh);
    else
	return empty;
}

/** @brief  Call a read handler.
 *  @param  hdesc    handler description <tt>"[ename.]hname[ params]"</tt>
 *  @param  context  optional element context
 *  @param  errh     optional error handler
 *  @return  handler result, or empty string on error
 *
 *  Searches for a read handler matching @a hdesc.  Any element name in @a
 *  hdesc is looked up relative to @a context.  (For example, if @a hdesc is
 *  "x.config" and @a context's name is "aaa/bbb/ccc", will search for
 *  elements named aaa/bbb/x, aaa/x, and finally x.)  If the handler exists,
 *  calls it (with specified parameters, if any) and returns the result.  If
 *  @a errh is nonnull, then errors, such as a missing handler or a write-only
 *  handler, are reported there.  If @a hdesc has no <tt>ename</tt>, then
 *  calls the global handler named <tt>hname</tt> on @a context's router. */
String
HandlerCall::call_read(const String &hdesc, const Element *context, ErrorHandler* errh)
{
    HandlerCall hcall(hdesc);
    if (hcall.initialize(OP_READ, context, errh) >= 0)
	return hcall.call_read();
    else
	return String();
}

/** @brief  Call a write handler specified by element and handler name.
 *  @param  e      relevant element, if any
 *  @param  hname  handler name
 *  @param  value  write value
 *  @param  errh   optional error handler
 *  @return  handler result, or -EINVAL on error
 *
 *  Searches for a write handler named @a hname on element @a e.  If the
 *  handler exists, calls it with @a value and returns the result.
 *  If @a errh is nonnull, then errors, such as a missing handler or a
 *  read-only handler, are reported there.  If @a e is some router's @link
 *  Router::root_element() root element@endlink, calls the global write
 *  handler named @a hname on that router. */
int
HandlerCall::call_write(Element* e, const String& hname, const String& value, ErrorHandler* errh)
{
    HandlerCall hc;
    int rv = hc.assign(e, hname, value, OP_WRITE, errh);
    return (rv >= 0 ? hc.call_write(errh) : rv);
}

/** @brief  Call a write handler.
 *  @param  hdesc    handler description <tt>"[ename.]hname[ value]"</tt>
 *  @param  context  optional element context
 *  @param  errh     optional error handler
 *  @return  handler result, or -EINVAL on error
 *
 *  Searches for a write handler matching @a hdesc.  Any element name in @a
 *  hdesc is looked up relative to @a context (see @link
 *  HandlerCall::call_read(const String &, const Element *, ErrorHandler *)
 *  above@endlink).  If the handler exists, calls it with the value
 *  specified by @a hdesc and returns the result.  If @a errh is nonnull, then
 *  errors, such as a missing handler or a read-only handler, are reported
 *  there.  If @a e is some router's @link Router::root_element() root
 *  element@endlink, calls the global write handler named @a hname on that
 *  router. */
int
HandlerCall::call_write(const String &hdesc, const Element *context, ErrorHandler *errh)
{
    HandlerCall hcall(hdesc);
    if (hcall.initialize(OP_WRITE, context, errh) >= 0)
	return hcall.call_write(errh);
    else
	return -EINVAL;
}

/** @brief  Call a write handler with a specified value.
 *  @param  hdesc    handler description <tt>"[ename.]hname[ value]"</tt>
 *  @param  value    handler value
 *  @param  context  optional element context
 *  @param  errh     optional error handler
 *  @return  handler result, or -EINVAL on error
 *
 *  Searches for a write handler matching @a hdesc.  Any element name in @a
 *  hdesc is looked up relative to @a context (see @link
 *  HandlerCall::call_read(const String &, const Element *, ErrorHandler *)
 *  above@endlink).  If the handler exists, calls it with @a value and returns
 *  the result.  (Any value specified by @a hdesc is ignored.)  If @a errh is
 *  nonnull, then errors, such as a missing handler or a read-only handler,
 *  are reported there.  If @a e is some router's @link Router::root_element()
 *  root element@endlink, calls the global write handler named @a hname on
 *  that router. */
int
HandlerCall::call_write(const String &hdesc, const String &value, const Element *context, ErrorHandler *errh)
{
    HandlerCall hcall(hdesc);
    if (hcall.initialize(OP_WRITE, context, errh) >= 0) {
	hcall.set_value(value);
	return hcall.call_write(errh);
    } else
	return -EINVAL;
}

String
HandlerCall::unparse() const
{
    if (initialized()) {
	String name = _h->unparse_name(_e);
	if (!_value)
	    return name;
	else
	    return name + " " + _value;
    } else if (_value)
	return _value;
    else
	return "<empty handler>";
}


bool
HandlerCallArg::parse(const String &str, HandlerCall &result, const ArgContext &args)
{
    HandlerCall hc(str);
    PrefixErrorHandler perrh(args.errh(), args.error_prefix());
    if (hc.initialize(flags | HandlerCall::f_preinitialize, args.context(), &perrh) >= 0) {
	result = hc;
	return true;
    } else
	return false;
}

CLICK_ENDDECLS
