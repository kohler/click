#ifndef CLICK_HANDLER_HH
#define CLICK_HANDLER_HH 1
#include <click/string.hh>
CLICK_DECLS
class Element;
class ErrorHandler;
class Handler;

/** @file <click/handler.hh>
 * @brief The Handler class for router handlers.
 */

typedef int (*HandlerHook)(int operation, String &data, Element *element,
			   const Handler *handler, ErrorHandler *errh);
typedef String (*ReadHandlerHook)(Element *handler, void *user_data);
typedef int (*WriteHandlerHook)(const String &data, Element *element,
				void *user_data, ErrorHandler *errh);

class Handler { public:

    enum Flags {
	OP_READ = 0x0001,	///< @brief Handler supports read operations.
	OP_WRITE = 0x0002,	///< @brief Handler supports write operations.
	READ_PARAM = 0x0004,	///< @brief Read handler takes parameters.
	UNIFORM = 0x0008,	///< @brief Use uniform hook for all operations.
	SPECIAL_FLAGS = OP_READ | OP_WRITE | READ_PARAM | UNIFORM,
				///< @brief These flags may not be set by
				///  Router::set_handler_flags().
	EXCLUSIVE = 0x0010,	///< @brief Handler is exclusive.
	RAW = 0x0020,		///< @brief Don't add newline to results.
	CALM = 0x0040,		///< @brief Read handler value changes rarely.
	EXPENSIVE = 0x0080,	///< @brief Read handler is expensive to call.
	BUTTON = 0x0100,	///< @brief Write handler ignores data.
	CHECKBOX = 0x0200,	///< @brief Read/write handler is boolean and
				///  should be rendered as a checkbox.
	DRIVER_FLAG_0 = 0x0400,	///< @brief Handler flag available for drivers.
	DRIVER_FLAG_1 = 0x0800,	///< @brief Handler flag available for drivers.
	DRIVER_FLAG_2 = 0x1000,	///< @brief Handler flag available for drivers.
	DRIVER_FLAG_3 = 0x2000,	///< @brief Handler flag available for drivers.
	USER_FLAG_SHIFT = 14,
	USER_FLAG_0 = 1 << USER_FLAG_SHIFT
				///< @brief First uninterpreted handler flag
				///  available for element-specific use.
				///  Equals 1 << USER_FLAG_SHIFT.
    };

    inline const String &name() const;
    inline uint32_t flags() const;
    inline void *user_data1() const;
    inline void *user_data2() const;

    inline bool readable() const;
    inline bool read_param() const;
    inline bool read_visible() const;
    inline bool writable() const;
    inline bool write_visible() const;
    inline bool visible() const;
    inline bool exclusive() const;
    inline bool raw() const;

    inline String call_read(Element *e, ErrorHandler *errh = 0) const;
    String call_read(Element *e, const String &param, bool raw,
		     ErrorHandler *errh) const;
    int call_write(const String &value, Element *e, bool raw,
		   ErrorHandler *errh) const;
  
    String unparse_name(Element *e) const;
    static String unparse_name(Element *e, const String &hname);

    static inline const Handler *blank_handler();

  private:

    String _name;
    union {
	HandlerHook h;
	struct {
	    ReadHandlerHook r;
	    WriteHandlerHook w;
	} rw;
    } _hook;
    void *_thunk1;
    void *_thunk2;
    uint32_t _flags;
    int _use_count;
    int _next_by_name;

    static const Handler *the_blank_handler;
    
    Handler(const String & = String());

    bool compatible(const Handler &) const;
  
    friend class Router;
  
};

/* The largest size a write handler is allowed to have. */
#define LARGEST_HANDLER_WRITE 65536


/** @brief Returns this handler's name. */
inline const String&
Handler::name() const
{
    return _name;
}

/** @brief Returns this handler's flags.

    The result is a bitwise-or of flags from the Flags enumeration type. */
inline uint32_t
Handler::flags() const
{
    return _flags;
}

/** @brief Returns this handler's first callback data. */
inline void*
Handler::user_data1() const
{
    return _thunk1;
}

/** @brief Returns this handler's second callback data. */
inline void*
Handler::user_data2() const
{
    return _thunk2;
}

/** @brief Returns true iff this is a valid read handler. */
inline bool
Handler::readable() const
{
    return _flags & OP_READ;
}

/** @brief Returns true iff this is a valid read handler that may accept
    parameters. */
inline bool
Handler::read_param() const
{
    return _flags & READ_PARAM;
}

/** @brief Returns true iff this is a valid visible read handler.

    Only visible handlers may be called from outside the router
    configuration. */
inline bool
Handler::read_visible() const
{
    return _flags & OP_READ;
}

/** @brief Returns true iff this is a valid write handler. */
inline bool
Handler::writable() const
{
    return _flags & OP_WRITE;
}

/** @brief Returns true iff this is a valid visible write handler.

    Only visible handlers may be called from outside the router
    configuration. */
inline bool
Handler::write_visible() const
{
    return _flags & OP_WRITE;
}

/** @brief Returns true iff this handler is visible. */
inline bool
Handler::visible() const
{
    return _flags & (OP_READ | OP_WRITE);
}

/** @brief Returns true iff this handler is exclusive.

    Exclusive means mutually exclusive with all other router processing.  In
    the Linux kernel module driver, reading or writing an exclusive handler
    using the Click filesystem will first lock all router threads and
    handlers.  Exclusivity is set by the EXCLUSIVE flag.  */
inline bool
Handler::exclusive() const
{
    return _flags & EXCLUSIVE;
}

/** @brief Returns true iff quotes should be removed when calling this
    handler.

    A raw handler expects and returns raw text.  Click will unquote quoted
    text before passing it to a raw handler, and (in the Linux kernel module)
    will not add a courtesy newline to the end of a raw handler's value.
    Rawness is set by the RAW flag. */
inline bool
Handler::raw() const
{
    return _flags & RAW;
}

/** @brief Call a read handler without parameters.
    @param e element on which to call the handler
    @param errh error handler

    The element must be nonnull; to call a global handler, pass the relevant
    router's Router::root_element().  @a errh may be null, in which case
    errors are ignored. */
inline String
Handler::call_read(Element* e, ErrorHandler* errh) const
{
    return call_read(e, String(), false, errh);
}

/** @brief Returns a handler incapable of doing anything.
 *
 *  The returned handler returns false for readable() and writable()
 *  and has flags() of zero. */
inline const Handler *
Handler::blank_handler()
{
    return the_blank_handler;
}

CLICK_ENDDECLS
#endif

