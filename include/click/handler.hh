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
	COMPREHENSIVE = 0x0008,	///< @brief Use comprehensive hook for all
				///  operations.
	SPECIAL_FLAGS = OP_READ | OP_WRITE | READ_PARAM | COMPREHENSIVE,
				///< @brief These flags may not be set by
				///  Router::set_handler_flags().
	EXCLUSIVE = 0,		///< @brief Handler is exclusive (the default):
				///  router threads must stop while it is
				///  called.
	NONEXCLUSIVE = 0x0010,	///< @brief Handler is nonexclusive: router
				///  threads don't need to stop while it is
				///  called.
	RAW = 0x0020,		///< @brief Don't add newline to results.
	READ_PRIVATE = 0x0040,	///< @brief Read handler private (invisible
				///  outside the router configuration).
	WRITE_PRIVATE = 0x0080,	///< @brief Write handler private (invisible
				///  outside the router configuration).
	DEPRECATED = 0x0100,	///< @brief Handler is deprecated and available
				///  only for compatibility.
	UNCOMMON = 0x0200,	///< @brief User interfaces should not display
				///  handler by default.
	CALM = 0x0400,		///< @brief Read handler value changes rarely.
	EXPENSIVE = 0x0800,	///< @brief Read handler is expensive to call.
	BUTTON = 0x1000,	///< @brief Write handler ignores data.
	CHECKBOX = 0x2000,	///< @brief Read/write handler is boolean and
				///  should be rendered as a checkbox.
	DRIVER_FLAG_SHIFT = 20,
	DRIVER_FLAG_0 = 1 << DRIVER_FLAG_SHIFT,
				///< @brief First uninterpreted handler flag
				///  available for drivers.  Equals 1 <<
				///  DRIVER_FLAG_SHIFT.
	USER_FLAG_SHIFT = 25,
	USER_FLAG_0 = 1 << USER_FLAG_SHIFT
				///< @brief First uninterpreted handler flag
				///  available for element-specific use.
				///  Equals 1 << USER_FLAG_SHIFT.
    };

    /** @brief Return this handler's name. */
    inline const String &name() const {
	return _name;
    }

    /** @brief Return this handler's flags.
     *
     * The result is a bitwise-or of flags from the Flags enumeration type. */
    inline uint32_t flags() const {
	return _flags;
    }
    
    /** @brief Return this handler's first callback data. */
    inline void *user_data1() const {
	return _thunk1;
    }

    /** @brief Return this handler's second callback data. */
    inline void *user_data2() const {
	return _thunk2;
    }


    /** @brief Check if this is a valid read handler. */
    inline bool readable() const {
	return _flags & OP_READ;
    }

    /** @brief Check if this is a valid read handler that may accept
     * parameters. */
    inline bool read_param() const {
	return _flags & READ_PARAM;
    }

    /** @brief Check if this is a public read handler.
     *
     * Private handlers may be not called from outside the router
     * configuration.  Handlers are public by default; to make a read handler
     * private, add the READ_PRIVATE flag. */
    inline bool read_visible() const {
	return (_flags & (OP_READ | READ_PRIVATE)) == OP_READ;
    }

    /** @brief Check if this is a valid write handler. */
    inline bool writable() const {
	return _flags & OP_WRITE;
    }

    /** @brief Check if this is a public write handler.
     *
     * Private handlers may not be called from outside the router
     * configuration.  Handlers are public by default; to make a write handler
     * private, add the WRITE_PRIVATE flag. */
    inline bool write_visible() const {
	return (_flags & (OP_WRITE | WRITE_PRIVATE)) == OP_WRITE;
    }

    /** @brief Check if this is a public read or write handler. */
    inline bool visible() const {
	return read_visible() || write_visible();
    }

    /** @brief Check if this handler is exclusive.
     *
     * Exclusive handlers are mutually exclusive with all other router
     * processing.  In the Linux kernel module driver, reading or writing an
     * exclusive handler using the Click filesystem will first lock all router
     * threads and handlers.  Handlers are exclusive by default.  Exclusivity
     * is cleared by the NONEXCLUSIVE flag.  */
    inline bool exclusive() const {
	return !(_flags & NONEXCLUSIVE);
    }

    /** @brief Check if spaces should be preserved when calling this handler.
     *
     * Some Click drivers perform some convenience processing on handler
     * values, for example by removing a terminating newline from write
     * handler values or adding a terminating newline to read handler values.
     * Raw handlers do not have their values manipulated in this way.  Rawness
     * is set by the RAW flag.
     *
     * <ul>
     * <li>The linuxmodule driver adds a terminating newline to non-raw read
     * handler values, but does not modify raw read handlers' values in any
     * way.</li>
     * <li>The same applies to handler values returned by the userlevel
     * driver's <tt>-h</tt> option.</li>
     * <li>The linuxmodule driver removes an optional terminating newline from
     * a one-line non-raw write handler value, but does not modify raw write
     * handlers' values in any way.</li>
     * </ul> */
    inline bool raw() const {
	return _flags & RAW;
    }


    /** @brief Call a read handler, possibly with parameters.
     * @param e element on which to call the handler
     * @param param parameters, or an empty string if no parameters
     * @param errh optional error handler
     *
     * The element must be nonnull; to call a global handler, pass the
     * relevant router's Router::root_element().  @a errh may be null, in
     * which case errors are reported to ErrorHandler::silent_handler(). */
    String call_read(Element *e, const String &param, ErrorHandler *errh) const;

    /** @brief Call a read handler, possibly with parameters.
     * @param e element on which to call the handler
     * @param param parameters, or an empty string if no parameters
     * @param raw true iff param is raw text (see raw())
     * @param errh optional error handler
     * @deprecated The @a raw argument is deprecated and ignored.
     *
     * The element must be nonnull; to call a global handler, pass the
     * relevant router's Router::root_element().  @a errh may be null, in
     * which case errors are reported to ErrorHandler::silent_handler(). */
    inline String call_read(Element *e, const String &param, bool raw,
			    ErrorHandler *errh) const CLICK_DEPRECATED;

    /** @brief Call a read handler without parameters.
     * @param e element on which to call the handler
     * @param errh error handler
     *
     * The element must be nonnull; to call a global handler, pass the
     * relevant router's Router::root_element().  @a errh may be null, in
     * which case errors are ignored. */
    inline String call_read(Element *e, ErrorHandler *errh = 0) const {
	return call_read(e, String(), errh);
    }

    /** @brief Call a write handler.
     * @param value value to write to the handler
     * @param e element on which to call the handler
     * @param errh optional error handler
     *
     * The element must be nonnull; to call a global handler, pass the
     * relevant router's Router::root_element().  @a errh may be null, in
     * which case errors are reported to ErrorHandler::silent_handler(). */
    int call_write(const String &value, Element *e, ErrorHandler *errh) const;

    /** @brief Call a write handler.
     * @param value value to write to the handler
     * @param e element on which to call the handler
     * @param raw true iff value is raw text (see raw())
     * @param errh optional error handler
     * @deprecated The @a raw argument is deprecated and ignored.
     *
     * The element must be nonnull; to call a global handler, pass the
     * relevant router's Router::root_element().  @a errh may be null, in
     * which case errors are reported to ErrorHandler::silent_handler(). */
    inline int call_write(const String &value, Element *e, bool raw,
			  ErrorHandler *errh) const CLICK_DEPRECATED;
  

    /** @brief Unparse this handler's name.
     * @param e relevant element
     *
     * If @a e is an actual element, then returns "ENAME.HNAME", where ENAME
     * is @a e's @link Element::name() name@endlink and HNAME is this
     * handler's name().  Otherwise, just returns name(). */
    String unparse_name(Element *e) const;

    /** @brief Unparse a handler name.
     * @param e relevant element, if any
     * @param hname handler name
     *
     * If @a e is an actual element on some router, then returns
     * "ENAME.hname", where ENAME is @a e's @link Element::name()
     * name@endlink.  Otherwise, just returns @a hname.*/
    static String unparse_name(Element *e, const String &hname);


    /** @brief Returns a handler incapable of doing anything.
     *
     *  The returned handler returns false for readable() and writable() and
     *  has flags() of zero. */
    static inline const Handler *blank_handler() {
	return the_blank_handler;
    }

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

inline String
Handler::call_read(Element *e, const String &param, bool,
		   ErrorHandler *errh) const
{
    return call_read(e, param, errh);
}

inline int
Handler::call_write(const String &value, Element *e, bool,
		    ErrorHandler *errh) const
{
    return call_write(value, e, errh);
}

CLICK_ENDDECLS
#endif

