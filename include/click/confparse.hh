// -*- c-basic-offset: 4; related-file-name: "../../lib/confparse.cc" -*-
#ifndef CLICK_CONFPARSE_HH
#define CLICK_CONFPARSE_HH
/// @cond never
#include <click/string.hh>
#include <click/vector.hh>
struct in_addr;
#if HAVE_IP6
struct in6_addr;
#endif
CLICK_DECLS
class ErrorHandler;
class StringAccum;
class Timestamp;
#ifndef CLICK_TOOL
class Element;
class Router;
class Handler;
class HandlerCall;
# define CP_VA_PARSE_ARGS_REST	const Element*, ErrorHandler*, ...
# define CP_OPT_CONTEXT		, const Element* context = 0
# define CP_CONTEXT		, const Element* context
# define CP_PASS_CONTEXT	, context
#else
# define CP_VA_PARSE_ARGS_REST	ErrorHandler*, ...
# define CP_OPT_CONTEXT
# define CP_CONTEXT
# define CP_PASS_CONTEXT
#endif
#ifndef CLICK_COMPILING_CONFPARSE_CC
# define CLICK_CONFPARSE_DEPRECATED CLICK_DEPRECATED
#else
# define CLICK_CONFPARSE_DEPRECATED /* */
#endif
#if __GNUC__ <= 3
# define CP_SENTINEL
#else
# define CP_SENTINEL __attribute__((sentinel))
#endif
/// @endcond

/// @name Argument Manipulation
//@{
const char* cp_skip_space(const char* begin, const char* end);
const char* cp_skip_comment_space(const char* begin, const char* end);
const char* cp_skip_double_quote(const char* begin, const char* end);
bool cp_eat_space(String &str);
inline bool cp_is_space(const String &str);
bool cp_is_word(const String &str);
bool cp_is_click_id(const String &str);

String cp_uncomment(const String &str);
String cp_unquote(const String &str);
const char* cp_process_backslash(const char* begin, const char* end, StringAccum &sa);
String cp_quote(const String &str, bool allow_newlines = false);

void cp_argvec(const String& str, Vector<String>& conf);
String cp_unargvec(const Vector<String>& conf);

void cp_spacevec(const String& str, Vector<String>& conf);

/// @brief  Remove and return the first space-separated argument from @a str.
/// @param[in,out]  str  space-separated configuration string
///
/// The first space-separated argument in the configuration string is removed
/// and returned.  The returned argument is passed through cp_uncomment().  @a
/// str is set to the remaining portion of the string, with any preceding
/// spaces and comments removed.  If the input string is all spaces and
/// comments, then both the returned string and @a str will be empty.
String cp_shift_spacevec(String &str);

String cp_unspacevec(const String *begin, const String *end);
inline String cp_unspacevec(const Vector<String> &conf);

/// @brief  Remove and return the first delimiter-separated argument from @a str.
/// @param[in,out]  str  delimiter-separated string
/// @param[in]  delim  delimiter
///
/// All characters up to the first delimiter character are removed and
/// returned.  @a str is set to the remaining portion of the string,
/// with up to one delimiter character removed.
String cp_shift_delimiter(String &str, char delim);

//@}

/// @name Direct Parsing Functions
//@{
enum CpErrors {
    CPE_OK = 0,
    CPE_FORMAT,
    CPE_NEGATIVE,
    CPE_OVERFLOW,
    CPE_INVALID,
    CPE_MEMORY,
    CPE_NOUNITS
};
extern int cp_errno;

// strings and words
bool cp_string(const String& str, String* result, String *rest = 0);
bool cp_word(const String& str, String* result, String *rest = 0);
bool cp_keyword(const String& str, String* result, String *rest = 0);

bool cp_bool(const String& str, bool* result);

// numbers
enum { cp_basic_integer_whole = 64 };
const char *cp_basic_integer(const char *begin, const char *end, int flags, int size, void *result);

inline const char *cp_integer(const char *begin, const char *end, int base, int *result);
inline const char *cp_integer(const char *begin, const char *end, int base, unsigned *result);
inline const char *cp_integer(const char *begin, const char *end, int base, long *result);
inline const char *cp_integer(const char *begin, const char *end, int base, unsigned long *result);
/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, unsigned *result);
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, unsigned long *result);
/// @endcond
#if HAVE_LONG_LONG
inline const char *cp_integer(const char *begin, const char *end, int base, long long *result);
inline const char *cp_integer(const char *begin, const char *end, int base, unsigned long long *result);
/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, unsigned long long *result);
/// @endcond
#elif HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG
inline const char *cp_integer(const char *begin, const char *end, int base, int64_t *result);
inline const char *cp_integer(const char *begin, const char *end, int base, uint64_t *result);
/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, uint64_t *result);
/// @endcond
#endif

inline bool cp_integer(const String &str, int base, int *result);
inline bool cp_integer(const String &str, int base, unsigned int *result);
inline bool cp_integer(const String &str, int base, long *result);
inline bool cp_integer(const String &str, int base, unsigned long *result);
#if HAVE_LONG_LONG
inline bool cp_integer(const String &str, int base, long long *result);
inline bool cp_integer(const String &str, int base, unsigned long long *result);
#elif HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG
inline bool cp_integer(const String &str, int base, int64_t *result);
inline bool cp_integer(const String &str, int base, uint64_t *result);
#endif

inline bool cp_integer(const String &str, int *result);
inline bool cp_integer(const String &str, unsigned int *result);
inline bool cp_integer(const String &str, long *result);
inline bool cp_integer(const String &str, unsigned long *result);
#if HAVE_LONG_LONG
inline bool cp_integer(const String &str, long long *result);
inline bool cp_integer(const String &str, unsigned long long *result);
#elif HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG
inline bool cp_integer(const String &str, int64_t *result);
inline bool cp_integer(const String &str, uint64_t *result);
#endif

#define CP_REAL2_MAX_FRAC_BITS 28
bool cp_real2(const String& str, int frac_bits, int32_t* result);
bool cp_real2(const String& str, int frac_bits, uint32_t* result);
bool cp_real10(const String& str, int frac_digits, int32_t* result);
bool cp_real10(const String& str, int frac_digits, uint32_t* result);
bool cp_real10(const String& str, int frac_digits, uint32_t* result_int, uint32_t* result_frac);
#if HAVE_FLOAT_TYPES
bool cp_double(const String& str, double* result);
#endif

bool cp_seconds_as(const String& str, int frac_digits, uint32_t* result);
bool cp_seconds_as_milli(const String& str, uint32_t* result);
bool cp_seconds_as_micro(const String& str, uint32_t* result);
#if HAVE_FLOAT_TYPES
bool cp_seconds(const String& str, double* result);
#endif
bool cp_time(const String &str, Timestamp *result, bool allow_negative = false);
bool cp_time(const String& str, struct timeval* result);

bool cp_bandwidth(const String& str, uint32_t* result);

// network addresses
class IPAddressList;
bool cp_ip_address(const String& str, IPAddress* result  CP_OPT_CONTEXT);
inline bool cp_ip_address(const String& str, struct in_addr* result  CP_OPT_CONTEXT);
bool cp_ip_address(const String& str, unsigned char* result  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String& str, IPAddress* result_addr, IPAddress* result_mask, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String& str, unsigned char* result_addr, unsigned char* result_mask, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String& str, IPAddress* result_addr, IPAddress* result_mask  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String& str, unsigned char* result_addr, unsigned char* result_mask  CP_OPT_CONTEXT);
bool cp_ip_address_list(const String& str, Vector<IPAddress>* result  CP_OPT_CONTEXT);

#if HAVE_IP6
class IP6Address;
bool cp_ip6_address(const String& str, IP6Address* result  CP_OPT_CONTEXT);
inline bool cp_ip6_address(const String& str, struct in6_addr* result  CP_OPT_CONTEXT);
bool cp_ip6_address(const String& str, unsigned char* result  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String& str, IP6Address* result_addr, int* result_prefix, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String& str, unsigned char* result_addr, int* result_prefix, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String& str, unsigned char* result_addr, unsigned char* result_mask, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String& str, IP6Address* result_addr, IP6Address* result_mask, bool allow_bare_address  CP_OPT_CONTEXT);
#endif

class EtherAddress;
bool cp_ethernet_address(const String& str, EtherAddress* result  CP_OPT_CONTEXT);
bool cp_ethernet_address(const String& str, unsigned char* result  CP_OPT_CONTEXT);

bool cp_tcpudp_port(const String& str, int proto, uint16_t* result  CP_OPT_CONTEXT);

#if !CLICK_TOOL
Element *cp_element(const String &str, const Element *context, ErrorHandler *errh = 0, const char *argname = 0);
Element *cp_element(const String &str, Router *router, ErrorHandler *errh = 0, const char *argname = 0);
bool cp_handler_name(const String& str, Element** result_element, String* result_hname, const Element* context, ErrorHandler* errh=0);
bool cp_handler(const String& str, int flags, Element** result_element, const Handler** result_handler, const Element* context, ErrorHandler* errh=0);
#endif

#if CLICK_USERLEVEL || CLICK_TOOL
bool cp_filename(const String& str, String* result);
bool cp_file_offset(const String& str, off_t* result);
#endif

#if !CLICK_TOOL
bool cp_anno(const String &str, int size, int *result  CP_OPT_CONTEXT);
#endif
//@}

/// @name cp_va_kparse
//@{
int cp_va_kparse(const Vector<String>& conf, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;
int cp_va_kparse(const String& str, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;
int cp_va_space_kparse(const String& str, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;
int cp_va_kparse_keyword(const String& str, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;
int cp_va_kparse_remove_keywords(Vector<String>& conf, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;

int cp_assign_arguments(const Vector<String> &argv, const String *param_begin, const String *param_end, Vector<String>* values = 0);

void cp_va_static_initialize();
void cp_va_static_cleanup();

/// @brief Type of flags for cp_va_kparse() items.
enum CpKparseFlags {
    cpkN = 0,		///< Default flags
    cpkM = 1,		///< Argument is mandatory
    cpkP = 2,		///< Argument may be specified positionally
    cpkC = 4,		///< Argument presence should be confirmed
    cpkD = 8,		///< Argument is deprecated
    cpkNormal = cpkN,
    cpkMandatory = cpkM,
    cpkPositional = cpkP,
    cpkConfirm = cpkC,
    cpkDeprecated = cpkD
};

/// @brief Type of argument type names for cp_va_kparse() items.
typedef const char *CpVaParseCmd;

extern const CpVaParseCmd
    cpEnd,		///< Use as argument name.  Ends cp_va argument list.
    cpIgnoreRest,	///< Use as argument name.  No result storage; causes cp_va_kparse() to ignore unparsed arguments and any remaining items.
    cpIgnore,		///< No result storage, ignores this argument.
    cpArgument,		///< Result storage String*, accepts any argument.
    cpArguments,	///< Result storage Vector<String>*, accepts any number of arguments with the same keyword.
    cpString,		///< Result storage String*, parsed by cp_string().
    cpWord,		///< Result storage String*, parsed by cp_word().
    cpKeyword,		///< Result storage String*, parsed by cp_keyword().
    cpBool,		///< Result storage bool*, parsed by cp_bool().
    cpByte,		///< Result storage unsigned char*, parsed by cp_integer().
    cpShort,		///< Result storage short*, parsed by cp_integer().
    cpUnsignedShort,	///< Result storage unsigned short*, parsed by cp_integer().
    cpInteger,		///< Result storage int32_t*, parsed by cp_integer().
    cpUnsigned,		///< Result storage uint32_t*, parsed by cp_integer().
    cpSize,		///< Result storage size_t*, parsed by cp_integer().
    cpNamedInteger,	///< Parse parameter uint32_t nameinfo_type, result storage int32_t*, parsed by NameInfo::query_int.
#if HAVE_INT64_TYPES
    cpInteger64,	///< Result storage int64_t*, parsed by cp_integer().
    cpUnsigned64,	///< Result storage uint64_t*, parsed by cp_integer().
#endif
    cpUnsignedReal2,	///< Parse parameter int frac_bits, result storage uint32_t*, parsed by cp_real2().
    cpReal10,		///< Parse parameter int frac_digits, result storage int32_t*, parsed by cp_real10().
    cpUnsignedReal10,	///< Parse parameter int frac_digits, result storage uint32_t*, parsed by cp_real10().
#if HAVE_FLOAT_TYPES
    cpDouble,		///< Result storage double*, parsed by cp_double().
#endif
    cpSeconds,		///< Result storage uint32_t*, parsed by cp_seconds_as() with frac_digits 0.
    cpSecondsAsMilli,	///< Result storage uint32_t*, parsed by cp_seconds_as_milli().
    cpSecondsAsMicro,	///< Result storage uint32_t*, parsed by cp_seconds_as_micro().
    cpTimestamp,	///< Result storage Timestamp*, parsed by cp_time().
    cpTimestampSigned,	///< Result storage Timestamp*, parsed by cp_time().
    cpTimeval,		///< Result storage struct timeval*, parsed by cp_time().
    cpBandwidth,	///< Result storage uint32_t*, parsed by cp_bandwidth().
    cpIPAddress,	///< Result storage IPAddress* or equivalent, parsed by cp_ip_address().
    cpIPPrefix,		///< Result storage IPAddress* addr and IPAddress *mask, parsed by cp_ip_prefix().
    cpIPAddressOrPrefix,///< Result storage IPAddress* addr and IPAddress *mask, parsed by cp_ip_prefix().
    cpIPAddressList,	///< Result storage Vector<IPAddress>*, parsed by cp_ip_address_list().
    cpEtherAddress,	///< Result storage EtherAddress*, parsed by cp_ethernet_address().
    cpEthernetAddress,	///< Result storage EtherAddress*, parsed by cp_ethernet_address().  Synonym for cpEtherAddress.
    cpTCPPort,		///< Result storage uint16_t*, parsed by cp_tcpudp_port().
    cpUDPPort,		///< Result storage uint16_t*, parsed by cp_tcpudp_port().
    cpElement,		///< Result storage Element**, parsed by cp_element().
    cpElementCast,	///< Parse parameter const char*, result storage void**, parsed by cp_element() and Element::cast().
    cpHandlerName,	///< Result storage Element** and String*, parsed by cp_handler_name().
    cpHandlerCallRead,	///< Result storage HandlerCall*, parsed by HandlerCall.
    cpHandlerCallWrite,	///< Result storage HandlerCall*, parsed by HandlerCall.
    cpHandlerCallPtrRead, ///< Result storage HandlerCall**, parsed by HandlerCall::reset_read.
    cpHandlerCallPtrWrite, ///< Result storage HandlerCall**, parsed by HandlerCall::reset_write.
    cpIP6Address,	///< Result storage IP6Address* or equivalent, parsed by cp_ip6_address().
    cpIP6Prefix,	///< Result storage IP6Address* addr and IP6Address* mask, parsed by cp_ip6_prefix().
    cpIP6PrefixLen,	///< Result storage IP6Address* addr and int* prefix_len, parsed by cp_ip6_prefix().
    cpIP6AddressOrPrefix,///< Result storage IP6Address* addr and IP6Address* mask, parsed by cp_ip6_prefix().
#if CLICK_USERLEVEL || CLICK_TOOL
    cpFilename,		///< Result storage String*, parsed by cp_filename().
    cpFileOffset,	///< Result storage off_t*, parsed by cp_integer().
#endif
#if !CLICK_TOOL
    cpAnno,		///< Parse parameter int annotation_size, result storage int*, parsed by cp_anno().
#endif
    cpOptional,		///< cp_va_parse only: Following arguments are optional.
    cpKeywords,		///< cp_va_parse only: Following arguments are keywords.
    cpConfirmKeywords,	///< cp_va_parse only: Following arguments are confirmed keywords.
    cpMandatoryKeywords;///< cp_va_parse only: Following arguments are mandatory keywords.
// old names, here for compatibility:
extern const CpVaParseCmd
    cpInterval CLICK_CONFPARSE_DEPRECATED,		// struct timeval*
    cpReadHandlerCall CLICK_CONFPARSE_DEPRECATED,	// HandlerCall**
    cpWriteHandlerCall CLICK_CONFPARSE_DEPRECATED;	// HandlerCall**
//@}

/// @name Unparsing
//@{
String cp_unparse_bool(bool value);
String cp_unparse_real2(int32_t value, int frac_bits);
String cp_unparse_real2(uint32_t value, int frac_bits);
#if HAVE_INT64_TYPES
String cp_unparse_real2(int64_t value, int frac_bits);
String cp_unparse_real2(uint64_t value, int frac_bits);
#endif
String cp_unparse_real10(int32_t value, int frac_digits);
String cp_unparse_real10(uint32_t value, int frac_digits);
String cp_unparse_milliseconds(uint32_t value);
String cp_unparse_microseconds(uint32_t value);
String cp_unparse_interval(const Timestamp& value) CLICK_DEPRECATED;
String cp_unparse_interval(const struct timeval& value) CLICK_DEPRECATED;
String cp_unparse_bandwidth(uint32_t value);
//@}

/// @name Legacy Functions
//@{
int cp_va_parse(const Vector<String>& conf, CP_VA_PARSE_ARGS_REST) CLICK_DEPRECATED;
int cp_va_parse(const String& str, CP_VA_PARSE_ARGS_REST) CLICK_DEPRECATED;
int cp_va_space_parse(const String& str, CP_VA_PARSE_ARGS_REST) CLICK_DEPRECATED;
int cp_va_parse_keyword(const String& str, CP_VA_PARSE_ARGS_REST) CLICK_DEPRECATED;
int cp_va_parse_remove_keywords(Vector<String>& conf, int first, CP_VA_PARSE_ARGS_REST) CLICK_DEPRECATED;
// Argument syntax:
// cp_va_arg ::= cpEnd		// terminates argument list (not 0!)
//    |   cpOptional | cpKeywords | cpIgnore...		// manipulators
//    |   CpVaParseCmd cmd, const char *description,
//	  [Optional Helpers], Results
//				// Helpers and Results depend on 'cmd';
//				// see table above
//    |   const char *keyword, CpVaParseCmd cmd, const char *description,
//	  [Optional Helpers], Results
//				// keyword argument, within cpKeywords or
//				// cpMandatoryKeywords
//    |   const char *keyword, CpVaParseCmd cmd, const char *description,
//	  bool *keyword_given, [Optional Helpers], Results
//				// keyword argument, within cpConfirmKeywords
// Returns the number of result arguments set, or negative on error.
// Stores no values in the result arguments on error.
//@}

/// @name Argument Types for cp_va_kparse()
//@{
struct cp_value;
struct cp_argtype;

typedef void (*cp_parsefunc)(cp_value* value, const String& arg,
			     ErrorHandler* errh, const char* argdesc  CP_CONTEXT);
typedef void (*cp_storefunc)(cp_value* value  CP_CONTEXT);

struct cp_argtype {
    const char* name;
    cp_argtype* next;
    cp_parsefunc parse;
    cp_storefunc store;
    void* user_data;
    int flags;
    const char* description;
    int internal;
    int use_count;
};

struct cp_value {
    // set by cp_va_parse:
    const cp_argtype* argtype;
    const char* keyword;
    const char* description CLICK_CONFPARSE_DEPRECATED;
    union {
	int i;
	const char *c_str;
	void *p;
    } extra;
    void* store;
    void* store2;
    bool* store_confirm;
    // set by parsefunc, used by storefunc:
    union {
	bool b;
	int i;
	unsigned u;
	int16_t s16;
	uint16_t u16;
	int32_t s32;
	uint32_t u32;
#if HAVE_INT64_TYPES
	int64_t s64;
	int64_t i64 CLICK_DEPRECATED;
	uint64_t u64;
#endif
	size_t size;
#if HAVE_FLOAT_TYPES
	double d;
#endif
	unsigned char address[16];
	int is[4];
#ifndef CLICK_TOOL
	Element *element;
#endif
	void *p;
    } v, v2;
    String v_string;
    String v2_string;
};

enum {
    cpArgNormal = 0, cpArgStore2 = 1, cpArgExtraInt = 2,
    cpArgExtraCStr = 4, cpArgAllowNumbers = 8
};
int cp_register_argtype(const char* name, const char* description, int flags,
			cp_parsefunc parsefunc, cp_storefunc storefunc,
			void* user_data = 0);
void cp_unregister_argtype(const char* name);

int cp_register_stringlist_argtype(const char* name, const char* description,
				   int flags);
int cp_extend_stringlist_argtype(const char* name, ...);
// Takes: const char* name, int value, ...., const char* ender = (const char*)0
//@}

/// @cond never
bool cp_seconds_as(int want_power, const String& str, uint32_t* result) CLICK_DEPRECATED;

#define cp_integer64 cp_integer
#define cp_unsigned64 cp_integer
#define cp_unsigned cp_integer
#define cp_unsigned_real10 cp_real10
#define cp_unsigned_real2 cp_real2
/// @endcond

/** @brief  Join the strings in @a conf with spaces and return the result.
 *
 *  This function does not quote or otherwise protect the strings in @a conf.
 *  The caller should do that if necessary.
 *  @sa cp_unspacevec(const String *, const String *) */
inline String cp_unspacevec(const Vector<String>& conf)
{
    return cp_unspacevec(conf.begin(), conf.end());
}

/** @brief  Test if @a str is all spaces.
 *  @return True when every character in @a str is a space. */
inline bool cp_is_space(const String& str)
{
    return cp_skip_space(str.begin(), str.end()) == str.end();
}

/** @brief  Parse an integer from [@a begin, @a end) in base @a base.
 * @param  begin  first character in string
 * @param  end    one past last character in string
 * @param  base   base of integer: 0 or 2-36
 * @param[out]  result  stores parsed result
 * @return  pointer to first unparsed character in string; equals @a begin
 *	     if the string didn't start with a valid integer
 *
 * This function parses an integer from the initial characters of a string.
 * The resulting integer is stored in *@a result.
 *
 * The integer format consists of an optional initial sign <tt>+/-</tt>,
 * followed by one or more digits.  A negative sign is only accepted if @a
 * result has a signed type.  Digits may be separated by underscores (to make
 * numbers easier to read), but the first and last characters in the integer
 * cannot be underscores, and two underscores can't appear in a row.  Some
 * examples:
 *
 * @code
 * 0
 * 0x100
 * -1_000_023
 * @endcode
 *
 * Digits are numbered from 0-9, then A-Z/a-z.  @a base determines which
 * digits are legal.  If @a base is 0, then a leading <tt>0x</tt> or
 * <tt>0X</tt> may precede the digits, indicating base 16; a leading
 * <tt>0</tt> indicates base 8; anything else is base 10.
 *
 * Returns the first character that can't be parsed as part of the integer.
 * If there is no valid integer at the beginning of the string, then returns
 * @a begin; *@a result is unchanged.
 *
 * This function checks for overflow.  If an integer is too large for @a
 * result, then the maximum possible value is stored in @a result and the
 * cp_errno variable is set to CPE_OVERFLOW.  Otherwise, cp_errno is set to
 * CPE_FORMAT (for no valid integer) or CPE_OK (if all was well).
 *
 * Overloaded versions of this function are available for int, unsigned int,
 * long, unsigned long, and (depending on configuration) long long and
 * unsigned long long @a result values.
 */
inline const char *cp_integer(const char *begin, const char *end, int base, int *result)
{
    return cp_basic_integer(begin, end, base, -(int) sizeof(*result), result);
}

/** @brief  Parse an integer from @a str in base @a base.
 * @param  str   string
 * @param  base  base of integer: 0 or 2-36
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an integer from an input string.  If the string correctly parses as
 * an integer, then the resulting value is stored in *@a result and the
 * function returns true.  Otherwise, *@a result remains unchanged and the
 * function returns false.
 *
 * Overloaded versions are available for int, unsigned int, long, unsigned
 * long, and (depending on configuration) long long and unsigned long long @a
 * result values.
 *
 * @sa cp_integer(const char *, const char *, int, int *) for the rules on
 * parsing integers.
 */
inline bool cp_integer(const String &str, int base, int *result)
{
    return cp_basic_integer(str.begin(), str.end(), base + cp_basic_integer_whole, -(int) sizeof(*result), result) != str.begin();
}


inline const char *cp_integer(const char *begin, const char *end, int base, unsigned *result)
{
    return cp_basic_integer(begin, end, base, (int) sizeof(*result), result);
}

/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, unsigned *result)
{
    return reinterpret_cast<const unsigned char *>(cp_integer(reinterpret_cast<const char *>(begin), reinterpret_cast<const char *>(end), base, result));
}
/// @endcond


inline const char *cp_integer(const char *begin, const char *end, int base, long *result)
{
    return cp_basic_integer(begin, end, base, -(int) sizeof(*result), result);
}

inline const char *cp_integer(const char *begin, const char *end, int base, unsigned long *result)
{
    return cp_basic_integer(begin, end, base, (int) sizeof(*result), result);
}

/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, unsigned long *result)
{
    return reinterpret_cast<const unsigned char *>(cp_integer(reinterpret_cast<const char *>(begin), reinterpret_cast<const char *>(end), base, result));
}
/// @endcond


#if HAVE_LONG_LONG

inline const char *cp_integer(const char *begin, const char *end, int base, long long *result)
{
    return cp_basic_integer(begin, end, base, -(int) sizeof(*result), result);
}

inline const char *cp_integer(const char *begin, const char *end, int base, unsigned long long *result)
{
    return cp_basic_integer(begin, end, base, (int) sizeof(*result), result);
}

/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, unsigned long long *result)
{
    return reinterpret_cast<const unsigned char *>(cp_integer(reinterpret_cast<const char *>(begin), reinterpret_cast<const char *>(end), base, result));
}
/// @endcond

#elif HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG

inline const char *cp_integer(const char *begin, const char *end, int base, int64_t *result)
{
    return cp_basic_integer(begin, end, base, -(int) sizeof(*result), result);
}

inline const char *cp_integer(const char *begin, const char *end, int base, uint64_t *result)
{
    return cp_basic_integer(begin, end, base, (int) sizeof(*result), result);
}

/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, uint64_t *result)
{
    return reinterpret_cast<const unsigned char *>(cp_integer(reinterpret_cast<const char *>(begin), reinterpret_cast<const char *>(end), base, result));
}
/// @endcond

#endif

inline bool cp_integer(const String &str, int base, int *result);

inline bool cp_integer(const String &str, int base, unsigned int *result)
{
    return cp_basic_integer(str.begin(), str.end(), base + cp_basic_integer_whole, (int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, int base, long *result)
{
    return cp_basic_integer(str.begin(), str.end(), base + cp_basic_integer_whole, -(int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, int base, unsigned long *result)
{
    return cp_basic_integer(str.begin(), str.end(), base + cp_basic_integer_whole, (int) sizeof(*result), result) != str.begin();
}

#if HAVE_LONG_LONG

inline bool cp_integer(const String &str, int base, long long *result)
{
    return cp_basic_integer(str.begin(), str.end(), base + cp_basic_integer_whole, -(int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, int base, unsigned long long *result)
{
    return cp_basic_integer(str.begin(), str.end(), base + cp_basic_integer_whole, (int) sizeof(*result), result) != str.begin();
}

#elif HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG

inline bool cp_integer(const String &str, int base, int64_t *result)
{
    return cp_basic_integer(str.begin(), str.end(), base + cp_basic_integer_whole, -(int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, int base, uint64_t *result)
{
    return cp_basic_integer(str.begin(), str.end(), base + cp_basic_integer_whole, (int) sizeof(*result), result) != str.begin();
}

#endif

/** @brief  Parse an integer from @a str in base 0.
 *
 *  Same as cp_integer(str, 0, result). */
inline bool cp_integer(const String &str, int *result)
{
    return cp_basic_integer(str.begin(), str.end(), cp_basic_integer_whole, -(int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, unsigned int *result)
{
    return cp_basic_integer(str.begin(), str.end(), cp_basic_integer_whole, (int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, long *result)
{
    return cp_basic_integer(str.begin(), str.end(), cp_basic_integer_whole, -(int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, unsigned long *result)
{
    return cp_basic_integer(str.begin(), str.end(), cp_basic_integer_whole, (int) sizeof(*result), result) != str.begin();
}

#if HAVE_LONG_LONG

inline bool cp_integer(const String &str, long long *result)
{
    return cp_basic_integer(str.begin(), str.end(), cp_basic_integer_whole, -(int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, unsigned long long *result)
{
    return cp_basic_integer(str.begin(), str.end(), cp_basic_integer_whole, (int) sizeof(*result), result) != str.begin();
}

#elif HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG

inline bool cp_integer(const String &str, int64_t *result)
{
    return cp_basic_integer(str.begin(), str.end(), cp_basic_integer_whole, -(int) sizeof(*result), result) != str.begin();
}

inline bool cp_integer(const String &str, uint64_t *result)
{
    return cp_basic_integer(str.begin(), str.end(), cp_basic_integer_whole, (int) sizeof(*result), result) != str.begin();
}

#endif

inline bool cp_ip_address(const String& str, struct in_addr *ina  CP_CONTEXT)
{
    return cp_ip_address(str, reinterpret_cast<IPAddress *>(ina)  CP_PASS_CONTEXT);
}

#if HAVE_IP6
inline bool cp_ip6_address(const String& str, struct in6_addr *x  CP_CONTEXT)
{
    return cp_ip6_address(str, reinterpret_cast<IP6Address *>(x)  CP_PASS_CONTEXT);
}
#endif

/// @cond never
inline bool cp_seconds_as(int want_power, const String &str, uint32_t *result)
{
    return cp_seconds_as(str, want_power, result);
}

#if !CLICK_TOOL
inline int cp_register_argtype(const char* name, const char* description, int flags,
			       void (*parsefunc)(cp_value *, const String &, ErrorHandler *, const char *, Element *),
			       void (*storefunc)(cp_value *, Element *),
			       void *user_data = 0) {
    return cp_register_argtype(name, description, flags,
			       (cp_parsefunc) parsefunc,
			       (cp_storefunc) storefunc,
			       user_data);
}
#endif

inline String cp_pop_spacevec(String &str) CLICK_DEPRECATED;

/// @brief  Remove and return the first space-separated argument from @a str.
/// @param[in,out]  str  space-separated configuration string
/// @deprecated This is a deprecated synonym for cp_shift_spacevec().
inline String cp_pop_spacevec(String &str) {
    return cp_shift_spacevec(str);
}

#undef CP_VA_ARGS_REST
#undef CP_OPT_CONTEXT
#undef CP_CONTEXT
#undef CP_PASS_CONTEXT
#undef CLICK_CONFPARSE_DEPRECATED
#undef CP_SENTINEL
#define cpEnd ((CpVaParseCmd) 0)
/// @endcond
CLICK_ENDDECLS
#endif
