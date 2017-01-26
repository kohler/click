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

typedef int (*HandlerCallback)(int operation, String &data, Element *element,
			       const Handler *handler, ErrorHandler *errh);
typedef String (*ReadHandlerCallback)(Element *handler, void *user_data);
typedef int (*WriteHandlerCallback)(const String &data, Element *element,
				    void *user_data, ErrorHandler *errh);

class Handler { public:

    enum Flags {
	f_read = 0x0001,	///< @brief Handler supports read operations.
	f_write = 0x0002,	///< @brief Handler supports write operations.
	f_read_param = 0x0004,	///< @brief Read handler takes parameters.
	f_exclusive = 0,	///< @brief Handler is exclusive (the default):
				///  router threads must stop while it is
				///  called.
	f_nonexclusive = 0x0020,///< @brief Handler is nonexclusive: router
				///  threads don't need to stop while it is
				///  called.
	f_raw = 0x0040,		///< @brief Don't add newline to results.
	f_read_private = 0x0080,///< @brief Read handler private (invisible
				///  outside the router configuration).
	f_write_private = 0x0100,///< @brief Write handler private (invisible
				///  outside the router configuration).
	f_deprecated = 0x0200,	///< @brief Handler is deprecated and available
				///  only for compatibility.
	f_uncommon = 0x0400,	///< @brief User interfaces should not display
				///  handler by default.
	f_calm = 0x0800,	///< @brief Read handler value changes rarely.
	f_expensive = 0x1000,	///< @brief Read handler is expensive to call.
	f_button = 0x2000,	///< @brief Write handler ignores data.
	f_checkbox = 0x4000,	///< @brief Read/write handler is boolean and
				///  should be rendered as a checkbox.
	f_driver0 = 1U << 26,
        f_driver1 = 1U << 27,   ///< @brief Uninterpreted handler flags
				///  available for drivers.
	f_user_shift = 28,
	f_user0 = 1U << f_user_shift,
				///< @brief First uninterpreted handler flag
				///  available for element-specific use.
				///  Equals 1 << f_user_shift.

	f_read_comprehensive = 0x0008,
	f_write_comprehensive = 0x0010,
	f_special = f_read | f_write | f_read_param | f_read_comprehensive | f_write_comprehensive
				///< @brief These flags may not be set by
				///  Router::set_handler_flags().
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

    /** @brief Return this handler's callback data.
     * @param op either f_read or f_write. */
    inline void *user_data(int op) const {
	return op == f_write ? _write_user_data : _read_user_data;
    }

    /** @brief Return this handler's read callback data. */
    inline void *read_user_data() const {
	return _read_user_data;
    }

    /** @brief Return this handler's write callback data. */
    inline void *write_user_data() const {
	return _write_user_data;
    }

    /** @cond never */
    inline void *user_data1() const CLICK_DEPRECATED;
    inline void *user_data2() const CLICK_DEPRECATED;
    /** @endcond never */


    /** @brief Test if this is a valid read handler. */
    inline bool readable() const {
	return _flags & f_read;
    }

    /** @brief Test if this is a valid read handler that may accept
     * parameters. */
    inline bool read_param() const {
	return _flags & f_read_param;
    }

    /** @brief Test if this is a public read handler.
     *
     * Private handlers may be not called from outside the router
     * configuration.  Handlers are public by default; to make a read handler
     * private, add the f_read_private flag. */
    inline bool read_visible() const {
	return (_flags & (f_read | f_read_private)) == f_read;
    }

    /** @brief Test if this is a valid write handler. */
    inline bool writable() const {
	return _flags & f_write;
    }

    /** @brief Test if this is a public write handler.
     *
     * Private handlers may not be called from outside the router
     * configuration.  Handlers are public by default; to make a write handler
     * private, add the f_write_private flag. */
    inline bool write_visible() const {
	return (_flags & (f_write | f_write_private)) == f_write;
    }

    /** @brief Test if this is a public read or write handler. */
    inline bool visible() const {
	return read_visible() || write_visible();
    }

    /** @brief Test if this handler can execute concurrently with other
     *         handlers. */
    inline bool allow_concurrent_handlers() const {
        return (_flags & f_nonexclusive);
    }

    /** @brief Test if this handler can execute concurrently with
     *         router threads. */
    inline bool allow_concurrent_threads() const {
        return (_flags & f_nonexclusive);
    }

    /** @brief Test if spaces should be preserved when calling this handler.
     *
     * Some Click drivers perform some convenience processing on handler
     * values, for example by removing a terminating newline from write
     * handler values or adding a terminating newline to read handler values.
     * Raw handlers do not have their values manipulated in this way.  Rawness
     * is set by the f_raw flag.
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
	return _flags & f_raw;
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


    /** @cond never */
    enum {
	h_read = f_read,
	h_write = f_write,
	h_read_param = f_read_param,
	h_exclusive = f_exclusive,
	h_nonexclusive = f_nonexclusive,
	h_raw = f_raw,
	h_read_private = f_read_private,
	h_write_private = f_write_private,
	h_deprecated = f_deprecated,
	h_uncommon = f_uncommon,
	h_calm = f_calm,
	h_expensive = f_expensive,
	h_button = f_button,
	h_checkbox = f_checkbox,
	h_driver_flag_0 = f_driver0,
        h_driver_flag_1 = f_driver1,
	h_user_flag_shift = f_user_shift,
	h_user_flag_0 = f_user0,
	h_read_comprehensive = f_read_comprehensive,
	h_write_comprehensive = f_write_comprehensive,
	h_special_flags = f_special
    };
    enum DeprecatedFlags {
	OP_READ = f_read,
	OP_WRITE = f_write,
	READ_PARAM = f_read_param,
	RAW = f_raw,
	// READ_PRIVATE = f_read_private,
	// WRITE_PRIVATE = f_write_private,
	// DEPRECATED = f_deprecated,
	// UNCOMMON = f_uncommon,
	CALM = f_calm,
	EXPENSIVE = f_expensive,
	BUTTON = f_button,
	CHECKBOX = f_checkbox,
	USER_FLAG_SHIFT = f_user_shift,
	USER_FLAG_0 = f_user0
    };
    enum CLICK_DEPRECATED {
	EXCLUSIVE = f_exclusive,
	NONEXCLUSIVE = f_nonexclusive
    };
    /** @endcond never */

    /** @cond never */
    /** @brief Call a read handler without parameters, passing new user_data().
     * @param e element on which to call the handler
     * @param new_user_data new user data
     *
     * This function should only be used for special purposes.  It fails
     * unless called on a handler created with a ReadHandlerCallback. */
    inline String __call_read(Element *e, void *new_user_data) const {
	assert((_flags & (f_read | f_read_comprehensive)) == f_read);
	return _read_hook.r(e, new_user_data);
    }
    /** @endcond never */

  private:

    String _name;
    union {
	HandlerCallback h;
	ReadHandlerCallback r;
    } _read_hook;
    union {
	HandlerCallback h;
	WriteHandlerCallback w;
    } _write_hook;
    void *_read_user_data;
    void *_write_user_data;
    uint32_t _flags;
    int _use_count;
    int _next_by_name;

    static const Handler *the_blank_handler;

    Handler(const String & = String());

    inline void combine(const Handler &x);
    inline bool compatible(const Handler &x) const;

    friend class Router;

};

/* The largest size a write handler is allowed to have. */
#define LARGEST_HANDLER_WRITE 65536

typedef HandlerCallback HandlerHook CLICK_DEPRECATED;
typedef ReadHandlerCallback ReadHandlerHook CLICK_DEPRECATED;
typedef WriteHandlerCallback WriteHandlerHook CLICK_DEPRECATED;

/** @cond never */
inline void *
Handler::user_data1() const
{
    return _read_user_data;
}

inline void *
Handler::user_data2() const
{
    return _write_user_data;
}
/** @endcond never */

CLICK_ENDDECLS
#endif
