// -*- related-file-name: "../../lib/error.cc" -*-
#ifndef CLICK_ERROR_HH
#define CLICK_ERROR_HH
#include <click/string.hh>
#include <click/pair.hh>
#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
# include <stdio.h>
#endif
#include <stdarg.h>
#if HAVE_ADDRESSABLE_VA_LIST
# define VA_LIST_REF_T		va_list *
# define VA_LIST_DEREF(val)	(*(val))
# define VA_LIST_REF(val)	(&(val))
#else
# define VA_LIST_REF_T		va_list
# define VA_LIST_DEREF(val)	(val)
# define VA_LIST_REF(val)	(val)
#endif
#if __GNUC__ <= 3
# define ERRH_SENTINEL
#else
# define ERRH_SENTINEL __attribute__((sentinel))
#endif
CLICK_DECLS

/** @class ErrorHandler
 * @brief Base class for error reporting.
 *
 * Click elements report errors through ErrorHandler objects, which represent
 * error collectors and printers.  ErrorHandlers are passed to configure() and
 * initialize() methods explicitly, as well as to write handlers; the
 * click_chatter() function calls ErrorHandler implicitly.
 *
 * <h3>Cooked error messages</h3>
 *
 * Most ErrorHandler interactions consist of a simple call like this:
 * @code
 * errh->error("not enough arguments (%d needed)", 5);
 *     // prints something like "not enough arguments (5 needed)\n"
 * @endcode
 *
 * This function constructs an error message string from the format arguments,
 * annotates the string with a default error level (here, el_error), and
 * prints it.  Alternate versions take a landmark specifying where the error
 * took place:
 * @code
 * errh->lwarning("file.click:2", "syntax error at '%s'", word.c_str());
 *     // prints something like "file.click:2: syntax error at 'foo'\n"
 * @endcode
 *
 * <h3>Raw error messages</h3>
 *
 * For finer control over error levels and annotations, construct an error
 * message string directly.  An error message is a string consisting of one or
 * more lines.  Each line begins with a set of optional textual @em
 * annotations.  The following error message has a @em level annotation
 * determining how serious the error is (this one is critical, since
 * el_critical == 2), and a @em landmark annotation, which specifies where the
 * error took place (here, "x.click:1"):
 *
 * <tt>"&lt;2&gt;{l:x.click:1}syntax error"</tt>
 *
 * Users can add other arbitrary annotations, which can be useful to pass
 * error metadata.  A pair of braces ends the annotation area.  This example
 * has one user annotation <tt>eoc</tt>, and a message area that would be
 * mistaken for an annotation except for the <tt>{}</tt>:
 *
 * <tt>"&lt;2&gt;{l:x.click:1}{eoc:520}{}{not:an annotation}"</tt>
 */
class ErrorHandler { public:

    /** @brief Error level constants.
     *
     * Lower values represent more serious errors.  Levels 0-7 correspond to
     * Linux's error levels.  Negative levels request immediate exit; at user
     * level, the Click process's exit status is the absolute value of the
     * error level. */
    enum Level {
	el_abort = -999,
	el_fatal = -1,
	el_emergency = 0,
	el_alert = 1,
	el_critical = 2,
	el_error = 3,
	el_warning = 4,
	el_notice = 5,
	el_info = 6,
	el_debug = 7
    };

    /** @brief Error level indicators. */
    static const char e_abort[],
	e_fatal[],
	e_emergency[],
	e_alert[],
	e_critical[],
	e_error[],
	e_warning[],
	e_warning_annotated[],
	e_notice[],
	e_info[],
	e_debug[];

    /** @brief Construct an ErrorHandler. */
    ErrorHandler() {
    }

    virtual ~ErrorHandler() {
    }


    /** @brief Initialize the ErrorHandler implementation.
     * @param errh default error handler
     * @return @a errh
     *
     * Call this function to initialize the ErrorHandler implementation.  The
     * function installs the default conversions, creates the
     * silent_handler(), and installs @a errh as the default error handler
     * (see default_handler()).
     *
     * @note The @a errh object becomes the property of the ErrorHandler
     * implementation and must not be deleted.
     * (ErrorHandler::static_cleanup() will delete it.)  Only the first call
     * to static_initialize() has any effect. */
    static ErrorHandler *static_initialize(ErrorHandler *errh);

    /** @brief Tear down the ErrorHandler implementation.
     *
     * Deletes the internal ErrorHandlers and uninstalls default
     * conversions. */
    static void static_cleanup();


    /** @brief Return the default ErrorHandler.
     * @sa static_initialize() */
    static ErrorHandler *default_handler() {
	return the_default_handler;
    }

    /** @brief Set the default ErrorHandler to @a errh.
     * @note @a errh becomes property of the ErrorHandler implementation,
     * and will be freed by static_cleanup().  However, any prior default
     * handler is @em not destroyed.  Callers should delete the prior handler
     * when necessary. */
    static void set_default_handler(ErrorHandler *errh);

    /** @brief Return the global silent ErrorHandler. */
    static ErrorHandler *silent_handler() {
	return the_silent_handler;
    }


    static const int ok_result;		///< Equals 0, used for error levels
					///  <5> and above
    static const int error_result;	///< Equals -EINVAL, used for error
					///  levels <4> and below


    /** @brief Print a debug message (level el_debug).
     *
     * @a fmt and any following arguments are parsed as by format(), and the
     * resulting string is passed to xmessage(). */
    void debug(const char *fmt, ...);
    /** @brief Print an informational message (level el_info). */
    void message(const char *fmt, ...);
    /** @brief Print a warning message (level el_warning).
     * @return error_result
     *
     * The string "warning: " is prepended to every line of the message. */
    int warning(const char *fmt, ...);
    /** @brief Print an error message (level el_error).
     * @return error_result */
    int error(const char *fmt, ...);
    /** @brief Print a fatal error message (level el_fatal).
     * @return error_result
     *
     * In many ErrorHandlers, calling fatal() will cause Click to abort. */
    int fatal(const char *fmt, ...);

    /** @brief Print a debug message with a landmark annotation. */
    void ldebug(const String &landmark, const char *fmt, ...);
    /** @brief Print an informational message with a landmark annotation. */
    void lmessage(const String &landmark, const char *fmt, ...);
    /** @brief Print a warning message with a landmark annotation. */
    int lwarning(const String &landmark, const char *fmt, ...);
    /** @brief Print an error message with a landmark annotation. */
    int lerror(const String &landmark, const char *fmt, ...);
    /** @brief Print a fatal error message with a landmark annotation. */
    int lfatal(const String &landmark, const char *fmt, ...);


    /** @brief Print an annotated error message.
     * @return ok_result if the minimum error level was el_notice or higher,
     * otherwise error_result
     *
     * This function drives the virtual functions actually responsible for
     * error message decoration and printing.  It passes @a str to decorate(),
     * separates the result into lines, calls emit() for each line, and calls
     * account() with the minimum error level of any line.
     *
     * Most users will call shorthand functions like error(), warning(), or
     * lmessage(), which add relevant annotations to the message. */
    int xmessage(const String &str);
    /** @brief Print an error message, adding annotations.
     * @param anno annotations
     * @param str error message
     *
     * Shorthand for xmessage(combine_anno(@a str, @a anno)). */
    int xmessage(const String &anno, const String &str) {
	return xmessage(combine_anno(str, anno));
    }
    /** @brief Format and print an error message, adding annotations.
     * @param anno annotations
     * @param fmt error message format
     * @param val format arguments
     *
     * Shorthand for xmessage(@a anno, format(@a fmt, @a val)). */
    int xmessage(const String &anno, const char *fmt, va_list val) {
	return xmessage(anno, format(fmt, val));
    }
    /** @brief Print an error message, adding landmark and other annotations.
     * @param landmark landmark annotation
     * @param anno additional annotations
     * @param str error message
     *
     * Shorthand for xmessage(combine_anno(@a anno, make_landmark_anno(@a
     * landmark)), @a str). */
    int xmessage(const String &landmark, const String &anno,
		 const String &str) {
	return xmessage(combine_anno(anno, make_landmark_anno(landmark)), str);
    }
    /** @brief Format and print an error message, adding landmark and other
     * annotations.
     * @param landmark landmark annotation
     * @param anno additional annotations
     * @param fmt error message format
     * @param val format arguments
     *
     * Shorthand for xmessage(@a landmark, @a anno, format(@a fmt, @a
     * val)). */
    int xmessage(const String &landmark, const String &anno,
		 const char *fmt, va_list val) {
	return xmessage(landmark, anno, format(fmt, val));
    }


    /** @brief Format an error string.
     * @param fmt printf-like format string
     * @return formatted error string
     *
     * Formats an error string using printf-like % conversions.  Conversions
     * include:
     *
     * <table>
     *
     * <tr><td><tt>\%d</tt>, <tt>\%i</tt></td><td>Format an <tt>int</tt> as a
     * decimal string.  Understands flags in <tt>#0- +</tt>, field widths
     * (including <tt>*</tt>), and precisions.</td></tr>
     *
     * <tr><td><tt>\%hd</tt>, <tt>\%ld</tt>, <tt>\%lld</tt>,
     * <tt>\%zd</tt></td><td>Format a <tt>short</tt>, <tt>long</tt>, <tt>long
     * long</tt>, or <tt>size_t</tt>.</td></tr>
     *
     * <tr><td><tt>\%^16d</tt>, <tt>\%^32d</tt>, <tt>\%^64d</tt></td>
     * <td>Format a 16-, 32-, or 64-bit integer.</td></tr>
     *
     * <tr><td><tt>\%o</tt>, <tt>\%u</tt>, <tt>\%x</tt>,
     * <tt>\%X</tt></td><td>Format an unsigned integer in octal, decimal, or
     * hexadecimal (with lower-case or upper-case letters).</td></tr>
     *
     * <tr><td><tt>\%s</tt></td><td>Format a C string (<tt>const char *</tt>).
     * The alternate form <tt>\%\#s</tt> calls String::printable() on the
     * input string.</td></tr>
     *
     * <tr><td><tt>\%c</tt></td><td>Format a character.  Prints a C-like
     * escape if the input character isn't printable ASCII.</td></tr>
     *
     * <tr><td><tt>\%p</tt></td><td>Format a pointer as a hexadecimal
     * value.</td></tr>
     *
     * <tr><td><tt>\%e</tt>, <tt>\%E</tt>, <tt>\%f</tt>, <tt>\%F</tt>,
     * <tt>\%g</tt>, <tt>\%G</tt></td><td>Format a <tt>double</tt> (user-level
     * only).</td></tr>
     *
     * <tr><td><tt>\%{...}</tt><td>Call a user-provided conversion function.
     * For example, <tt>\%{ip_ptr}</tt> reads an <tt>IPAddress *</tt> argument
     * from the argument list, and formats the pointed-to address using
     * IPAddress::unparse().</td></tr>
     *
     * <tr><td><tt>\%\%</tt></td><td>Format a literal \% character.</td></tr>
     *
     * </table> */
    static String format(const char *fmt, ...);
    /** @overload */
    static String format(const char *fmt, va_list val);


    /** @brief Decorate an error message.
     * @param str error message, possibly with annotations
     * @return decorated error message
     *
     * @warning ErrorHandler users don't need to call this function directly;
     * it is called implicitly by the error()/xmessage() functions.
     *
     * This virtual function is called to decorate an error message before it
     * is emitted.  The input @a str is an error message string, possibly
     * annotated.  The default implementation returns @a str unchanged.  Other
     * ErrorHandlers might add context lines (ContextErrorHandler), prefixes
     * (PrefixErrorHandler), or a default landmark (LandmarkErrorHandler). */
    virtual String decorate(const String &str);

    /** @brief Output an error message line.
     * @param str error message line, possibly with annotations
     * @param user_data callback data, 0 for first line in a message
     * @param more true iff this is the last line in the current message
     * @return @a user_data to be passed to emit() for the next line
     *
     * @warning ErrorHandler users don't need to call this function directly;
     * it is called implicitly by the error()/xmessage() functions.
     *
     * After calling decorate(), ErrorHandler splits the message into
     * individual lines and calls emit() once per line.  ErrorHandler
     * subclasses should output the error lines as appropriate; for example,
     * FileErrorHandler outputs the error message to a file.
     *
     * @a str does not contain a newline, but may contain annotations,
     * including a landmark annotation.  Most ErrorHandlers use parse_anno()
     * to extract the landmark annotation, clean it with clean_landmark(), and
     * print it ahead of the error message proper.
     *
     * ErrorHandler can handle multi-line error messages.  However, the emit()
     * function takes a line at a time; this is more useful in practice for
     * most error message printers.  The @a user_data and @a more arguments
     * can help an ErrorHandler combine the lines of a multi-line error
     * message.  @a user_data is null for the first line; for second and
     * subsequent lines, ErrorHandler passes the result of the last line's
     * emit() call.  @a more is true iff this is the last line in the current
     * message.
     *
     * The default emit() implementation does nothing. */
    virtual void *emit(const String &str, void *user_data, bool more);

    /** @brief Account for an error message at level @a level.
     * @param level minimum error level in the message
     *
     * @warning ErrorHandler users don't need to call this function directly;
     * it is called implicitly by the error()/xmessage() functions.
     *
     * After calling emit() for the lines of an error message, ErrorHandler
     * calls account(), passing the minimum (worst) error level of any message
     * line (or 1000 if no line had a level).  For instance, the
     * BaseErrorHandler::account() implementation updates the nwarnings() and
     * nerrors() counts.  Some other ErrorHandlers override account() to, for
     * example, exit on el_fatal level or below. */
    virtual void account(int level);


    /** @brief Return the number of warnings reported so far. */
    virtual int nwarnings() const = 0;
    /** @brief Return the number of errors reported so far. */
    virtual int nerrors() const = 0;
    /** @brief Reset the nwarnings() and nerrors() counts to zero. */
    virtual void reset_counts() = 0;



    /** @brief Create an error annotation.
     * @param name annotation name
     * @param value annotation value
     * @return annotation string
     *
     * Returns an error annotation that associates annotation @a name with @a
     * value.
     *
     * If @a name equals "<>", then returns a level annotation of the form
     * "<@a value>".  @a value must be valid number; if it isn't, the function
     * returns the empty string.
     *
     * Otherwise, @a name must be a nonempty series of letters and digits.
     * make_anno() returns a string of the form "{@a name:@a value}", where
     * special characters in @a value are quoted with backslashes. */
    static String make_anno(const char *name, const String &value);

    /** @brief Apply annotations from @a anno to every line in @a str.
     * @param str string
     * @param anno annotation string
     *
     * The annotations from @a anno are applied to every line in @a str.  New
     * annotations do not override existing annotations with the same names.
     * If the @a anno string ends with non-annotation characters, this
     * substring is prefixed to every line in @a str.
     *
     * For example:
     * @code
     * combine_anno("Line 1\n{l:old}{x:x}Line 2\n", "<0>{l:new}  ")
     *    // returns "<0>{l:new}  Line 1\n<0>{l:old}{x:x}  Line 2\n"
     * @endcode */
    static String combine_anno(const String &str, const String &anno);

    /** @brief Parse error annotations from a string.
     * @param str the string
     * @param begin pointer within @a str to start of annotation area
     * @param end pointer within @a str to end of annotation area
     * @return pointer to first character after annotation area
     * @pre @a str.begin() <= {@a begin, @a end} <= @a str.end()
     * @post @a begin <= returned value <= @a end
     *
     * Use this function to skip an error line's annotation area, possibly
     * extracting named annotations.
     *
     * The variable arguments portion consists of a series of pairs of C
     * strings and value pointers, terminated by a null character pointer.
     * Each C string is an annotation name.  The corresponding annotation
     * value, if found, is stored as a String object in the value pointer.
     * You can also store the <tt>int</tt> value of an annotation by prefixing
     * an annotation name with the '#' character.
     *
     * For example:
     * @code
     * String line = "{l:file:30}<4.5>error message\n";
     * String landmark_str, level_str;
     * const char *s = ErrorHandler::parse_anno(line, line.begin(), line.end(),
     *            "l", &landmark_str, "<>", &level_str, (const char *) 0);
     *     // Results: s points to "error message\n",
     *     // landmark_str == "file:30", level_str == "4.5"
     *
     * int level;
     * s = ErrorHandler::parse_anno(line, line.begin(), line.end(),
     *            "#<>", &level, (const char *) 0);
     *     // Results: s points to "error message\n", level_str == 4
     * @endcode */
    static const char *parse_anno(const String &str,
		const char *begin, const char *end, ...) ERRH_SENTINEL;


    /** @brief Return a landmark annotation equal to @a x.
     * @param x landmark
     *
     * If @a x is empty, returns the empty string.  Otherwise, if @a x looks
     * like a formatted annotation (it starts with an open brace), returns @a
     * x unchanged.  Otherwise, returns make_anno("l", @a x). */
    static String make_landmark_anno(const String &x) {
	if (x && x[0] == '{')
	    return x;
	else if (x)
	    return make_anno("l", x);
	else
	    return String();
    }

    /** @brief Clean the @a landmark.
     * @param landmark landmark text
     * @param colon if true, append <tt>": "</tt> to a nonempty landmark
     *
     * Removes trailing space and an optional trailing colon from @a landmark
     * and returns the result.  If @a colon is true, and the cleaned landmark
     * isn't the empty string, then appends <tt>": "</tt> to the result. */
    static String clean_landmark(const String &landmark, bool colon = false);


    // error conversions
    struct Conversion;
    typedef String (*ConversionFunction)(int flags, VA_LIST_REF_T);
    enum ConversionFlags {
	f_zero_pad = 1, f_plus_positive = 2, f_space_positive = 4,
	f_left_just = 8, f_alternate_form = 16, f_uppercase = 32,
	f_signed = 64, f_negative = 128
    };
    static Conversion *add_conversion(const String &name, ConversionFunction func);
    static int remove_conversion(Conversion *conversion);

  private:

    static ErrorHandler *the_default_handler;
    static ErrorHandler *the_silent_handler;

    static const char *skip_anno(const String &str,
				 const char *begin, const char *end,
				 String *name_result, String *value_result,
				 bool raw);

};


class BaseErrorHandler : public ErrorHandler { public:

    BaseErrorHandler()
	: _nwarnings(0), _nerrors(0) {
    }

    int nwarnings() const {
	return _nwarnings;
    }
    int nerrors() const {
	return _nerrors;
    }
    void reset_counts() {
	_nwarnings = _nerrors = 0;
    }

    void account(int level) {
	if (level <= el_error)
	    ++_nerrors;
	else if (level == el_warning)
	    ++_nwarnings;
    }

  private:

    int _nwarnings;
    int _nerrors;

};

class SilentErrorHandler : public BaseErrorHandler { public:

    SilentErrorHandler() {
    }

};

#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
class FileErrorHandler : public BaseErrorHandler { public:

    FileErrorHandler(FILE *, const String & = String());

    void *emit(const String &str, void *user_data, bool more);
    void account(int level);

  private:

    FILE *_f;
    String _context;

};
#endif

class LocalErrorHandler : public BaseErrorHandler { public:

    LocalErrorHandler(ErrorHandler *errh)	: _errh(errh) { }

    String decorate(const String &str);
    void *emit(const String &str, void *user_data, bool more);
    void account(int level);

  private:

    ErrorHandler *_errh;

};

class ErrorVeneer : public ErrorHandler { public:

    ErrorVeneer(ErrorHandler *errh)	: _errh(errh) { }

    int nwarnings() const;
    int nerrors() const;
    void reset_counts();

    String decorate(const String &str);
    void *emit(const String &str, void *user_data, bool more);
    void account(int level);

  protected:

    ErrorHandler *_errh;

};

class ContextErrorHandler : public ErrorVeneer { public:

    ContextErrorHandler(ErrorHandler *, const String &context, const String &indent = String::make_stable("  ", 2), const String &context_landmark = String());

    String decorate(const String &str);

  private:

    String _context;
    String _indent;
    String _context_landmark;

};

class PrefixErrorHandler : public ErrorVeneer { public:

    PrefixErrorHandler(ErrorHandler *errh, const String &prefix);

    String decorate(const String &str);

  private:

    String _prefix;

};

class LandmarkErrorHandler : public ErrorVeneer { public:

    LandmarkErrorHandler(ErrorHandler *err, const String &landmark);

    void set_landmark(const String &landmark) {
	_landmark = make_landmark_anno(landmark);
    }

    String decorate(const String &str);

  private:

    String _landmark;

};

#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
class BailErrorHandler : public ErrorVeneer { public:

    BailErrorHandler(ErrorHandler *errh, int level = el_error);

    void account(int level);

  private:

    int _level;

};
#endif

#undef ERRH_SENTINEL
CLICK_ENDDECLS
#endif
