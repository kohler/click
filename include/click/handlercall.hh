// -*- c-basic-offset: 4; related-file-name: "../../lib/handlercall.cc" -*-
#ifndef CLICK_HANDLERCALL_HH
#define CLICK_HANDLERCALL_HH
#include <click/router.hh>
CLICK_DECLS

class HandlerCall { public:

    // Create a HandlerCall. You generally don't need to do this explicitly;
    // see the 'reset_read' and 'reset_write' methods below.
    HandlerCall()		: _e(0), _h(Handler::blank_handler()) { }
    HandlerCall(const String& s): _e((Element*)(-1)), _h(Handler::blank_handler()), _value(s) { }

    // Return true iff the HandlerCall is valid.
    bool ok() const		{ return _h != Handler::blank_handler(); }
    operator bool() const	{ return _h != Handler::blank_handler(); }

    // Return true iff the HandlerCall has been initialized.
    bool initialized() const	{ return _e != (Element*)(-1); }

    // Return a String that will parse into an equivalent HandlerCall.
    String unparse() const;

    // Return the element on which to call this handler.
    Element* element() const	{ return _e; }
    
    // Return the handler to call.
    const Handler* handler() const { return _h; }
    
    // Return the value to be sent to a write handler.
    const String& value() const	{ assert(initialized()); return _value; }

    // Change the value to be sent to a write handler.
    void set_value(const String& v) { assert(initialized()); _value = v; }

    // Call this handler and return its result. Returns the empty string or
    // negative if the HandlerCall isn't ok().
    inline String call_read() const;
    inline int call_write(ErrorHandler* = 0) const;

    // Call the specified handler and return its result. Returns the empty
    // string or negative if the handler isn't valid.
    static String call_read(Element*, const String& hname, ErrorHandler* = 0);
    static String call_read(const String& hdesc, Router*, ErrorHandler* = 0);
    static int call_write(Element*, const String& hname, const String& value = String(), ErrorHandler* = 0);
    static int call_write(const String& hdesc_with_value, Router*, ErrorHandler* = 0);
    static int call_write(const String& hdesc, const String& value, Router*, ErrorHandler* = 0);
    
    // Replace 'hcall' with a handler call parsed from 'hdesc'. A new
    // HandlerCall may be allocated if 'hcall' is null. 'hcall' is not changed
    // unless 'hdesc' is valid. Returns 0 if valid, negative if not.
    static inline int reset_read(HandlerCall*& hcall, const String& hdesc, Element* context, ErrorHandler* = 0);
    static inline int reset_write(HandlerCall*& hcall, const String& hdesc, Element* context, ErrorHandler* = 0);

    // Replace 'hcall' with a handler call obtained from 'e', 'hname', and
    // possibly 'value'. A new HandlerCall may be allocated if 'hcall' is
    // null. 'hcall' is not changed unless the specified handler is valid.
    // Returns 0 if valid, negative if not.
    static inline int reset_read(HandlerCall*& hcall, Element* e, const String& hname, ErrorHandler* = 0);
    static inline int reset_write(HandlerCall*& hcall, Element* e, const String& hname, const String& value = String(), ErrorHandler* = 0);

    // Initialize a handler call once handler information is available.
    // Returns 0 if valid, negative if not.
    enum Flags {
	CHECK_READ = 1, CHECK_WRITE = 2, ALLOW_VALUE = 4,
	ALLOW_PREINITIALIZE = 8
    };
    int initialize(int flags, Element*, ErrorHandler* = 0);
    inline int initialize_read(Element*, ErrorHandler* = 0);
    inline int initialize_write(Element*, ErrorHandler* = 0);

    // Less-used functions.
    void clear()		{ _e = 0; _h = Handler::blank_handler(); _value = String(); }
    static int reset(HandlerCall*&, const String& hdesc, int flags, Element*, ErrorHandler* = 0);
    static int reset(HandlerCall*&, Element*, const String& hname, const String& value, int flags, ErrorHandler* = 0);

  private:
    
    Element* _e;
    const Handler* _h;
    String _value;

    int parse(int flags, Element*, ErrorHandler*);
    int assign(Element*, const String&, const String&, int flags, ErrorHandler*);

};

inline int
HandlerCall::reset_read(HandlerCall*& hcall, const String& hdesc, Element* context, ErrorHandler* errh)
{
    return reset(hcall, hdesc, CHECK_READ, context, errh);
}

inline int
HandlerCall::reset_write(HandlerCall*& hcall, const String& hdesc, Element* context, ErrorHandler* errh)
{
    return reset(hcall, hdesc, CHECK_WRITE | ALLOW_VALUE, context, errh);
}

inline int
HandlerCall::reset_read(HandlerCall*& hcall, Element* e, const String& hname, ErrorHandler* errh)
{
    return reset(hcall, e, hname, String(), CHECK_READ, errh);
}

inline int
HandlerCall::reset_write(HandlerCall*& hcall, Element* e, const String& hname, const String& value, ErrorHandler* errh)
{
    return reset(hcall, e, hname, value, CHECK_WRITE | ALLOW_VALUE, errh);
}

inline int
HandlerCall::initialize_read(Element* context, ErrorHandler* errh)
{
    return initialize(CHECK_READ, context, errh);
}

inline int
HandlerCall::initialize_write(Element* context, ErrorHandler* errh)
{
    return initialize(CHECK_WRITE | ALLOW_VALUE, context, errh);
}

inline String
HandlerCall::call_read() const
{
    return _h->call_read(_e);
}

inline int
HandlerCall::call_write(ErrorHandler *errh) const
{
    return _h->call_write(_value, _e, errh);
}

CLICK_ENDDECLS
#endif
