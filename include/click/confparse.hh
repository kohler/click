// -*- c-basic-offset: 4; related-file-name: "../../lib/confparse.cc" -*-
#ifndef CLICK_CONFPARSE_HH
#define CLICK_CONFPARSE_HH
/// @cond never
#include <click/string.hh>
#include <click/vector.hh>
struct in_addr;
CLICK_DECLS
class ErrorHandler;
class StringAccum;
class Timestamp;
#ifndef CLICK_TOOL
class Element;
class Router;
class Handler;
class HandlerCall;
# define CP_VA_PARSE_ARGS_REST	Element*, ErrorHandler*, ...
# define CP_OPT_CONTEXT		, Element* context = 0
# define CP_CONTEXT		, Element* context
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
String cp_pop_spacevec(String& str);
String cp_unspacevec(const String* begin, const String* end);
inline String cp_unspacevec(const Vector<String>& conf);
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

// numbers
bool cp_bool(const String& str, bool* result);

const char *cp_integer(const char* begin, const char* end, int base, int* result);
const char *cp_integer(const char* begin, const char* end, int base, unsigned int* result);
/// @cond never
inline const unsigned char *cp_integer(const unsigned char* begin, const unsigned char* end, int base, unsigned int* result);
/// @endcond

#if SIZEOF_LONG == SIZEOF_INT
// may be needed to simplify overloading 
inline const char *cp_integer(const char* begin, const char* end, int base, long* result);
inline const char *cp_integer(const char* begin, const char* end, int base, unsigned long* result);
#elif SIZEOF_LONG != 8
# error "long has an odd size"
#endif

#if HAVE_INT64_TYPES
const char *cp_integer(const char* begin, const char* end, int base, int64_t* result);
const char *cp_integer(const char* begin, const char* end, int base, uint64_t* result);
/// @cond never
inline const unsigned char *cp_integer(const unsigned char* begin, const unsigned char* end, int base, uint64_t* result);
/// @endcond
#endif

bool cp_integer(const String& str, int base, int* result);
bool cp_integer(const String& str, int base, unsigned int* result);
#if SIZEOF_LONG == SIZEOF_INT
inline bool cp_integer(const String& str, int base, long* result);
inline bool cp_integer(const String& str, int base, unsigned long* result);
#endif
#ifdef HAVE_INT64_TYPES
bool cp_integer(const String& str, int base, int64_t* result);
bool cp_integer(const String& str, int base, uint64_t* result);
#endif

inline bool cp_integer(const String& str, int* result);
inline bool cp_integer(const String& str, unsigned int* result);
#if SIZEOF_LONG == SIZEOF_INT
inline bool cp_integer(const String& str, long* result);
inline bool cp_integer(const String& str, unsigned long* result);
#endif
#ifdef HAVE_INT64_TYPES
inline bool cp_integer(const String& str, int64_t* result);
inline bool cp_integer(const String& str, uint64_t* result);
#endif

#ifdef CLICK_USERLEVEL
bool cp_file_offset(const String& str, off_t* result);
#endif

#define CP_REAL2_MAX_FRAC_BITS 28
bool cp_real2(const String& str, int frac_bits, int32_t* result);
bool cp_real2(const String& str, int frac_bits, uint32_t* result);
bool cp_real10(const String& str, int frac_digits, int32_t* result);
bool cp_real10(const String& str, int frac_digits, uint32_t* result);
bool cp_real10(const String& str, int frac_digits, uint32_t* result_int, uint32_t* result_frac);
#ifdef HAVE_FLOAT_TYPES
bool cp_double(const String& str, double* result);
#endif

bool cp_seconds_as(const String& str, int frac_digits, uint32_t* result);
bool cp_seconds_as_milli(const String& str, uint32_t* result);
bool cp_seconds_as_micro(const String& str, uint32_t* result);
bool cp_time(const String& str, Timestamp* result);
bool cp_time(const String& str, struct timeval* result);

bool cp_bandwidth(const String& str, uint32_t* result);

// network addresses
class IPAddress;
class IPAddressList;
bool cp_ip_address(const String& str, IPAddress* result  CP_OPT_CONTEXT);
inline bool cp_ip_address(const String& str, struct in_addr* result  CP_OPT_CONTEXT);
bool cp_ip_address(const String& str, unsigned char* result  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String& str, IPAddress* result_addr, IPAddress* result_mask, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String& str, unsigned char* result_addr, unsigned char* result_mask, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String& str, IPAddress* result_addr, IPAddress* result_mask  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String& str, unsigned char* result_addr, unsigned char* result_mask  CP_OPT_CONTEXT);
bool cp_ip_address_list(const String& str, Vector<IPAddress>* result  CP_OPT_CONTEXT);

#ifdef HAVE_IP6
class IP6Address;
bool cp_ip6_address(const String& str, IP6Address* result  CP_OPT_CONTEXT);
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

#ifndef CLICK_TOOL
Element *cp_element(const String& str, Element* context, ErrorHandler* errh=0);
Element *cp_element(const String& str, Router* router, ErrorHandler* errh=0);
bool cp_handler_name(const String& str, Element** result_element, String* result_hname, Element* context, ErrorHandler* errh=0);
bool cp_handler(const String& str, int flags, Element** result_element, const Handler** result_handler, Element* context, ErrorHandler* errh=0);
#endif

#ifdef HAVE_IPSEC
bool cp_des_cblock(const String& str, unsigned char* result);
#endif

#if CLICK_USERLEVEL
bool cp_filename(const String& str, String* result);
#endif
//@}

/// @name cp_va_kparse
//@{
int cp_va_kparse(const Vector<String>& conf, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;
int cp_va_kparse(const String& str, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;
int cp_va_space_kparse(const String& str, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;
int cp_va_kparse_keyword(const String& str, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;
int cp_va_kparse_remove_keywords(Vector<String>& conf, CP_VA_PARSE_ARGS_REST) CP_SENTINEL;

int cp_assign_arguments(const Vector<String>& argv, const String *keys_begin, const String *keys_end, Vector<String>* values = 0);

void cp_va_static_initialize();
void cp_va_static_cleanup();

/// @brief Type of flags for cp_va_kparse() items.
enum CpKparseFlags {
    cpkN = 0,		///< Default flags
    cpkM = 1,		///< Argument is mandatory
    cpkP = 2,		///< Argument may be specified positionally
    cpkC = 4,		///< Argument presence should be confirmed
    cpkNormal = cpkN,
    cpkMandatory = cpkM,
    cpkPositional = cpkP,
    cpkConfirm = cpkC
};

/// @brief Type of argument type names for cp_va_kparse() items.
typedef const char *CpVaParseCmd;

extern const CpVaParseCmd
    cpEnd,		///< Use as argument name.  Ends cp_va argument list.
    cpIgnoreRest,	///< Use as argument name.  No result storage; causes cp_va_kparse to ignore unparsed arguments and any remaining items.
    cpIgnore,		///< No result storage (this argument is ignored).
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
    cpNamedInteger,	///< Parse parameter uint32_t nameinfo_type, result storage int32_t*, parsed by NameInfo::query_int.
#ifdef HAVE_INT64_TYPES
    cpInteger64,	///< Result storage int64_t*, parsed by cp_integer().
    cpUnsigned64,	///< Result storage uint64_t*, parsed by cp_integer().
#endif
#ifdef CLICK_USERLEVEL
    cpFileOffset,	///< Result storage off_t*, parsed by cp_integer().
#endif
    cpUnsignedReal2,	///< Parse parameter int frac_bits, result storage uint32_t*, parsed by cp_real2().
    cpReal10,		///< Parse parameter int frac_digits, result storage int32_t*, parsed by cp_real10().
    cpUnsignedReal10,	///< Parse parameter int frac_digits, result storage uint32_t*, parsed by cp_real10().
#ifdef HAVE_FLOAT_TYPES
    cpDouble,		///< Result storage double*, parsed by cp_double().
#endif
    cpSeconds,		///< Result storage uint32_t*, parsed by cp_seconds_as() with frac_digits 0.
    cpSecondsAsMilli,	///< Result storage uint32_t*, parsed by cp_seconds_as_milli().
    cpSecondsAsMicro,	///< Result storage uint32_t*, parsed by cp_seconds_as_micro().
    cpTimestamp,	///< Result storage Timestamp*, parsed by cp_time().
    cpTimeval,		///< Result storage struct timeval*, parsed by cp_time().
    cpBandwidth,	///< Result storage uint32_t*, parsed by cp_bandwidth().
    cpIPAddress,	///< Result storage IPAddress* or equivalent, parsed by cp_ip_address().
    cpIPPrefix,		///< Result storage IPAddress* addr and IPAddress *mask, parsed by cp_ip_prefix().
    cpIPAddressOrPrefix,///< Result storage IPAddress* addr and IPAddress *mask, parsed by cp_ip_prefix().
    cpIPAddressList,	///< Result storage Vector<IPAddress>*, parsed by cp_ip_address_list().
    cpEthernetAddress,	///< Result storage EtherAddress*, parsed by cp_ethernet_address().
    cpTCPPort,		///< Result storage uint16_t*, parsed by cp_tcpudp_port().
    cpUDPPort,		///< Result storage uint16_t*, parsed by cp_tcpudp_port().
    cpElement,		///< Result storage Element**, parsed by cp_element().
    cpHandlerName,	///< Result storage Element** and String*, parsed by cp_handler_name().
    cpHandlerCallRead,	///< Result storage HandlerCall*, parsed by HandlerCall.
    cpHandlerCallWrite,	///< Result storage HandlerCall*, parsed by HandlerCall.
    cpHandlerCallPtrRead, ///< Result storage HandlerCall**, parsed by HandlerCall::reset_read.
    cpHandlerCallPtrWrite, ///< Result storage HandlerCall**, parsed by HandlerCall::reset_write.
    cpIP6Address,	///< Result storage IP6Address* or equivalent, parsed by cp_ip6_address().
    cpIP6Prefix,	///< Result storage IP6Address* addr and IP6Address* mask, parsed by cp_ip6_prefix().
    cpIP6AddressOrPrefix,///< Result storage IP6Address* addr and IP6Address* mask, parsed by cp_ip6_prefix().
    cpDesCblock,	///< Result storage uint8_t[8], parsed by cp_des_cblock().
    cpFilename,		///< Result storage String*, parsed by cp_filename().
    cpOptional,		///< cp_va_parse only: Following arguments are optional.
    cpKeywords,		///< cp_va_parse only: Following arguments are keywords.
    cpConfirmKeywords,	///< cp_va_parse only: Following arguments are confirmed keywords.
    cpMandatoryKeywords;///< cp_va_parse only: Following arguments are mandatory keywords.
// old names, here for compatibility:
extern const CpVaParseCmd
    cpInterval CLICK_CONFPARSE_DEPRECATED,		// struct timeval*
    cpEtherAddress CLICK_CONFPARSE_DEPRECATED,		// EtherAddress*
    cpReadHandlerCall CLICK_CONFPARSE_DEPRECATED,	// HandlerCall**
    cpWriteHandlerCall CLICK_CONFPARSE_DEPRECATED;	// HandlerCall**
//@}

/// @name Unparsing
//@{
String cp_unparse_bool(bool value);
String cp_unparse_real2(int32_t value, int frac_bits);
String cp_unparse_real2(uint32_t value, int frac_bits);
#ifdef HAVE_INT64_TYPES
String cp_unparse_real2(int64_t value, int frac_bits);
String cp_unparse_real2(uint64_t value, int frac_bits);
#endif
String cp_unparse_real10(int32_t value, int frac_digits);
String cp_unparse_real10(uint32_t value, int frac_digits);
String cp_unparse_milliseconds(uint32_t value);
String cp_unparse_microseconds(uint32_t value);
String cp_unparse_interval(const Timestamp& value);
String cp_unparse_interval(const struct timeval& value);
String cp_unparse_bandwidth(uint32_t value);
//@}

/// @name Legacy Functions
//@{
int cp_va_parse(const Vector<String>& conf, CP_VA_PARSE_ARGS_REST);
int cp_va_parse(const String& str, CP_VA_PARSE_ARGS_REST);
int cp_va_space_parse(const String& str, CP_VA_PARSE_ARGS_REST);
int cp_va_parse_keyword(const String& str, CP_VA_PARSE_ARGS_REST);
int cp_va_parse_remove_keywords(Vector<String>& conf, int first, CP_VA_PARSE_ARGS_REST);
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

/// @name Argument Types for cp_va_kparse
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
    int extra;
    void* store;
    void* store2;
    bool* store_confirm;
    // set by parsefunc, used by storefunc:
    union {
	bool b;
	int32_t i;
	uint32_t u;
#ifdef HAVE_INT64_TYPES
	int64_t i64;
	uint64_t u64;
#endif
#ifdef HAVE_FLOAT_TYPES
	double d;
#endif
	unsigned char address[16];
	int is[4];
#ifndef CLICK_TOOL
	Element* element;
#endif
    } v, v2;
    String v_string;
    String v2_string;
};

enum { cpArgNormal = 0, cpArgStore2 = 1, cpArgExtraInt = 2, cpArgAllowNumbers = 4 };
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

/** @brief  Parse an integer from @a str in base 0.
 *
 *  Same as cp_integer(str, 0, result). */
inline bool cp_integer(const String& str, int* result)
{
    return cp_integer(str, 0, result);
}

inline bool cp_integer(const String& str, unsigned int* result)
{
    return cp_integer(str, 0, result);
}

/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, unsigned int* result)
{
    return (const unsigned char *) cp_integer((const char *) begin, (const char *) end, base, result);
}
/// @endcond

#ifdef HAVE_INT64_TYPES
/// @cond never
inline const unsigned char *cp_integer(const unsigned char *begin, const unsigned char *end, int base, uint64_t* result)
{
    return (const unsigned char *) cp_integer((const char *) begin, (const char *) end, base, result);
}
/// @endcond

inline bool cp_integer(const String& str, uint64_t* result)
{
    return cp_integer(str, 0, result);
}

inline bool cp_integer(const String& str, int64_t* result)
{
    return cp_integer(str, 0, result);
}
#endif

#if SIZEOF_LONG == SIZEOF_INT
inline const char* cp_integer(const char* begin, const char* end, int base, long* result)
{
    return cp_integer(begin, end, base, reinterpret_cast<int*>(result));
}

inline bool cp_integer(const String& str, int base, long* result)
{
    return cp_integer(str, base, reinterpret_cast<int*>(result));
}

inline bool cp_integer(const String& str, long* result)
{
    return cp_integer(str, reinterpret_cast<int*>(result));
}

inline const char* cp_integer(const char* begin, const char* end, int base, unsigned long* result)
{
    return cp_integer(begin, end, base, reinterpret_cast<unsigned int*>(result));
}

inline bool cp_integer(const String& str, int base, unsigned long* result)
{
    return cp_integer(str, base, reinterpret_cast<unsigned int*>(result));
}

inline bool cp_integer(const String& str, unsigned long* result)
{
    return cp_integer(str, reinterpret_cast<unsigned int*>(result));
}
#endif

inline bool cp_ip_address(const String& str, struct in_addr* ina  CP_CONTEXT)
{
    return cp_ip_address(str, reinterpret_cast<IPAddress*>(ina)  CP_PASS_CONTEXT);
}

/// @cond never
inline bool cp_seconds_as(int want_power, const String &str, uint32_t *result)
{
    return cp_seconds_as(str, want_power, result);
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
