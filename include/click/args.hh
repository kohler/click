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


/** @cond never */
template <typename T> struct Args_has_enable_direct_parse {
  private:
    template <typename X> static char test(typename X::enable_direct_parse *);
    template <typename> static int test(...);
  public:
    enum { value = (sizeof(test<T>(0)) == 1) };
};
template <typename P, bool direct = Args_has_enable_direct_parse<P>::value>
struct Args_parse_helper;
template <typename P> struct Args_parse_helper<P, false> {
    template <typename T, typename A>
    static inline T *slot(T &variable, A &args) {
	return args.slot(variable);
    }
    template <typename T, typename A>
    static inline T *initialized_slot(T &variable, A &args) {
	return args.initialized_slot(variable);
    }
    template <typename T, typename A>
    static inline bool parse(P parser, const String &str, T &s, A &args) {
	return parser.parse(str, s, args);
    }
    template <typename T1, typename T2, typename A>
    static inline bool parse(P parser, const String &str, T1 &s1, T2 &s2, A &args) {
	return parser.parse(str, s1, s2, args);
    }
};
template <typename P> struct Args_parse_helper<P, true> {
    template <typename T, typename A>
    static inline T *slot(T &variable, A &) {
	return &variable;
    }
    template <typename T, typename A>
    static inline T *initialized_slot(T &variable, A &) {
	return &variable;
    }
    template <typename T, typename A>
    static inline bool parse(P parser, const String &str, T &s, A &args) {
	return parser.direct_parse(str, s, args);
    }
    template <typename T1, typename T2, typename A>
    static inline bool parse(P parser, const String &str, T1 &s1, T2 &s2, A &args) {
	return parser.direct_parse(str, s1, s2, args);
    }
};
/** @endcond never */


/** @class Args
  @brief Argument parser class.

  Args parses Click configuration strings in a type-safe manner.

  Args manages <em>arguments</em> and <em>result slots</em>. Arguments are
  strings to be parsed and result slots are parsed values.

  The read() functions parse arguments into result slots.

  @code
  Args args;
  args.push_back("A 1"); // add argument

  int a_result;
  args.read("A", a_result); // parse "A" into a result slot
  @endcode

  As arguments are parsed, Args marks them off and adds new result slots.
  Each result slot is paired with a variable belonging to the caller.
  However, the caller's variables aren't modified until the parse
  <em>executes</em> via complete(), consume(), or execute().

  @code
  Args args; args.push_back("A 1");

  int a_result = 0;
  args.read("A", a_result);
  assert(a_result == 0);  // parsed value not yet assigned
  args.execute();         // this call assigns results
  assert(a_result == 1);
  @endcode

  If Args encounters a parse error, then execution doesn't modify
  <em>any</em> of the caller's variables.

  @code
  Args args; args.push_back("A 1, B NOT_AN_INTEGER");

  int a_result = 0, b_result = 0;
  args.read("A", a_result)   // succeeds
      .read("B", b_result)   // fails, since B is not an integer
      .execute();
  assert(a_result == 0 && b_result == 0);
  @endcode

  Each read() function comes in five variants. read() reads an optional
  keyword argument. read_m() reads a mandatory keyword argument: if the
  argument was not supplied, Args will report a parse error. read_p() reads
  an optional positional argument. If the keyword was not supplied, but a
  positional argument was, that is used. read_mp() reads a mandatory
  positional argument. Positional arguments are parsed in order. The fifth
  variant of read() takes an integer <em>flags</em> argument; flags include
  Args::positional, Args::mandatory, and others, such as Args::deprecated.

  The complete() execution method checks that every argument has been
  successfully parsed and reports an error if not.  consume()
  doesn't check for completion, but removes parsed arguments from the
  argument set.  Execution methods return 0 on success and <0 on failure.
  You can check the parse status before execution using status().

  Args methods are designed to chain. All read() methods (and some others)
  return a reference to the Args itself. It is often possible to parse a
  whole set of arguments using a single temporary Args. For example:

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

  The actual work of parsing is handled by <em>parser objects</em>. Many
  common variable types have default parsers defined by the DefaultArg<T>
  template. For example, the default parser for an integer value understands
  the common textual representations of integers. You can also pass a parser
  explicitly. For example:

  @code
  int a, b, c;
  args.read("A", a)      // parse A using DefaultArg<int> = IntArg()
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

  Most parsers are <em>disciplined</em>, meaning that they modify
  <b>result</b> only if the parse succeeds. This doesn't matter in the
  context of Args, but can matter to users who call a parse function
  directly.
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
     * @post This Args shares @a conf with the caller.
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
     * @param x reference to result
     * @return *this
     *
     * Creates a result slot for @a x and calls
     * DefaultArg<T>().parse(string, result, *this). */
    template <typename T>
    Args &read(const char *keyword, T &x) {
	return read(keyword, 0, x);
    }
    template <typename T>
    Args &read_m(const char *keyword, T &x) {
	return read(keyword, mandatory, x);
    }
    template <typename T>
    Args &read_p(const char *keyword, T &x) {
	return read(keyword, positional, x);
    }
    template <typename T>
    Args &read_mp(const char *keyword, T &x) {
	return read(keyword, mandatory | positional, x);
    }
    template <typename T>
    Args &read(const char *keyword, int flags, T &x) {
	args_base_read(this, keyword, flags, x);
	return *this;
    }

    /** @brief Read an argument using the default parser, or set it to a
     *    default value if the argument is was not supplied.
     * @param keyword argument name
     * @param x reference to result
     * @param default_value default value
     * @return *this
     *
     * Creates a result slot for @a x. If @a keyword was supplied, calls
     * DefaultArg<T>().parse(string, result, this). Otherwise, assigns the
     * result to @a value. */
    template <typename T, typename V>
    Args &read_or_set(const char *keyword, T &x, const V &default_value) {
	return read_or_set(keyword, 0, x, default_value);
    }
    template <typename T, typename V>
    Args &read_or_set_p(const char *keyword, T &x, const V &default_value) {
	return read_or_set(keyword, positional, x, default_value);
    }
    template <typename T, typename V>
    Args &read_or_set(const char *keyword, int flags, T &x, const V &default_value) {
	args_base_read_or_set(this, keyword, flags, x, default_value);
	return *this;
    }

    /** @brief Read an argument using a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @param x reference to result
     * @return *this
     *
     * Creates a result slot for @a x and calls @a parser.parse(string,
     * result, *this). */
    template <typename P, typename T>
    Args &read(const char *keyword, P parser, T &x) {
	return read(keyword, 0, parser, x);
    }
    template <typename P, typename T>
    Args &read_m(const char *keyword, P parser, T &x) {
	return read(keyword, mandatory, parser, x);
    }
    template <typename P, typename T>
    Args &read_p(const char *keyword, P parser, T &x) {
	return read(keyword, positional, parser, x);
    }
    template <typename P, typename T>
    Args &read_mp(const char *keyword, P parser, T &x) {
	return read(keyword, mandatory | positional, parser, x);
    }
    template <typename P, typename T>
    Args &read(const char *keyword, int flags, P parser, T &x) {
	args_base_read(this, keyword, flags, parser, x);
	return *this;
    }

    /** @brief Read an argument using a specified parser, or set it to a
     *    default value if the argument is was not supplied.
     * @param keyword argument name
     * @param parser parser object
     * @param x reference to result variable
     * @param default_value default value
     * @return *this
     *
     * Creates a result slot for @a x. If argument @a keyword was supplied,
     * calls @a parser.parse(string, result, *this). Otherwise, assigns the
     * result to @a default_value. */
    template <typename P, typename T, typename V>
    Args &read_or_set(const char *keyword, P parser, T &x, const V &default_value) {
	return read_or_set(keyword, 0, parser, x, default_value);
    }
    template <typename P, typename T, typename V>
    Args &read_or_set_p(const char *keyword, P parser, T &x, const V &default_value) {
	return read_or_set(keyword, positional, parser, x, default_value);
    }
    template <typename P, typename T, typename V>
    Args &read_or_set(const char *keyword, int flags, P parser, T &x, const V &default_value) {
	args_base_read_or_set(this, keyword, flags, parser, x, default_value);
	return *this;
    }

    /** @brief Read an argument using a specified parser with two results.
     * @param keyword argument name
     * @param parser parser object
     * @param x1 reference to first result
     * @param x2 reference to second result
     * @return *this
     *
     * Creates results for @a x1 and @a x2 and calls @a parser.parse(string,
     * result1, result2, *this). */
    template<typename P, typename T1, typename T2>
    Args &read(const char *keyword, P parser, T1 &x1, T2 &x2) {
	return read(keyword, 0, parser, x1, x2);
    }
    template<typename P, typename T1, typename T2>
    Args &read_m(const char *keyword, P parser, T1 &x1, T2 &x2) {
	return read(keyword, mandatory, parser, x1, x2);
    }
    template<typename P, typename T1, typename T2>
    Args &read_p(const char *keyword, P parser, T1 &x1, T2 &x2) {
	return read(keyword, positional, parser, x1, x2);
    }
    template<typename P, typename T1, typename T2>
    Args &read_mp(const char *keyword, P parser, T1 &x1, T2 &x2) {
	return read(keyword, mandatory | positional, parser, x1, x2);
    }
    template<typename P, typename T1, typename T2>
    Args &read(const char *keyword, int flags, P parser, T1 &x1, T2 &x2) {
	args_base_read(this, keyword, flags, parser, x1, x2);
	return *this;
    }

    /** @brief Pass an argument to a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @return *this
     *
     * Calls @a parser.parse(string, *this). */
    template <typename P>
    Args &read_with(const char *keyword, P parser) {
	return read_with(keyword, 0, parser);
    }
    template <typename P>
    Args &read_m_with(const char *keyword, P parser) {
	return read_with(keyword, mandatory, parser);
    }
    template <typename P>
    Args &read_p_with(const char *keyword, P parser) {
	return read_with(keyword, positional, parser);
    }
    template <typename P>
    Args &read_mp_with(const char *keyword, P parser) {
	return read_with(keyword, mandatory | positional, parser);
    }
    template <typename P>
    Args &read_with(const char *keyword, int flags, P parser) {
	args_base_read_with(this, keyword, flags, parser);
	return *this;
    }

    /** @brief Pass an argument to a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @param x reference to result
     * @return *this
     *
     * Creates a result slot for @a x and calls @a parser.parse(string,
     * result, *this).
     *
     * @deprecated Use read(keyword, parser, variable) instead. */
    template <typename P, typename T>
    Args &read_with(const char *keyword, P parser, T &x) {
	return read(keyword, parser, x);
    }
    template <typename P, typename T>
    Args &read_m_with(const char *keyword, P parser, T &x) {
	return read_m(keyword, parser, x);
    }
    template <typename P, typename T>
    Args &read_p_with(const char *keyword, P parser, T &x) {
	return read_p(keyword, parser, x);
    }
    template <typename P, typename T>
    Args &read_mp_with(const char *keyword, P parser, T &x) {
	return read_mp(keyword, parser, x);
    }
    template <typename P, typename T>
    Args &read_with(const char *keyword, int flags, P parser, T &x) {
	return read(keyword, flags, parser, x);
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
    template <typename P>
    Args &read_all_with(const char *keyword, P parser) {
	return read_all_with(keyword, 0, parser);
    }
    template <typename P>
    Args &read_all_with(const char *keyword, int flags, P parser) {
	args_base_read_all_with(this, keyword, flags | firstmatch, parser);
	return *this;
    }

    /** @brief Pass all matching arguments to a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @param x reference to result
     * @return *this
     *
     * Creates a result for @a x and calls @a parser.parse(string, result,
     * *this) zero or more times, once per matching argument.
     *
     * @note The value of read_status() is true iff at least one argument
     * matched and all matching arguments successfully parsed. */
    template <typename P, typename T>
    Args &read_all_with(const char *keyword, P parser, T &x) {
	return read_all_with(keyword, 0, parser, x);
    }
    template <typename P, typename T>
    Args &read_all_with(const char *keyword, int flags, P parser, T &x) {
	args_base_read_all_with(this, keyword, flags | firstmatch, parser, x);
	return *this;
    }

    /** @brief Pass all matching arguments to a specified parser.
     * @param keyword argument name
     * @param parser parser object
     * @param x reference to vector of results
     * @return *this
     *
     * For each @a keyword argument, calls @a parser.parse(string, value,
     * *this). The resulting values are collected into a vector result slot
     * for @a x.
     *
     * @note The value of read_status() is true iff at least one argument
     * matched and all matching arguments successfully parsed. */
    template <typename P, typename T>
    Args &read_all(const char *keyword, P parser, Vector<T> &x) {
	return read_all(keyword, 0, parser, x);
    }
    template <typename T>
    Args &read_all(const char *keyword, Vector<T> &x) {
	return read_all(keyword, 0, DefaultArg<T>(), x);
    }
    template <typename P, typename T>
    Args &read_all(const char *keyword, int flags, P parser, Vector<T> &x) {
	args_base_read_all(this, keyword, flags | firstmatch, parser, x);
	return *this;
    }
    template <typename P, typename T>
    Args &read_all(const char *keyword, int flags, Vector<T> &x) {
	return read_all(keyword, flags, DefaultArg<T>(), x);
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


    /** @brief Create and return a result slot for @a x.
     *
     * If T is a trivially copyable type, such as int, then the resulting
     * slot might not be initialized. */
    template <typename T>
    T *slot(T &x) {
	if (has_trivial_copy<T>::value)
	    return reinterpret_cast<T *>(simple_slot(&x, sizeof(T)));
	else
	    return complex_slot(x);
    }

    /** @brief Create and return a result slot for @a x.
     *
     * The resulting slot is always default-initialized. */
    template <typename T>
    T *initialized_slot(T &x) {
	T *s = slot(x);
	if (has_trivial_copy<T>::value)
	    *s = T();
	return s;
    }

    /** @brief Add a result that assigns @a x to @a value.
     * @return *this */
    template <typename T, typename V>
    Args &set(T &x, const V &value) {
	if (T *s = slot(x))
	    *s = value;
	return *this;
    }


    /** @cond never */
    template<typename T>
    void base_read(const char *keyword, int flags, T &variable) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status)) {
	    T *s = Args_parse_helper<DefaultArg<T> >::slot(variable, *this);
	    postparse(s && Args_parse_helper<DefaultArg<T> >::parse(DefaultArg<T>(), str, *s, *this), slot_status);
	}
    }

    template<typename T, typename V>
    void base_read_or_set(const char *keyword, int flags, T &variable, const V &value) {
	Slot *slot_status;
	String str = find(keyword, flags, slot_status);
	T *s = Args_parse_helper<DefaultArg<T> >::slot(variable, *this);
	postparse(s && (str ? Args_parse_helper<DefaultArg<T> >::parse(DefaultArg<T>(), str, *s, *this) : (*s = value, true)), slot_status);
    }

    template<typename P, typename T>
    void base_read(const char *keyword, int flags, P parser, T &variable) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status)) {
	    T *s = Args_parse_helper<P>::slot(variable, *this);
	    postparse(s && Args_parse_helper<P>::parse(parser, str, *s, *this), slot_status);
	}
    }

    template<typename T, typename P, typename V>
    void base_read_or_set(const char *keyword, int flags, P parser, T &variable, const V &value) {
	Slot *slot_status;
	String str = find(keyword, flags, slot_status);
	T *s = Args_parse_helper<P>::slot(variable, *this);
	postparse(s && (str ? Args_parse_helper<P>::parse(parser, str, *s, *this) : (*s = value, true)), slot_status);
    }

    template<typename P, typename T1, typename T2>
    void base_read(const char *keyword, int flags,
		   P parser, T1 &variable1, T2 &variable2) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status)) {
	    T1 *s1 = Args_parse_helper<P>::slot(variable1, *this);
	    T2 *s2 = Args_parse_helper<P>::slot(variable2, *this);
	    postparse(s1 && s2 && Args_parse_helper<P>::parse(parser, str, *s1, *s2, *this), slot_status);
	}
    }

    template<typename P>
    void base_read_with(const char *keyword, int flags, P parser) {
	Slot *slot_status;
	if (String str = find(keyword, flags, slot_status))
	    postparse(parser.parse(str, *this), slot_status);
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
	T *s = Args_parse_helper<P>::initialized_slot(variable, *this);
	while (String str = find(keyword, flags, slot_status)) {
	    postparse(s && Args_parse_helper<P>::parse(parser, str, *s, *this), slot_status);
	    read_status = (read_status != 0) && _read_status;
	    flags &= ~mandatory;
	}
	_read_status = (read_status == 1);
    }

    template<typename P, typename T>
    void base_read_all(const char *keyword, int flags, P parser, Vector<T> &variable) {
	Slot *slot_status;
	int read_status = -1;
	Vector<T> *s = slot(variable);
	while (String str = find(keyword, flags, slot_status)) {
	    T sx;
	    postparse(parser.parse(str, sx, *this), slot_status);
	    if (_read_status)
		s->push_back(sx);
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

template <typename P, typename T>
void args_base_read_all(Args *args, const char *keyword, int flags,
			P parser, Vector<T> &variable) CLICK_NOINLINE;
template <typename P, typename T>
void args_base_read_all(Args *args, const char *keyword, int flags,
			P parser, Vector<T> &variable)
{
    args->base_read_all(keyword, flags, parser, variable);
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
	    range_error(args, integer_traits<V>::is_signed,
			click_int_large_t(x));
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
    void range_error(const ArgContext &args, bool is_signed,
		     click_int_large_t value);

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
    template <typename T>
    BoundedIntArg(T min_value, T max_value, int b = 0)
	: IntArg(b), min_value(min_value), max_value(max_value) {
	static_assert(integer_traits<T>::is_integral, "BoundedIntArg argument must be integral");
	is_signed = integer_traits<T>::is_signed;
    }

    template <typename V>
    bool parse(const String &str, V &result, const ArgContext &args = blank_args) {
	V x;
	if (!IntArg::parse(str, x, args))
	    return false;
	else if (!check_min(typename integer_traits<V>::max_type(x))) {
	    range_error(args, is_signed, min_value);
	    return false;
	} else if (!check_max(typename integer_traits<V>::max_type(x))) {
	    range_error(args, is_signed, max_value);
	    return false;
	} else {
	    result = x;
	    return true;
	}
    }

    inline bool check_min(click_int_large_t x) const {
	if (is_signed)
	    return x >= min_value;
	else
	    return x >= 0 && click_uint_large_t(x) >= click_uint_large_t(min_value);
    }
    inline bool check_min(click_uint_large_t x) const {
	if (is_signed)
	    return min_value < 0 || x >= click_uint_large_t(min_value);
	else
	    return click_uint_large_t(x) >= click_uint_large_t(min_value);
    }
    inline bool check_max(click_int_large_t x) const {
	if (is_signed)
	    return x <= max_value;
	else
	    return x >= 0 && click_uint_large_t(x) <= click_uint_large_t(max_value);
    }
    inline bool check_max(click_uint_large_t x) const {
	if (is_signed)
	    return max_value >= 0 && x <= click_uint_large_t(max_value);
	else
	    return click_uint_large_t(x) <= click_uint_large_t(max_value);
    }

    click_intmax_t min_value;
    click_intmax_t max_value;
    bool is_signed;
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
    static bool parse(const String &str, Vector<String> &result, const ArgContext & = blank_args) {
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
