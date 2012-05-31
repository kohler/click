// -*- c-basic-offset: 4; related-file-name: "../../lib/args.cc" -*-
#ifndef CLICK_ARGS_HH
#define CLICK_ARGS_HH
#include <click/type_traits.hh>
#include <click/vector.hh>
#include <click/string.hh>
#include <click/confparse.hh>
#include <click/timestamp.hh>
#if CLICK_BSDMODULE
# include <machine/stdarg.h>
#else
# include <stdarg.h>
#endif
CLICK_DECLS
class Element;
class ErrorHandler;

/** @class ArgContext
  @brief Argument context class.

  The ArgContext class encapsulates state useful for parsing arguments: an
  element context and an ErrorHandler for reporting parse errors.

  Args is derived from ArgContext.  Some parser functions take an ArgContext
  reference rather than an Args reference.  This clarifies that the parser
  function doesn't modify Args internals.  Also, ArgContext objects are smaller
  and quicker to construct than Args objects.
*/
class ArgContext { public:

    /** @brief Construct an argument context.
     * @param errh optional error handler */
    ArgContext(ErrorHandler *errh = 0)
	: _errh(errh), _arg_keyword(0), _read_status(false) {
#if !CLICK_TOOL
	_context = 0;
#endif
    }

#if !CLICK_TOOL
    /** @brief Construct an argument context.
     * @param context optional element context
     * @param errh optional error handler */
    ArgContext(const Element *context, ErrorHandler *errh = 0)
	: _context(context), _errh(errh), _arg_keyword(0), _read_status(false) {
    }

    /** @brief Return the element context. */
    const Element *context() const {
	return _context;
    }
#endif

    /** @brief Return the associated error handler. */
    ErrorHandler *errh() const {
	return _errh;
    }

    /** @brief Return a prefix string associated with the current argument.
     *
     * If the current argument is keyword FOO, returns "FOO: ". */
    String error_prefix() const;

    /** @brief Report a parse error for the current argument. */
    void error(const char *fmt, ...) const;

    /** @brief Report a parse warning for the current argument. */
    void warning(const char *fmt, ...) const;

    /** @brief Report a message for the current argument. */
    void message(const char *fmt, ...) const;

    void xmessage(const String &anno, const String &str) const;
    void xmessage(const String &anno, const char *fmt, va_list val) const;

  protected:

#if !CLICK_TOOL
    const Element *_context;
#endif
    ErrorHandler *_errh;
    const char *_arg_keyword;
    mutable bool _read_status;

};


/** @class Args
  @brief Argument parser class.

  The Args class parses Click configuration strings in a type-safe manner.

  Args manages two types of state, <em>arguments</em> and <em>results</em>.
  Arguments are strings to be parsed, and results are parsed values.  As
  arguments are successfully parsed, Args marks off arguments and adds new
  results.

  Each result is paired with a <em>variable</em> in the caller's context.  A
  separate <em>execution</em> step assigns each parsed result to its variable
  (but only if no parse error was previously encountered).

  Arguments are named with keywords and parsed by read() functions.  For
  example:

  @code
  Args args; args.push_back("A 1"); // one argument
  int a = 2;

  args.read("A", a);  // parses the argument "A" and creates a result
  assert(a == 2);     // caller's variable not assigned yet
  args.execute();     // now caller's variable is assigned
  assert(a == 1);
  @endcode

  Each read() function comes in five variants.  read() reads an optional
  keyword argument.  read_m() reads a mandatory keyword argument: if the
  argument was not supplied, Args will report an error.  read_p() reads an
  optional positional argument: if the keyword was not supplied, but a
  positional argument was, that is used.  read_mp() reads a mandatory
  positional argument.  Positional arguments are parsed in order.  The fifth
  variant of read() takes an integer <em>flags</em> argument; flags include
  Args::positional, Args::mandatory, and others, such as Args::deprecated.

  The complete() execution method checks that every argument has been
  successfully parsed and reports an error if not.  The consume() execution
  method doesn't check for completion, but removes parsed arguments from the
  argument set.  Execution methods return 0 on success and <0 on failure.
  Check the parse status before execution using the status() method.

  Args methods are designed to chain.  Many methods return a reference to the
  called object, and it is often possible to parse a whole set of arguments
  using a single temporary.  For example:

  @code
  Vector<String> conf;
  conf.push_back("A 1");
  conf.push_back("B 2");

  int a, b;
  if (Args(conf).read("A", a)
      .read("B", b)
      .complete() >= 0)
      click_chatter("Success! a=%d, b=%d", a, b);
  @endcode

  Arguments are parsed by independent <em>parser objects</em>.  Many common
  variable types have default parsers defined by the DefaultParser<T>
  template.  For example, the default parser for an integer value understands
  the common textual representations of integers.  You can also pass a parser
  explicitly.  For example:

  @code
  int a, b, c;
  args.read("A", a)      // parse A using DefaultParser<int> = IntArg()
      .read("B", IntArg(2), b);   // parse B using IntArg(2): base-2
  @endcode

  Args generally calls a parser object's parse() method with three arguments:

  <ol>
  <li>const String &<b>str</b>: The string to be parsed.</li>
  <li>T &<b>result</b>: A reference to the result.  The parsed value, if any,
     should be stored here.  (This points to storage local to Args, not to
     the caller's variable.)</li>
  <li>Args &<b>args</b>: A reference to the calling Args object, for error
     reporting.</li>
  </ol>

  The parse() method should return true if the parse succeeds and false if it
  fails.  Type-specific error messages should be reported using methods like
  <b>args</b>.error().  For generic errors, the parse() method can simply
  return false; Args will generate a "KEYWORD: parse error" message.

  parse() methods can be static or non-static.  Most parsers are
  <em>disciplined</em>, meaning that they modify <b>result</b> only if the
  parse succeeds.  (This doesn't matter in the context of Args, but can matter
  to users who call a parse function directly.)
*/
class Args : public ArgContext {
    struct Slot;

  public:

    /** @brief Construct an argument parser.
     * @param errh optional error handler */
    Args(ErrorHandler *errh = 0);

    /** @brief Construct an argument parser parsing a copy of @a conf.
     * @param conf list of configuration arguments
     * @param errh optional error handler */
    Args(const Vector<String> &conf, ErrorHandler *errh = 0);

#if !CLICK_TOOL
    /** @brief Construct an argument parser.
     * @param context optional element context
     * @param errh optional error handler */
    Args(const Element *context, ErrorHandler *errh = 0);

    /** @brief Construct an argument parser parsing a copy of @a conf.
     * @param conf list of configuration arguments
     * @param context optional element context
     * @param errh optional error handler */
    Args(const Vector<String> &conf, const Element *context,
	 ErrorHandler *errh = 0);
#endif



    /** @brief Copy construct an argument parser.
     * @note @a x's results are not copied. */
    Args(const Args &x);

    ~Args();

    /** @brief Assign to a copy of @a x.
     * @pre results_empty() && @a x.results_empty() */
    Args &operator=(const Args &x);


    /** @brief Return true iff this parser has no arguments or results. */
    bool empty() const {
	return (!_conf || !_conf->size()) && !_slots && _simple_slotbuf[0] == 0;
    }

    /** @brief Return true iff this parser has no results. */
    bool results_empty() const {
	return !_slots && _simple_slotbuf[0] == 0;
    }


    /** @brief Remove all arguments.
     * @return *this */
    Args &clear() {
	if (_conf)
	    _conf->clear();
	_kwpos.clear();
	return *this;
    }

    /** @brief Bind this parser's arguments to @a conf.
     * @param conf reference to new arguments
     * @return *this
     * @post The argument parser shares @a conf with the caller.
     * For instance, consume() will modify @a conf. */
    Args &bind(Vector<String> &conf);

    /** @brief Append argument @a arg to this parser.
     * @return *this */
    Args &push_back(const String &arg);

    /** @brief Append arguments in the range [@a begin, @a end) to this parser.
     * @return *this */
    template<typename Iter> Args &push_back(Iter begin, Iter end) {
	while (begin != end) {
	    push_back(*begin);
	    ++begin;
	}
	return *this;
    }

    /** @brief Append the space-separated words in @a str to this parser.
     * @return *this */
    Args &push_back_words(const String &str);

    /** @brief Append the comma-separated arguments in @a str to this parser.
     * @return *this */
    Args &push_back_args(const String &str);

    /** @brief Reset the parse status for every argument.
     * @return *this
     *
     * For example:
     * @code
     * Vector<String> conf; conf.push_back("1"); conf.push_back("2");
     * int a, b;
     * Args(conf).read_p("A", a).read_p("B", b).execute();
     * assert(a == 1 && b == 2);
     * Args(conf).read_p("A", a).reset().read_p("B", b).execute();
     * assert(a == 1 && b == 1);
     * @endcode
     * Results are not affected. */
    Args &reset() {
	reset_from(0);
	return *this;
    }


    static constexpr int mandatory = 1;  ///< read flag for mandatory arguments
    static constexpr int positional = 2; ///< read flag for positional arguments
    static constexpr int deprecated = 4; ///< read flag for deprecated arguments
    static constexpr int firstmatch = 8; ///< read flag to take first matching argument

    /** @brief Read an argument using its type's default parser.
     * @param keyword argument name
     * @param variable reference to result variable
     * @return *this
     *
     * Creates a result for type T.  Calls DefaultParser<T>().parse(string,
     * result, *this). */
    template<typename T>
    Args &read(const char *keyword, T &variable) {
	return read(keyword, 0, variable);
    }
    template<typename T>
    Args &read_m(const char *keyword, T &variable) {
	return read(keyword, mandatory, variable);
    }
    template<typename T>
    Args &read_p(const char *keyword, T &variable) {
	return read(keyword, positional, variable);
    }
    template<typename T>
    Args &read_mp(const char *keyword, T &variable) {
	return read(keyword, mandatory | positional, variable);
    }
    template<typename T>
    Args &read(const char *keyword, int flags, T &variable) {
	args_base_read(this, keyword, flags, variable);
	return *this;
    }

    /** @brief Read an argument using the default parser, or set it to a
     *    default value if the argument is was not supplied.
     * @param keyword argument name
     * @param variable reference to result variable
     * @param value default value
     * @return *this
     *
     * Calls DefaultParser<T>().parse(string, result, this) if argument @a
     * keyword was supplied.  Otherwise, adds a result that will set @a variable
     * to @a value. */
    template<typename T, typename V>
    Args &read_or_set(const char *keyword, T &variable, const V &value) {
	return read_or_set(keyword, 0, variable, value);
    }
    template<typename T, typename V>
    Args &read_or_set_p(const char *keyword, T &variable, const V &value) {
	return read_or_set(keyword, positional, variable, value);
    }
    template<typename T, typename V>
    Args &read_or_set(const char *keyword, int flags, T &variable, const V &value) {
	args_base_read_or_set(this, keyword, flags, variable, value);
	return *this;
    }

    /** @brief Read an argument using a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @param variable reference to result variable
     * @return *this
     *
     * Calls @a parser.parse(string, variable, *this). */
    template<typename P, typename T>
    Args &read(const char *keyword, P parser, T &variable) {
	return read(keyword, 0, parser, variable);
    }
    template<typename P, typename T>
    Args &read_m(const char *keyword, P parser, T &variable) {
	return read(keyword, mandatory, parser, variable);
    }
    template<typename P, typename T>
    Args &read_p(const char *keyword, P parser, T &variable) {
	return read(keyword, positional, parser, variable);
    }
    template<typename P, typename T>
    Args &read_mp(const char *keyword, P parser, T &variable) {
	return read(keyword, mandatory | positional, parser, variable);
    }
    template<typename P, typename T>
    Args &read(const char *keyword, int flags, P parser, T &variable) {
	args_base_read(this, keyword, flags, parser, variable);
	return *this;
    }

    /** @brief Read an argument using a specified parser, or set it to a
     *    default value if the argument is was not supplied.
     * @param keyword argument name
     * @param parser parser object
     * @param variable reference to result variable
     * @param value default value
     * @return *this
     *
     * Calls @a parser.parse(string, variable, *this) if argument @a keyword was
     * supplied.  Otherwise, adds a result that will set @a variable to @a
     * value. */
    template<typename P, typename T, typename V>
    Args &read_or_set(const char *keyword, P parser, T &variable, const V &value) {
	return read_or_set(keyword, 0, parser, variable, value);
    }
    template<typename P, typename T, typename V>
    Args &read_or_set_p(const char *keyword, P parser, T &variable, const V &value) {
	return read_or_set(keyword, positional, parser, variable, value);
    }
    template<typename P, typename T, typename V>
    Args &read_or_set(const char *keyword, int flags, P parser, T &variable, const V &value) {
	args_base_read_or_set(this, keyword, flags, parser, variable, value);
	return *this;
    }

    /** @brief Read an argument using a specified parser with two results.
     * @param keyword argument name
     * @param parser parser object
     * @param variable1 reference to first result variable
     * @param variable2 reference to first result variable
     * @return *this
     *
     * Calls @a parser.parse(string, variable1, variable2, *this). */
    template<typename P, typename T1, typename T2>
    Args &read(const char *keyword, P parser, T1 &variable1, T2 &variable2) {
	return read(keyword, 0, parser, variable1, variable2);
    }
    template<typename P, typename T1, typename T2>
    Args &read_m(const char *keyword, P parser, T1 &variable1, T2 &variable2) {
	return read(keyword, mandatory, parser, variable1, variable2);
    }
    template<typename P, typename T1, typename T2>
    Args &read_p(const char *keyword, P parser, T1 &variable1, T2 &variable2) {
	return read(keyword, positional, parser, variable1, variable2);
    }
    template<typename P, typename T1, typename T2>
    Args &read_mp(const char *keyword, P parser, T1 &variable1, T2 &variable2) {
	return read(keyword, mandatory | positional, parser, variable1, variable2);
    }
    template<typename P, typename T1, typename T2>
    Args &read(const char *keyword, int flags, P parser, T1 &variable1, T2 &variable2) {
	args_base_read(this, keyword, flags, parser, variable1, variable2);
	return *this;
    }

    /** @brief Pass an argument to a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @return *this
     *
     * Calls @a parser.parse(string, *this). */
    template<typename P>
    Args &read_with(const char *keyword, P parser) {
	return read_with(keyword, 0, parser);
    }
    template<typename P>
    Args &read_m_with(const char *keyword, P parser) {
	return read_with(keyword, mandatory, parser);
    }
    template<typename P>
    Args &read_p_with(const char *keyword, P parser) {
	return read_with(keyword, positional, parser);
    }
    template<typename P>
    Args &read_mp_with(const char *keyword, P parser) {
	return read_with(keyword, mandatory | positional, parser);
    }
    template<typename P>
    Args &read_with(const char *keyword, int flags, P parser) {
	args_base_read_with(this, keyword, flags, parser);
	return *this;
    }

    /** @brief Pass an argument to a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @param variable reference to result variable
     * @return *this
     *
     * Calls @a parser.parse(string, *this, variable). */
    template<typename P, typename T>
    Args &read_with(const char *keyword, P parser, T &variable) {
	return read_with(keyword, 0, parser, variable);
    }
    template<typename P, typename T>
    Args &read_m_with(const char *keyword, P parser, T &variable) {
	return read_with(keyword, mandatory, parser, variable);
    }
    template<typename P, typename T>
    Args &read_p_with(const char *keyword, P parser, T &variable) {
	return read_with(keyword, positional, parser, variable);
    }
    template<typename P, typename T>
    Args &read_mp_with(const char *keyword, P parser, T &variable) {
	return read_with(keyword, mandatory | positional, parser, variable);
    }
    template<typename P, typename T>
    Args &read_with(const char *keyword, int flags, P parser, T &variable) {
	args_base_read_with(this, keyword, flags, parser, variable);
	return *this;
    }

    /** @brief Pass all matching arguments to a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @return *this
     *
     * Calls @a parser.parse(string, *this) zero or more times.
     *
     * @note The value of read_status() is true iff at least one argument
     * matched and all matching arguments successfully parsed. */
    template<typename P>
    Args &read_all_with(const char *keyword, P parser) {
	return read_all_with(keyword, 0, parser);
    }
    template<typename P>
    Args &read_all_with(const char *keyword, int flags, P parser) {
	args_base_read_all_with(this, keyword, flags | firstmatch, parser);
	return *this;
    }

    /** @brief Pass all matching arguments to a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @param variable reference to result variable
     * @return *this
     *
     * Calls @a parser.parse(string, *this, variable) zero or more times.
     *
     * @note The value of read_status() is true iff at least one argument
     * matched and all matching arguments successfully parsed. */
    template<typename P, typename T>
    Args &read_all_with(const char *keyword, P parser, T &variable) {
	return read_all_with(keyword, 0, parser, variable);
    }
    template<typename P, typename T>
    Args &read_all_with(const char *keyword, int flags, P parser, T &variable) {
	args_base_read_all_with(this, keyword, flags | firstmatch, parser, variable);
	return *this;
    }


    /** @brief Return the current parse status. */
    bool status() const {
	return _status;
    }
    /** @brief Set @a x to the current parse status.
     * @return *this */
    Args &status(bool &x) {
	x = _status;
	return *this;
    }
    /** @overload */
    const Args &status(bool &x) const {
	x = _status;
	return *this;
    }

    /** @brief Return true iff the last read request succeeded.
     *
     * This function should only be called after a read. */
    bool read_status() const {
	return _read_status;
    }
    /** @brief Set @a x to the success status of the last read request.
     *
     * This function should only be called after a read. */
    Args &read_status(bool &x) {
	x = _read_status;
	return *this;
    }
    /** @overload */
    const Args &read_status(bool &x) const {
	x = _read_status;
	return *this;
    }


    /** @brief Remove all arguments matched so far. */
    Args &strip();

    /** @brief Assign results.
     * @return 0 if the parse succeeded, <0 otherwise
     * @post results_empty()
     *
     * Results are only assigned if status() is true (the parse is successful
     * so far).  Clears results as a side effect. */
    int execute();

    /** @brief Assign results and remove matched arguments.
     * @return 0 if the parse succeeded, <0 otherwise
     * @post results_empty()
     *
     * Matched arguments are always removed.  Results are only assigned if
     * status() is true (the parse is successful so far).  Clears results as a
     * side effect. */
    int consume();

    /** @brief Assign results if all arguments matched.
     * @return 0 if the parse succeeded, <0 otherwise
     * @post results_empty()
     *
     * Results are only assigned if status() is true (the parse is successful
     * so far) and all arguments have been parsed.  Clears results as a side
     * effect. */
    int complete();


    /** @brief Create and return a result that assigns @a variable. */
    template<typename T>
    T *slot(T &variable) {
	if (has_trivial_copy<T>::value)
	    return reinterpret_cast<T *>(simple_slot(&variable, sizeof(T)));
	else
	    return complex_slot(variable);
    }

    /** @brief Add a result that assigns @a variable to @a value.
     * @return *this */
    template<typename T, typename V>
    Args &set(T &variable, const V &value) {
	if (T *s = slot(variable))
	    *s = value;
	return *this;
    }


    /** @cond never */
    template<typename T>
    void base_read(const char *keyword, int flags, T &variable) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status)) {
	    T *s = slot(variable);
	    postparse(s && DefaultArg<T>().parse(str, *s, *this), slot_status);
	}
    }

    template<typename T, typename V>
    void base_read_or_set(const char *keyword, int flags, T &variable, const V &value) {
	Slot *slot_status;
	String str = find(keyword, flags, slot_status);
	T *s = slot(variable);
	postparse(s && (str ? DefaultArg<T>().parse(str, *s, *this) : (*s = value, true)), slot_status);
    }

    template<typename P, typename T>
    void base_read(const char *keyword, int flags, P parser, T &variable) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status)) {
	    T *s = slot(variable);
	    postparse(s && parser.parse(str, *s, *this), slot_status);
	}
    }

    template<typename T, typename P, typename V>
    void base_read_or_set(const char *keyword, int flags, P parser, T &variable, const V &value) {
	Slot *slot_status;
	String str = find(keyword, flags, slot_status);
	T *s = slot(variable);
	postparse(s && (str ? parser.parse(str, *s, *this) : (*s = value, true)), slot_status);
    }

    template<typename P, typename T1, typename T2>
    void base_read(const char *keyword, int flags,
		   P parser, T1 &variable1, T2 &variable2) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status)) {
	    T1 *s1 = slot(variable1);
	    T2 *s2 = slot(variable2);
	    postparse(s1 && s2 && parser.parse(str, *s1, *s2, *this), slot_status);
	}
    }

    template<typename P>
    void base_read_with(const char *keyword, int flags, P parser) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status))
	    postparse(parser.parse(str, *this), slot_status);
    }

    template<typename P, typename T>
    void base_read_with(const char *keyword, int flags, P parser, T &variable) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status))
	    postparse(parser.parse(str, *this, variable), slot_status);
    }

    template<typename P>
    void base_read_all_with(const char *keyword, int flags, P parser) {
	Slot *slot_status;
	int read_status = -1;
	while (String str = find(keyword, flags, slot_status)) {
	    postparse(parser.parse(str, *this), slot_status);
	    read_status = (read_status != 0) && _read_status;
	    flags &= ~mandatory;
	}
	_read_status = (read_status == 1);
    }

    template<typename P, typename T>
    void base_read_all_with(const char *keyword, int flags, P parser, T &variable) {
	Slot *slot_status;
	int read_status = -1;
	while (String str = find(keyword, flags, slot_status)) {
	    postparse(parser.parse(str, *this, variable), slot_status);
	    read_status = (read_status != 0) && _read_status;
	    flags &= ~mandatory;
	}
	_read_status = (read_status == 1);
    }
    /** @endcond never */

  private:

    struct Slot {
	Slot() {
	}
	virtual ~Slot() {
	}
	virtual void store() = 0;
	Slot *_next;
    };

    struct BytesSlot : public Slot {
	BytesSlot(void *ptr, size_t size)
	    : _ptr(ptr), _slot(new char[size]), _size(size) {
	}
	~BytesSlot() {
	    delete[] _slot;
	}
	void store() {
	    memcpy(_ptr, _slot, _size);
	}
	void *_ptr;
	char *_slot;
	size_t _size;
    };

    template<typename T>
    struct SlotT : public Slot {
	SlotT(T *ptr)
	    : _ptr(ptr) {
	}
	void store() {
	    assign_consume(*_ptr, _slot);
	}
	T *_ptr;
	T _slot;
    };

    enum {
#if SIZEOF_VOID_P == 4
	simple_slotbuf_size = 24
#else
	simple_slotbuf_size = 48
#endif
    };

#if !CLICK_DEBUG_ARGS_USAGE
    bool _my_conf;
#else
    bool _my_conf : 1;
    bool _consumed : 1;
#endif
    bool _status;
    uint8_t _simple_slotpos;

    Vector<String> *_conf;
    Vector<int> _kwpos;

    Slot *_slots;
    uint8_t _simple_slotbuf[simple_slotbuf_size];

    inline void initialize(const Vector<String> *conf);
    void reset_from(int i);

    String find(const char *keyword, int flags, Slot *&slot_status);
    void postparse(bool ok, Slot *slot_status);
    void check_complete();

    static inline int simple_slot_size(int size);
    inline void simple_slot_info(int offset, int size,
				 void *&slot, void **&pointer);
    void *simple_slot(void *data, size_t size);
    template<typename T> T *complex_slot(T &variable);

};

extern const ArgContext blank_args;


template<typename T>
struct DefaultArg {
};

template<typename T>
T *Args::complex_slot(T &variable)
{
    if (SlotT<T> *s = new SlotT<T>(&variable)) {
	s->_next = _slots;
	_slots = s;
	return &s->_slot;
    } else {
	error("out of memory");
	return 0;
    }
}


/** @cond never */
/* These functions are here because some GCC versions ignore noinline
   attributes on member function templates. */
template<typename T>
void args_base_read(Args *args, const char *keyword, int flags, T &variable)
    CLICK_NOINLINE;
template<typename T>
void args_base_read(Args *args, const char *keyword, int flags, T &variable)
{
    args->base_read(keyword, flags, variable);
}

template<typename T, typename V>
void args_base_read_or_set(Args *args, const char *keyword, int flags,
			   T &variable, const V &value) CLICK_NOINLINE;
template<typename T, typename V>
void args_base_read_or_set(Args *args, const char *keyword, int flags,
			   T &variable, const V &value)
{
    args->base_read_or_set(keyword, flags, variable, value);
}

template<typename P, typename T>
void args_base_read(Args *args, const char *keyword, int flags,
		    P parser, T &variable) CLICK_NOINLINE;
template<typename P, typename T>
void args_base_read(Args *args, const char *keyword, int flags,
		    P parser, T &variable)
{
    args->base_read(keyword, flags, parser, variable);
}

template<typename P, typename T, typename V>
void args_base_read_or_set(Args *args, const char *keyword, int flags,
			   P parser, T &variable, const V &value) CLICK_NOINLINE;
template<typename P, typename T, typename V>
void args_base_read_or_set(Args *args, const char *keyword, int flags,
			   P parser, T &variable, const V &value)
{
    args->base_read_or_set(keyword, flags, parser, variable, value);
}

template<typename P, typename T1, typename T2>
void args_base_read(Args *args, const char *keyword, int flags,
		    P parser, T1 &variable1, T2 &variable2) CLICK_NOINLINE;
template<typename P, typename T1, typename T2>
void args_base_read(Args *args, const char *keyword, int flags,
		    P parser, T1 &variable1, T2 &variable2)
{
    args->base_read(keyword, flags, parser, variable1, variable2);
}

template<typename P>
void args_base_read_with(Args *args, const char *keyword, int flags, P parser)
    CLICK_NOINLINE;
template<typename P>
void args_base_read_with(Args *args, const char *keyword, int flags, P parser)
{
    args->base_read_with(keyword, flags, parser);
}

template<typename P, typename T>
void args_base_read_with(Args *args, const char *keyword, int flags,
			 P parser, T &variable) CLICK_NOINLINE;
template<typename P, typename T>
void args_base_read_with(Args *args, const char *keyword, int flags,
			 P parser, T &variable)
{
    args->base_read_with(keyword, flags, parser, variable);
}

template<typename P>
void args_base_read_all_with(Args *args, const char *keyword, int flags, P parser)
    CLICK_NOINLINE;
template<typename P>
void args_base_read_all_with(Args *args, const char *keyword, int flags, P parser)
{
    args->base_read_all_with(keyword, flags, parser);
}

template<typename P, typename T>
void args_base_read_all_with(Args *args, const char *keyword, int flags,
			     P parser, T &variable) CLICK_NOINLINE;
template<typename P, typename T>
void args_base_read_all_with(Args *args, const char *keyword, int flags,
			     P parser, T &variable)
{
    args->base_read_all_with(keyword, flags, parser, variable);
}
/** @endcond never */


class NumArg { public:
    enum {
	status_ok = 0,
	status_inval = EINVAL,
	status_range = ERANGE,
#if defined(ENOTSUP)
	status_notsup = ENOTSUP,
#elif defined(ENOTSUPP)
	status_notsup = ENOTSUPP,
#else
	status_notsup,
#endif
	status_unitless
    };
};


/** @class IntArg
  @brief Parser class for integers.

  IntArg(@a base) reads integers in base @a base.  @a base defaults to 0,
  which means arguments are parsed in base 10 by default, but prefixes 0x, 0,
  and 0b parse hexadecimal, octal, and binary numbers, respectively.

  Integer overflow is treated as an error.

  @sa SaturatingIntArg, BoundedIntArg */
class IntArg : public NumArg { public:

    typedef uint32_t limb_type;

    IntArg(int b = 0)
	: base(b) {
    }

    const char *parse(const char *begin, const char *end,
		      bool is_signed, int size,
		      limb_type *value, int nlimb);

    template<typename V>
    bool parse_saturating(const String &str, V &result, const ArgContext &args = blank_args) {
	(void) args;
	constexpr bool is_signed = integer_traits<V>::is_signed;
	constexpr int nlimb = int((sizeof(V) + sizeof(limb_type) - 1) / sizeof(limb_type));
	limb_type x[nlimb];
	if (parse(str.begin(), str.end(), is_signed, int(sizeof(V)), x, nlimb)
	    != str.end())
	    status = status_inval;
	if (status && status != status_range)
	    return false;
	typedef typename make_unsigned<V>::type unsigned_v_type;
	extract_integer(x, reinterpret_cast<unsigned_v_type &>(result));
	return true;
    }

    template<typename V>
    bool parse(const String &str, V &result, const ArgContext &args = blank_args) {
	V x;
	if (!parse_saturating(str, x, args)
	    || (status && status != status_range))
	    return false;
	else if (status == status_range) {
	    typedef typename make_unsigned<V>::type unsigned_v_type;
	    report_error(args, integer_traits<V>::negative(x),
			 unsigned_v_type(x));
	    return false;
	} else {
	    result = x;
	    return true;
	}
    }

    int base;
    int status;

  protected:

    static const char *span(const char *begin, const char *end,
			    bool is_signed, int &b);
    void report_error(const ArgContext &args, bool negative,
		      click_uint_large_t value) const;

};

/** @class SaturatingIntArg
  @brief Parser class for integers with saturating overflow.

  SaturatingIntArg(@a base) is like IntArg(@a base), but integer overflow is
  not an error; instead, the closest representable value is returned.

  @sa IntArg */
class SaturatingIntArg : public IntArg { public:
    SaturatingIntArg(int b = 0)
	: IntArg(b) {
    }

    template<typename V>
    bool parse(const String &str, V &result, const ArgContext &args = blank_args) {
	return parse_saturating(str, result, args);
    }
};

/** @class BoundedIntArg
  @brief Parser class for integers with explicit bounds.

  BoundedIntArg(@a min, @a max, @a base) is like IntArg(@a base), but numbers
  less than @a min or greater than @a max are treated as errors.

  @sa IntArg */
class BoundedIntArg : public IntArg { public:
    BoundedIntArg(int min_value, int max_value, int b = 0)
	: IntArg(b), min_value(min_value), max_value(max_value) {
    }

    template<typename V>
    bool parse(const String &str, V &result, const ArgContext &args = blank_args) {
	V x;
	if (!IntArg::parse(str, x, args))
	    return false;
	else if (x < min_value || x > max_value) {
	    int bound = x < min_value ? min_value : max_value;
	    status = status_range;
	    report_error(args, bound < 0, bound < 0 ? -bound : bound);
	    return false;
	} else {
	    result = x;
	    return true;
	}
    }

    int min_value;
    int max_value;
};

template<> struct DefaultArg<unsigned char> : public IntArg {};
template<> struct DefaultArg<signed char> : public IntArg {};
template<> struct DefaultArg<char> : public IntArg {};
template<> struct DefaultArg<unsigned short> : public IntArg {};
template<> struct DefaultArg<short> : public IntArg {};
template<> struct DefaultArg<unsigned int> : public IntArg {};
template<> struct DefaultArg<int> : public IntArg {};
template<> struct DefaultArg<unsigned long> : public IntArg {};
template<> struct DefaultArg<long> : public IntArg {};
#if HAVE_LONG_LONG
template<> struct DefaultArg<unsigned long long> : public IntArg {};
template<> struct DefaultArg<long long> : public IntArg {};
#endif


/** @class FixedPointArg
  @brief Parser class for fixed-point numbers with @a b bits of fraction. */
class FixedPointArg : public NumArg { public:
    explicit FixedPointArg(int b, int exponent = 0)
	: fraction_bits(b), exponent_delta(exponent) {
    }
    inline bool parse_saturating(const String &str, uint32_t &result, const ArgContext &args = blank_args);
    bool parse(const String &str, uint32_t &result, const ArgContext &args = blank_args);
    bool parse_saturating(const String &str, int32_t &result, const ArgContext &args = blank_args);
    bool parse(const String &str, int32_t &result, const ArgContext &args = blank_args);
    int fraction_bits;
    int exponent_delta;
    int status;
  private:
    bool underparse(const String &str, bool is_signed, uint32_t &result);
};

inline bool
FixedPointArg::parse_saturating(const String &str, uint32_t &result, const ArgContext &)
{
    return underparse(str, false, result);
}

/** @class DecimalFixedPointArg
  @brief Parser class for fixed-point numbers with @a d decimal digits of fraction. */
class DecimalFixedPointArg : public NumArg { public:
    DecimalFixedPointArg(int d, int exponent = 0)
	: fraction_digits(d), exponent_delta(exponent) {
    }
    inline bool parse_saturating(const String &str, uint32_t &result, const ArgContext &args = blank_args);
    bool parse(const String &str, uint32_t &result, const ArgContext &args = blank_args);
    bool parse_saturating(const String &str, int32_t &result, const ArgContext &args = blank_args);
    bool parse(const String &str, int32_t &result, const ArgContext &args = blank_args);
    bool parse_saturating(const String &str, uint32_t &iresult, uint32_t &fresult, const ArgContext &args = blank_args);
    bool parse(const String &str, uint32_t &iresult, uint32_t &fresult, const ArgContext &args = blank_args);
    int fraction_digits;
    int exponent_delta;
    int status;
  private:
    bool underparse(const String &str, bool is_signed, uint32_t &result);
};

inline bool
DecimalFixedPointArg::parse_saturating(const String &str, uint32_t &result, const ArgContext &)
{
    return underparse(str, false, result);
}


#if HAVE_FLOAT_TYPES
/** @class DoubleArg
  @brief Parser class for double-precision floating point numbers. */
class DoubleArg : public NumArg { public:
    DoubleArg() {
    }
    bool parse(const String &str, double &result, const ArgContext &args = blank_args);
    int status;
};

template<> struct DefaultArg<double> : public DoubleArg {};
#endif


/** @class BoolArg
  @brief Parser class for booleans. */
class BoolArg { public:
    static bool parse(const String &str, bool &result, const ArgContext &args = blank_args);
    static String unparse(bool x) {
	return String(x);
    }
};

template<> struct DefaultArg<bool> : public BoolArg {};


class UnitArg { public:
    explicit UnitArg(const char *unit_def, const char *prefix_chars_def)
	: units_(reinterpret_cast<const unsigned char *>(unit_def)),
	  prefix_chars_(reinterpret_cast<const unsigned char *>(prefix_chars_def)) {
    }
    const char *parse(const char *begin, const char *end, int &power, int &factor) const;
  private:
    const unsigned char *units_;
    const unsigned char *prefix_chars_;
    void check_units();
};


/** @class BandwidthArg
  @brief Parser class for bandwidth specifications.

  Handles suffixes such as "Gbps", "k", etc. */
class BandwidthArg : public NumArg { public:
    bool parse(const String &str, uint32_t &result, const ArgContext & = blank_args);
    static String unparse(uint32_t x);
    int status;
};


#if !CLICK_TOOL
/** @class AnnoArg
  @brief Parser class for annotation specifications. */
class AnnoArg { public:
    AnnoArg(int s)
	: size(s) {
    }
    bool parse(const String &str, int &result, const ArgContext &args = blank_args);
  private:
    int size;
};
#endif


/** @class SecondsArg
  @brief Parser class for seconds and powers thereof.

  The @a d argument is the number of digits of fraction to parse.
  For example, to parse milliseconds, use SecondsArg(3). */
class SecondsArg : public NumArg { public:
    SecondsArg(int d = 0)
	: fraction_digits(d) {
    }
    bool parse_saturating(const String &str, uint32_t &result, const ArgContext &args = blank_args);
    bool parse(const String &str, uint32_t &result, const ArgContext &args = blank_args);
#if HAVE_FLOAT_TYPES
    bool parse(const String &str, double &result, const ArgContext &args = blank_args);
#endif
    int fraction_digits;
    int status;
};


/** @class AnyArg
  @brief Parser class that accepts any argument. */
class AnyArg { public:
    static bool parse(const String &, const ArgContext & = blank_args) {
	return true;
    }
    static bool parse(const String &str, String &result, const ArgContext & = blank_args) {
	result = str;
	return true;
    }
    static bool parse(const String &str, const ArgContext &, Vector<String> &result) {
	result.push_back(str);
	return true;
    }
};


bool cp_string(const String &str, String *result, String *rest);

/** @class StringArg
  @brief Parser class for possibly-quoted strings. */
class StringArg { public:
    static bool parse(const String &str, String &result, const ArgContext & = blank_args) {
	return cp_string(str, &result, 0);
    }
};

template<> struct DefaultArg<String> : public StringArg {};


bool cp_keyword(const String &str, String *result, String *rest);

/** @class KeywordArg
  @brief Parser class for keywords. */
class KeywordArg { public:
    static bool parse(const String &str, String &result, const ArgContext & = blank_args) {
	return cp_keyword(str, &result, 0);
    }
};


bool cp_word(const String &str, String *result, String *rest);

/** @class KeywordArg
  @brief Parser class for words. */
class WordArg { public:
    static bool parse(const String &str, String &result, const ArgContext & = blank_args) {
	return cp_word(str, &result, 0);
    }
};


#if CLICK_USERLEVEL || CLICK_TOOL
/** @class FilenameArg
  @brief Parser class for filenames. */
class FilenameArg { public:
    static bool parse(const String &str, String &result, const ArgContext &args = blank_args);
};
#endif


#if !CLICK_TOOL
/** @class ElementArg
  @brief Parser class for elements. */
class ElementArg { public:
    static bool parse(const String &str, Element *&result, const ArgContext &args);
};

template<> struct DefaultArg<Element *> : public ElementArg {};

/** @class ElementCastArg
  @brief Parser class for elements of type @a t. */
class ElementCastArg { public:
    ElementCastArg(const char *t)
	: type(t) {
    }
    bool parse(const String &str, Element *&result, const ArgContext &args);
    template<typename T> bool parse(const String &str, T *&result, const ArgContext &args) {
	return parse(str, reinterpret_cast<Element *&>(result), args);
    }
    const char *type;
};
#endif

CLICK_ENDDECLS
#endif
