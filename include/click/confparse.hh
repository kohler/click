#ifndef CONFPARSE_HH
#define CONFPARSE_HH
#include <click/string.hh>
#include <click/vector.hh>
class ErrorHandler;
#ifndef CLICK_TOOL
class Element;
# define CP_VA_PARSE_ARGS_REST Element *, ErrorHandler *, ...
# define CP_OPT_CONTEXT , Element *context = 0
# define CP_CONTEXT , Element *context
#else
# define CP_VA_PARSE_ARGS_REST ErrorHandler *, ...
# define CP_OPT_CONTEXT
# define CP_CONTEXT
#endif

bool cp_eat_space(String &);
bool cp_is_space(const String &);
bool cp_is_word(const String &);

String cp_unquote(const String &);
String cp_quote(const String &, bool allow_newlines = false);
String cp_uncomment(const String &);

// argument lists <-> lists of arguments
void cp_argvec(const String &, Vector<String> &);
String cp_unargvec(const Vector<String> &);
void cp_spacevec(const String &, Vector<String> &);
String cp_unspacevec(const Vector<String> &);

enum CpErrors {
  CPE_OK = 0,
  CPE_FORMAT,
  CPE_NEGATIVE,
  CPE_OVERFLOW,
  CPE_INVALID,
};
extern int cp_errno;

// strings and words
bool cp_string(const String &, String *, String *rest = 0);
bool cp_word(const String &, String *, String *rest = 0);
bool cp_keyword(const String &, String *, String *rest = 0);

// numbers
bool cp_bool(const String &, bool *);
bool cp_integer(const String &, int *);
bool cp_integer(const String &, int base, int *);
bool cp_unsigned(const String &, unsigned *);
bool cp_unsigned(const String &, int base, unsigned *);
bool cp_real2(const String &, int frac_bits, int *);
bool cp_unsigned_real2(const String &, int frac_bits, unsigned *);
bool cp_real10(const String &, int frac_digits, int *, int *);
bool cp_real10(const String &, int frac_digits, int *);
bool cp_milliseconds(const String &, int *);
bool cp_timeval(const String &, struct timeval *);

String cp_unparse_bool(bool);
String cp_unparse_ulonglong(unsigned long long, int base, bool uppercase);
String cp_unparse_real2(int, int frac_bits);
String cp_unparse_real2(unsigned, int frac_bits);
String cp_unparse_real10(int, int frac_digits);
String cp_unparse_real10(unsigned, int frac_digits);
String cp_unparse_milliseconds(int);

// network addresses
class IPAddress;
class IPAddressSet;
bool cp_ip_address(const String &, unsigned char *  CP_OPT_CONTEXT);
bool cp_ip_address(const String &, IPAddress *  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String &, unsigned char *, unsigned char *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String &, IPAddress *, IPAddress *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String &, unsigned char *, unsigned char *  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String &, IPAddress *, IPAddress *  CP_OPT_CONTEXT);
bool cp_ip_address_set(const String &, IPAddressSet *  CP_OPT_CONTEXT);

class IP6Address;
bool cp_ip6_address(const String &, unsigned char *  CP_OPT_CONTEXT);
bool cp_ip6_address(const String &, IP6Address *  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, unsigned char *, int *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, unsigned char *, unsigned char *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, IPAddress *, IP6Address *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, unsigned char *, unsigned char *  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, IPAddress *, IP6Address *  CP_OPT_CONTEXT);

class EtherAddress;
bool cp_ethernet_address(const String &, unsigned char *  CP_OPT_CONTEXT);
bool cp_ethernet_address(const String &, EtherAddress *  CP_OPT_CONTEXT);

#ifndef CLICK_TOOL
Element *cp_element(const String &, Element *, ErrorHandler *);
#endif

#ifdef HAVE_IPSEC
bool cp_des_cblock(const String &, unsigned char *);
#endif

typedef const char * const CpVaParseCmd;
static CpVaParseCmd cpEnd = 0;
extern CpVaParseCmd
  cpOptional,
  cpUnmixedKeywords,
  cpMixedKeywords,
  cpKeywords,
  cpIgnore,
  cpIgnoreRest,
  cpArgument,	// String *value
  cpString,	// String *value
  cpWord,	// String *value
  cpBool,	// bool *value
  cpByte,	// unsigned char *value
  cpShort,	// short *value
  cpUnsignedShort, // unsigned short *value
  cpInteger,	// int *value
  cpUnsigned,	// unsigned *value
  cpReal2,	  // int frac_bits, int *value
  cpNonnegReal2,  // int frac_bits, unsigned *value
  cpReal10,	  // int frac_digits, int *value
  cpNonnegReal10, // int frac_digits, unsigned *value
  cpMilliseconds, // int *value_milliseconds
  cpTimeval,	// struct timeval *value
  cpIPAddress,	// unsigned char value[4] (or IPAddress *, or unsigned int *)
  cpIPPrefix,	// unsigned char value[4], unsigned char mask[4]
  cpIPAddressOrPrefix,	// unsigned char value[4], unsigned char mask[4]
  cpIPAddressSet,	// IPAddressSet *
  cpEthernetAddress,	// unsigned char value[6] (or EtherAddress *)
  cpElement,	// Element **value
  cpIP6Address,	// unsigned char value[16] (or IP6Address *)
  cpIP6Prefix,	// unsigned char value[16], unsigned char mask[16]
  cpIP6AddressOrPrefix,	// unsigned char value[16], unsigned char mask[16]
  cpDesCblock;	// unsigned char value[8]

int cp_va_parse(const Vector<String> &, CP_VA_PARSE_ARGS_REST);
int cp_va_parse(const String &, CP_VA_PARSE_ARGS_REST);
int cp_va_space_parse(const String &, CP_VA_PARSE_ARGS_REST);
int cp_va_parse_keyword(const String &, CP_VA_PARSE_ARGS_REST);
// Takes: cpEnd					end of argument list
//        cpOptional, cpKeywords, cpIgnore...	manipulators
//        CpVaParseCmd type_id,			actual argument
//		const char *description,
//		[[from table above; usually T *value_store]]
// Stores no values in the value_store arguments on error.

void cp_va_static_initialize();
void cp_va_static_cleanup();

// adding and removing types suitable for cp_va_parse and friends
struct cp_value;
struct cp_argtype;

typedef void (*cp_parsefunc)(cp_value *, const String &arg,
			     ErrorHandler *, const char *argdesc  CP_CONTEXT);
typedef void (*cp_storefunc)(cp_value *  CP_CONTEXT);

enum { cpArgNormal = 0, cpArgStore2, cpArgExtraInt };
int cp_register_argtype(const char *name, const char *description,
			int extra, cp_parsefunc, cp_storefunc);
void cp_unregister_argtype(const char *name);

struct cp_argtype {
  const char *name;
  cp_argtype *next;
  cp_parsefunc parse;
  cp_storefunc store;
  int extra;
  const char *description;
  int internal;
  int use_count;
};

struct cp_value {
  // set by cp_va_parse:
  const cp_argtype *argtype;
  const char *keyword;
  const char *description;
  int extra;
  void *store;
  void *store2;
  // set by parsefunc, used by storefunc:
  union {
    bool b;
    int i;
    unsigned u;
    unsigned char address[32];
    int is[8];
#ifndef CLICK_TOOL
    Element *element;
#endif
  } v;
  String v_string;
};

#undef CP_VA_ARGS_REST
#undef CP_OPT_CONTEXT
#undef CP_CONTEXT
#endif
