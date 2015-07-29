// -*- c-basic-offset: 4; related-file-name: "../../lib/handlercall.cc" -*-
#ifndef CLICK_HANDLERCALL_HH
#define CLICK_HANDLERCALL_HH
#include <click/router.hh>
CLICK_DECLS
class VariableExpander;

/** @brief  Convenience class for calling handlers.
 *
 *  The HandlerCall class simplifies the process of calling Click handlers.
 *  (The lower-level interface is the Handler class.)  HandlerCall is used in
 *  two ways: (1) to call handlers immediately via static member functions,
 *  and (2) to set up future handler calls via HandlerCall objects.  The
 *  immediate handler call functions take handler names as arguments and
 *  perform all necessary error checks before calling handlers, if any.  A
 *  HandlerCall object encapsulates a handler reference (possibly including
 *  parameters), again automating all necessary error checks.
 *
 *  <h2>Immediate Handler Calls</h2>
 *
 *  This example code shows how to use the HandlerCall functions for calling
 *  handlers immediately.
 *
 *  @code
 *  class YourElement { ...
 *      Element *_other;
 *  }
 *
 *  void YourElement::function() {
 *      // Call _other's "config" read handler.
 *      String result = HandlerCall::call_read(_other, "config");
 *
 *      // The same, providing an error handler to print errors.
 *      ErrorHandler *errh = ErrorHandler::default_handler();
 *      result = HandlerCall::call_read(_other, "config", errh);
 *      // (Each function takes an optional last argument of "errh".)
 *
 *      // Call the "foo.config" read handler.  Search for element "foo" in
 *      // the current compound element context.
 *      result = HandlerCall::call_read("foo.config", this);
 *
 *      // Call the global "config" read handler for the current router.
 *      result = HandlerCall::call_read("config", this);
 *
 *      // Call _other's "stop" write handler with empty value.
 *      int success = HandlerCall::call_write(_other, "stop");
 *
 *      // Call _other's "config" write handler with value "blah".
 *      success = HandlerCall::call_write(_other, "config", "blah");
 *
 *      // Call the "foo.config" write handler with value "blah".
 *      success = HandlerCall::call_write("foo.config blah", this);
 *      // Or, alternately:
 *      success = HandlerCall::call_write("foo.config", "blah", this);
 *  }
 *  @endcode
 *
 *  <h2>HandlerCall Objects</h2>
 *
 *  This example code shows how to use the HandlerCall objects to call
 *  handlers with simplified error checking.
 *
 *  @code
 *  class YourElement { ...
 *      HandlerCall _read_call;
 *      HandlerCall _write_call;
 *  }
 *
 *  YourElement::YourElement()
 *      : _read_call(), _write_call() {
 *  }
 *
 *  int YourElement::configure(Vector<String> &conf, ErrorHandler *errh) {
 *      return cp_va_kparse(conf, this, errh,
 *                     "READ_CALL", cpkP, cpHandlerCallRead, &_read_call,
 *                     "WRITE_CALL", cpkP, cpHandlerCallWrite, &_write_call,
 *                     cpEnd);
 *  }
 *
 *  int YourElement::initialize(ErrorHandler *errh) {
 *      if ((_read_call && _read_call.initialize_read(this, errh) < 0)
 *          || (_write_call && _write_call.initialize_write(this, errh) < 0))
 *          return -1;
 *      return 0;
 *  }
 *
 *  void YourElement::function() {
 *      // call _read_call, print result
 *      if (_read_call)
 *          click_chatter("%s", _read_call.call_read());
 *
 *      // call _write_call with error handler
 *      if (_write_call)
 *          _write_call.call_write(ErrorHandler::default_handler());
 *  }
 *  @endcode
 *
 *  If usually your element's handler calls aren't used, you can save a small
 *  amount of space by using pointers to HandlerCall objects, as in this
 *  example.  The cpHandlerCallPtrRead and cpHandlerCallPtrWrite types allow
 *  the _read_call and _write_call members to start out as null pointers.
 *
 *  @code
 *  class YourElement { ...
 *      HandlerCall *_read_call;
 *      HandlerCall *_write_call;
 *  }
 *
 *  YourElement::YourElement()
 *      : _read_call(0), _write_call(0) {
 *  }
 *
 *  int YourElement::configure(Vector<String> &conf, ErrorHandler *errh) {
 *      return cp_va_kparse(conf, this, errh,
 *                     "READ_CALL", cpkP, cpHandlerCallPtrRead, &_read_call,
 *                     "WRITE_CALL", cpkP, cpHandlerCallPtrWrite, &_write_call,
 *                     cpEnd);
 *  }
 *
 *  int YourElement::initialize(ErrorHandler *errh) {
 *      if ((_read_call && _read_call->initialize_read(this, errh) < 0)
 *          || (_write_call && _write_call->initialize_write(this, errh) < 0))
 *          return -1;
 *      return 0;
 *  }
 *
 *  void YourElement::cleanup(CleanupStage) {
 *      delete _read_call;
 *      delete _write_call;
 *  }
 *
 *  void YourElement::function() {
 *      // call _read_call, print result
 *      if (_read_call)
 *          click_chatter("%s", _read_call->call_read());
 *
 *      // call _write_call with error handler
 *      if (_write_call)
 *          _write_call->call_write(ErrorHandler::default_handler());
 *  }
 *  @endcode
 */
class HandlerCall { public:

    /** @name Immediate Handler Calls */
    //@{
    static String call_read(Element *e, const String &hname,
			    ErrorHandler *errh = 0);
    static String call_read(const String &hdesc, const Element *context,
			    ErrorHandler *errh = 0);
    static int call_write(Element *e, const String &hname,
			  ErrorHandler *errh = 0);
    static int call_write(Element *e, const String &hname, const String &value,
			  ErrorHandler *errh = 0);
    static int call_write(const String &hdesc,
			  const Element *context, ErrorHandler *errh = 0);
    static int call_write(const String &hdesc, const String &value,
			  const Element *context, ErrorHandler *errh = 0);
    //@}



    /** @brief  Construct an empty HandlerCall.
     *
     *  Any attempt to read, write, or initialize the HandlerCall will
     *  fail. */
    HandlerCall()
	: _e(0), _h(Handler::blank_handler()) {
    }

    /** @brief  Construct a HandlerCall described by @a hdesc.
     *  @param  hdesc  handler description <tt>"[ename.]hname[ value]"</tt>
     *
     *  Although the resulting HandlerCall isn't empty, it must be initialized
     *  before it can be used.  It returns false for initialized().  The
     *  handler description is not checked for syntax errors, though;
     *  initialize() does that. */
    HandlerCall(const String &hdesc)
	: _e(reinterpret_cast<Element *>(4)), _h(Handler::blank_handler()),
	  _value(hdesc) {
    }


    enum Flags {
	readable = Handler::f_read,
	f_read = Handler::f_read,
	writable = Handler::f_write,
	f_write = Handler::f_write,
	f_preinitialize = 4,
        f_unquote_param = 8
    };

    /** @brief  Initialize the HandlerCall.
     *  @param  flags    zero or more of f_read, f_write, f_preinitialize,
     *                   f_unquote_param
     *  @param  context  optional element context
     *  @param  errh     optional error handler
     *  @return 0 on success, negative on failure
     *
     *  Initializes the HandlerCall object.  The handler description supplied
     *  to the constructor is parsed and checked for syntax errors.  Any
     *  element reference is looked up relative to @a context, if any.  (For
     *  example, if @a hdesc was "x.config" and @a context's name is
     *  "aaa/bbb/ccc", this will search for elements named aaa/bbb/x, aaa/x,
     *  and finally x.  If @a context is null, then the description must refer
     *  to a global handler.)  If f_read is set in @a flags, then there
     *  must be a read handler named appropriately; if f_write is set,
     *  then there must be a write handler.  If f_unquote_param is set, then any
     *  parameters are unquoted.
     *
     *  Initialization fails if the handler description was bogus (for
     *  example, an empty string, or something like "*#!$&!(#&$."), if the
     *  named handler does not exist, if a read handler description has
     *  parameters but the read handler doesn't actually take parameters, and
     *  so forth.  If @a errh is nonnull, errors are reported there.  The
     *  HandlerCall becomes empty on failure: empty() will return true, and
     *  (bool) *this will return false.  Future call_read() and call_write()
     *  attempts will correctly fail.
     *
     *  If the f_preinitialize flag is set, the initialize function will check
     *  whether the router's handlers are ready (Router::handlers_ready()).
     *  If handlers are not ready, then initialize() will check for syntax
     *  errors, but not actually look up the handler (since we don't know yet
     *  whether or not the handler exists).  Absent a syntax error,
     *  initialize() will return 0 for success even though the HandlerCall
     *  remains uninitialized. */
    int initialize(int flags, const Element *context, ErrorHandler *errh = 0);

    /** @brief  Initialize the HandlerCall for reading.
     *  @param  context  optional element context
     *  @param  errh     optional error handler
     *
     *  Equivalent to @link initialize(int, const Element*, ErrorHandler*)
     *  initialize@endlink(f_read, @a context, @a errh). */
    inline int initialize_read(const Element *context, ErrorHandler *errh = 0);

    /** @brief  Initialize the HandlerCall for writing.
     *  @param  context  optional element context
     *  @param  errh     optional error handler
     *
     *  Equivalent to @link initialize(int, const Element*, ErrorHandler*)
     *  initialize@endlink(f_write, @a context, @a errh). */
    inline int initialize_write(const Element *context, ErrorHandler *errh = 0);


    typedef bool (HandlerCall::*unspecified_bool_type)() const;

    /** @brief  Test if HandlerCall is empty.
     *  @return True if HandlerCall is not empty, false otherwise.
     *
     *  Valid HandlerCall objects have been successfully initialized. */
    operator unspecified_bool_type() const {
	return _h != Handler::blank_handler() || _e ? &HandlerCall::empty : 0;
    }

    /** @brief  Test if HandlerCall is empty.
     *  @return True if HandlerCall is empty, false otherwise. */
    bool empty() const {
	return _h == Handler::blank_handler() && !_e;
    }

    /** @brief  Test if HandlerCall is initialized.
     *  @return True if HandlerCall is initialized, false otherwise. */
    bool initialized() const {
	return _h != Handler::blank_handler();
    }


    /** @brief  Call a read handler.
     *  @param  errh  optional error handler
     *  @return  Read handler result.
     *
     *  Fails and returns the empty string if this HandlerCall is invalid or
     *  not a read handler.  If @a errh is nonnull, then any errors are
     *  reported there, whether from HandlerCall or the handler itself. */
    inline String call_read(ErrorHandler *errh = 0) const;

    /** @brief  Call a write handler.
     *  @param  errh  optional error handler
     *  @return  Write handler result.
     *
     *  Fails and returns -EINVAL if this HandlerCall is invalid or not a
     *  write handler.  If @a errh is nonnull, then any errors are reported
     *  there, whether from HandlerCall or the handler itself. */
    inline int call_write(ErrorHandler *errh = 0) const;

    /** @brief  Call a write handler, expanding its argument.
     *  @param  scope  variable scope
     *  @param  errh  optional error handler
     *  @return  Write handler result.
     *
     *  The write value is expanded in @a scope before the handler is called.
     *  Fails and returns -EINVAL if this HandlerCall is invalid or not a
     *  write handler.  If @a errh is nonnull, then any errors are reported
     *  there, whether from HandlerCall or the handler itself. */
    inline int call_write(const VariableExpander &scope, ErrorHandler *errh = 0) const;

    /** @brief  Call a write handler with an additional value.
     *  @param  value_ext  write value extension
     *  @param  errh       optional error handler
     *  @return  Write handler result.
     *
     *  The @a value_ext is appended to the write value before the handler is
     *  called.  (For example, consider a handler with description "a.set
     *  value".  call_write("foo") will call "a.set value foo".)  Fails and
     *  returns -EINVAL if this HandlerCall is invalid or not a write handler.
     *  If @a errh is nonnull, then any errors are reported there, whether
     *  from HandlerCall or the handler itself. */
    inline int call_write(const String &value_ext, ErrorHandler *errh = 0) const;


    /** @brief  Create and initialize a HandlerCall from @a hdesc.
     *  @param  hcall    stores the HandlerCall result
     *  @param  hdesc    handler description "[ename.]hname[ value]"
     *  @param  flags    initialization flags (f_read, f_write, f_preinitialize)
     *  @param  context  optional element context
     *  @param  errh     optional error handler
     *  @return  0 on success, -EINVAL on failure
     *
     *  Creates a HandlerCall and initializes it.  Behaves somewhat like:
     *
     *  @code
     *  hcall = new HandlerCall(hdesc);
     *  return hcall->initialize(flags, context, errh);
     *  @endcode
     *
     *  However, (1) if initialization fails, then @a hcall is untouched; and
     *  (2) if initialization succeeds and @a hcall is not null, then the
     *  existing HandlerCall is assigned so that it corresponds to the new one
     *  (no new memory allocations).
     *
     *  If @a errh is nonnull, then any errors are reported there. */
    static int reset(HandlerCall *&hcall, const String &hdesc, int flags,
		     const Element *context, ErrorHandler *errh = 0);

    /** @brief  Create and initialize a HandlerCall on element @a e.
     *  @param  hcall  stores the HandlerCall result
     *  @param  e      relevant element, if any
     *  @param  hname  handler name
     *  @param  value  handler value
     *  @param  flags  initialization flags (f_read, f_write, f_preinitialize)
     *  @param  errh   optional error handler
     *  @return  0 on success, -EINVAL on failure
     *
     *  Creates a HandlerCall and initializes it.  Behaves analogously to
     *  reset(HandlerCall*&, const String&, int, const Element*, ErrorHandler*). */
    static int reset(HandlerCall *&hcall,
		     Element *e, const String &hname, const String &value,
		     int flags, ErrorHandler *errh = 0);


    /** @brief  Create and initialize a read HandlerCall from @a hdesc.
     *  @param  hcall    stores the HandlerCall result
     *  @param  hdesc    handler description "[ename.]hdesc[ param]"
     *  @param  context  optional element context
     *  @param  errh     optional error handler
     *  @return  0 on success, -EINVAL on failure
     *
     *  Equivalent to
     *  @link reset(HandlerCall*&, const String&, int, const Element*, ErrorHandler*) reset@endlink(@a hcall, @a hdesc, f_read, @a context, @a errh). */
    static inline int reset_read(HandlerCall *&hcall, const String &hdesc,
				 const Element *context, ErrorHandler *errh = 0);

    /** @brief  Create and initialize a read HandlerCall from @a hdesc.
     *  @param  hcall  stores the HandlerCall result
     *  @param  e      relevant element, if any
     *  @param  hname  handler name
     *  @param  errh   optional error handler
     *  @return  0 on success, -EINVAL on failure
     *
     *  Equivalent to
     *  @link reset(HandlerCall*&, Element*, const String&, const String&, int, ErrorHandler*) reset@endlink(@a hcall, @a e, @a hname, String(), f_read, @a context, @a errh). */
    static inline int reset_read(HandlerCall *&hcall,
				 Element *e, const String &hname,
				 ErrorHandler *errh = 0);

    /** @brief  Create and initialize a write HandlerCall from @a hdesc.
     *  @param  hcall    stores the HandlerCall result
     *  @param  hdesc    handler description "[ename.]hdesc[ value]"
     *  @param  context  optional element context
     *  @param  errh     optional error handler
     *  @return  0 on success, -EINVAL on failure
     *
     *  Equivalent to
     *  @link reset(HandlerCall*&, const String&, int, const Element*, ErrorHandler*) reset@endlink(@a hcall, @a hdesc, f_write, @a context, @a errh). */
    static inline int reset_write(HandlerCall *&hcall, const String &hdesc,
				  const Element *context, ErrorHandler *errh = 0);

    /** @brief  Create and initialize a read HandlerCall from @a hdesc.
     *  @param  hcall  stores the HandlerCall result
     *  @param  e      relevant element, if any
     *  @param  hname  handler name
     *  @param  value  write handler value
     *  @param  errh   optional error handler
     *  @return  0 on success, -EINVAL on failure
     *
     *  Equivalent to
     *  @link reset(HandlerCall*&, Element*, const String&, const String&, int, ErrorHandler*) reset@endlink(@a hcall, @a e, @a hname, @ value, f_write, @a context, @a errh). */
    static inline int reset_write(HandlerCall *&hcall,
				  Element *e, const String &hname,
				  const String &value = String(),
				  ErrorHandler *errh = 0);


    /** @brief  Return the Element corresponding to this HandlerCall.
     *
     *  Returns null if invalid.  A global handler may return some
     *  Router::root_element() or null. */
    Element *element() const {
	return _e;
    }

    /** @brief  Return the Handler corresponding to this HandlerCall.
     *
     *  Returns Handler::blank_handler() if invalid. */
    const Handler *handler() const {
	return _h;
    }

    /** @brief  Return the write handler value and/or read handler parameters.
     *
     *  Returns the empty string if invalid. */
    const String &value() const	{
	return initialized() ? _value : String::make_empty();
    }

    /** @brief  Sets the write handler value and/or read handler parameters.
     *  @param  value  new value and/or parameters
     *
     *  Does nothing if invalid. */
    void set_value(const String &value) {
	if (initialized())
	    _value = value;
    }

    /** @brief  Return a String that will parse into an equivalent HandlerCall.
     *
     *  Will work even if the HandlerCall has not been initialized. */
    String unparse() const;

    /** @brief  Make this HandlerCall empty.
     *
     *  Subsequent attempts to read, write, or initialize the HandlerCall will
     *  fail. */
    void clear() {
	_e = 0;
	_h = Handler::blank_handler();
	_value = String();
    }


    /** @cond never */
    enum {
        CHECK_READ = f_read, OP_READ = f_read, h_read = f_read,
        CHECK_WRITE = f_write, OP_WRITE = f_write, h_write = f_write,
        PREINITIALIZE = f_preinitialize, h_preinitialize = f_preinitialize,
        UNQUOTE_PARAM = f_unquote_param, h_unquote_param = f_unquote_param
    };
    /** @endcond never */

  private:

    Element *_e;
    const Handler *_h;
    String _value;

    int parse(int flags, Element*, ErrorHandler*);
    int assign(Element*, const String&, const String&, int flags, ErrorHandler*);

};

inline int
HandlerCall::reset_read(HandlerCall*& hcall, const String& hdesc, const Element* context, ErrorHandler* errh)
{
    return reset(hcall, hdesc, f_read, context, errh);
}

inline int
HandlerCall::reset_write(HandlerCall*& hcall, const String& hdesc, const Element* context, ErrorHandler* errh)
{
    return reset(hcall, hdesc, f_write, context, errh);
}

inline int
HandlerCall::reset_read(HandlerCall*& hcall, Element* e, const String& hname, ErrorHandler* errh)
{
    return reset(hcall, e, hname, String(), f_read, errh);
}

inline int
HandlerCall::reset_write(HandlerCall*& hcall, Element* e, const String& hname, const String& value, ErrorHandler* errh)
{
    return reset(hcall, e, hname, value, f_write, errh);
}

inline int
HandlerCall::initialize_read(const Element* context, ErrorHandler* errh)
{
    return initialize(f_read, context, errh);
}

inline int
HandlerCall::initialize_write(const Element* context, ErrorHandler* errh)
{
    return initialize(f_write, context, errh);
}

inline String
HandlerCall::call_read(ErrorHandler *errh) const
{
    return _h->call_read(_e, _value, errh);
}

inline int
HandlerCall::call_write(ErrorHandler *errh) const
{
    return _h->call_write(_value, _e, errh);
}

String cp_expand(const String &str, const VariableExpander &env, bool expand_quote, int depth);

inline int
HandlerCall::call_write(const VariableExpander &scope, ErrorHandler *errh) const
{
    return _h->call_write(cp_expand(_value, scope, false, 0), _e, errh);
}

inline int
HandlerCall::call_write(const String &value_ext, ErrorHandler *errh) const
{
    if (_value && value_ext)
	return _h->call_write(_value + " " + value_ext, _e, errh);
    else
	return _h->call_write(_value ? _value : value_ext, _e, errh);
}

/** @brief  Call a write handler specified by element and handler name.
 *  @param  e      relevant element, if any
 *  @param  hname  handler name
 *  @param  errh   optional error handler
 *  @return  handler result, or -EINVAL on error
 *
 *  Searches for a write handler named @a hname on element @a e.  If the
 *  handler exists, calls it (with empty write value) and returns the result.
 *  If @a errh is nonnull, then errors, such as a missing handler or a
 *  read-only handler, are reported there.  If @a e is some router's @link
 *  Router::root_element() root element@endlink, calls the global write
 *  handler named @a hname on that router. */
inline int
HandlerCall::call_write(Element *e, const String &hname, ErrorHandler *errh)
{
    return call_write(e, hname, String(), errh);
}


/** @class HandlerCallArg
  @brief Parser class for handler call specifications.

  The constructor argument should generally be either HandlerCall::writable or
  HandlerCall::readable.  For example:

  @code
  HandlerCall end_h;
  ... Args(...) ...
     .read("END_CALL", HandlerCallArg(HandlerCall::writable), end_h)
     ...
  @endcode */
struct HandlerCallArg {
    HandlerCallArg(int f)
	: flags(f) {
    }
    bool parse(const String &str, HandlerCall &result, const ArgContext &args);
    int flags;
};

CLICK_ENDDECLS
#endif
