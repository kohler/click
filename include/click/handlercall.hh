// -*- c-basic-offset: 4; related-file-name: "../../lib/handlercall.cc" -*-
#ifndef CLICK_HANDLERCALL_HH
#define CLICK_HANDLERCALL_HH
#include <click/element.hh>
CLICK_DECLS

class HandlerCall { public:

    // Create a HandlerCall. You generally don't need to do this explicitly;
    // see the 'reset_read' and 'reset_write' methods below.
    HandlerCall()		: _e(0), _hi(-1) { }
    HandlerCall(const String &s): _e((Element *)(-1)), _hi(-1), _value(s) { }

    // Return true iff the HandlerCall is valid.
    bool ok() const		{ return _hi >= 0; }

    // Return true iff the HandlerCall has been initialized.
    bool initialized() const	{ return _e != (Element *)(-1); }

    // Return true iff the HandlerCall represents a read handler.
    bool is_read() const;

    // Return a String that will parse into an equivalent HandlerCall.
    String unparse() const;

    // Call this handler and return its result. Returns the empty string or
    // negative if the HandlerCall isn't ok().
    String call_read() const;
    int call_write(ErrorHandler * = 0) const;

    // Call the specified handler and return its result. Returns the empty
    // string or negative if the handler isn't valid.
    static String call_read(Element *, const String &hname, ErrorHandler * = 0);
    static int call_write(Element *, const String &hname, const String &value = String(), ErrorHandler * = 0);
    
    // Call the described handler and return its result. Returns the empty
    // string or negative if the handler isn't valid.
    static String call_read(const String &hdesc, Router *, ErrorHandler * = 0);
    static int call_write(const String &hdesc, Router *, ErrorHandler * = 0);
    
    // Replace 'hcall' with a handler call parsed from 'hdesc'. A new
    // HandlerCall may be allocated if 'hcall' is null. 'hcall' is not changed
    // unless 'hdesc' is valid. Returns 0 if valid, negative if not.
    static int reset_read(HandlerCall *&hcall, const String &hdesc, Element *context, ErrorHandler * = 0);
    static int reset_write(HandlerCall *&hcall, const String &hdesc, Element *context, ErrorHandler * = 0);

    // Replace 'hcall' with a handler call obtained from 'e', 'hname', and
    // possibly 'value'. A new HandlerCall may be allocated if 'hcall' is
    // null. 'hcall' is not changed unless the specified handler is valid.
    // Returns 0 if valid, negative if not.
    static int reset_read(HandlerCall *&hcall, Element *e, const String &hname, ErrorHandler * = 0);
    static int reset_write(HandlerCall *&hcall, Element *e, const String &hname, const String &value = String(), ErrorHandler * = 0);

    // Initialize a handler call once handler information is available.
    // Returns 0 if valid, negative if not.
    int initialize_read(Element *, ErrorHandler * = 0);
    int initialize_write(Element *, ErrorHandler * = 0);

    // Less-used functions.
    void clear()		{ _e = 0; _hi = -1; _value = String(); }
    void reset(const String &s)	{ _e = (Element *)(-1); _hi = -1; _value = s; }
    static int reset(HandlerCall *&, const String &hdesc, bool write, Element *, ErrorHandler * = 0);
    static int reset(HandlerCall *&, Element *, const String &hname, const String &value, bool write, ErrorHandler * = 0);
    int parse(const String &, bool write, Element *, ErrorHandler * = 0);

  private:
    
    static const char * const READ_MARKER;
    
    Element *_e;
    int _hi;
    String _value;

    inline void assign(Element *, int, const String &, bool write);

};

inline bool
HandlerCall::is_read() const
{
    return _value.data() == READ_MARKER;
}

inline int
HandlerCall::reset_read(HandlerCall *&hcall, const String &hdesc, Element *context, ErrorHandler *errh)
{
    return reset(hcall, hdesc, false, context, errh);
}

inline int
HandlerCall::reset_write(HandlerCall *&hcall, const String &hdesc, Element *context, ErrorHandler *errh)
{
    return reset(hcall, hdesc, true, context, errh);
}

inline int
HandlerCall::reset_read(HandlerCall *&hcall, Element *e, const String &hname, ErrorHandler *errh)
{
    return reset(hcall, e, hname, String(), false, errh);
}

inline int
HandlerCall::reset_write(HandlerCall *&hcall, Element *e, const String &hname, const String &value, ErrorHandler *errh)
{
    return reset(hcall, e, hname, value, true, errh);
}

inline int
HandlerCall::initialize_read(Element *context, ErrorHandler *errh)
{
    if (!initialized())
	parse(_value, false, context, errh);
    return (ok() ? 0 : -1);
}

inline int
HandlerCall::initialize_write(Element *context, ErrorHandler *errh)
{
    if (!initialized())
	parse(_value, true, context, errh);
    return (ok() ? 0 : -1);
}

CLICK_ENDDECLS
#endif
