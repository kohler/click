#ifndef CONFPARSE_HH
#define CONFPARSE_HH
#include "string.hh" 
#include "vector.hh"
class ErrorHandler;
#ifndef CLICK_TOOL
class Element;
# define CP_VA_PARSE_ARGS_REST Element *, ErrorHandler *, ...
# define CP_CONTEXT , Element *context = 0
#else
# define CP_VA_PARSE_ARGS_REST ErrorHandler *, ...
# define CP_CONTEXT
#endif

bool cp_eat_space(String &);
bool cp_is_space(const String &);
bool cp_is_word(const String &);

// argument lists <-> lists of arguments
String cp_subst(const String &);
void cp_argvec(const String &, Vector<String> &);
void cp_spacevec(const String &, Vector<String> &);
String cp_unargvec(const Vector<String> &);
String cp_unspacevec(const Vector<String> &);
String cp_unquote(const String &);
String cp_quote(const String &, bool allow_newlines = false);

// numbers
bool cp_bool(const String &, bool *);
bool cp_integer(String, int *);
bool cp_integer(String, int base, int *);
bool cp_ulong(String, unsigned long *);
bool cp_real(const String &, int frac_digits, int *, int *);
bool cp_real(const String &, int frac_digits, int *);
bool cp_real2(const String &, int frac_bits, int *);
bool cp_milliseconds(const String &, int *);
bool cp_word(const String &, String *, String *rest = 0);
bool cp_string(const String &, String *, String *rest = 0);

// network addresses
bool cp_ip_address(const String &, unsigned char *  CP_CONTEXT);
bool cp_ip6_address(const String &, unsigned char *  CP_CONTEXT);
bool cp_ip_address_mask(const String &, unsigned char *, unsigned char *, bool allow_bare_address = false  CP_CONTEXT);
bool cp_ethernet_address(const String &, unsigned char *  CP_CONTEXT);
#ifndef CLICK_TOOL
class IPAddress; class IP6Address; class EtherAddress;
bool cp_ip_address(const String &, IPAddress &  CP_CONTEXT);
bool cp_ip6_address(const String &, IP6Address &  CP_CONTEXT);
bool cp_ip_address_mask(const String &, IPAddress &, IPAddress &, bool allow_bare_address = false  CP_CONTEXT);
bool cp_ethernet_address(const String &, EtherAddress &  CP_CONTEXT);
Element *cp_element(const String &, Element *, ErrorHandler *);
#endif

#ifdef HAVE_IPSEC
bool cp_des_cblock(const String &, unsigned char *, String *rest = 0);
#endif

enum CpVaParseCmd {
  cpEnd = 0,
  cpOptional,
  cpIgnoreRest,
  cpLastControl,
  cpIgnore,
  cpBool,	// bool *value
  cpByte,	// unsigned char *value
  cpInteger,	// int *value
  cpUnsigned,	// int *value
  cpUnsignedLong, // unsigned long *value
  cpReal,	// int frac_digits, int *value
  cpNonnegReal,	// int frac_digits, int *value
  cpMilliseconds, // int *value_milliseconds
  cpNonnegFixed, // int frac_bits, int *value
  cpString,	// String *value
  cpWord,	// String *value
  cpArgument,	// String *value
  cpIPAddress,	// unsigned char value[4] (or IPAddress *, or unsigned int *)
  cpIPAddressMask, // unsigned char value[4], unsigned char mask[4]
  cpIPAddressOptMask, // unsigned char value[4], unsigned char mask[4]
  cpEthernetAddress, // unsigned char value[6] (or EtherAddress *)
  cpElement,	// Element **value
  cpDesCblock,  // unsigned char value[8]
  cpIP6Address  // unsigned char value[16] (or IP6Address *)
};

int cp_va_parse(const Vector<String> &, CP_VA_PARSE_ARGS_REST);
int cp_va_parse(const String &, CP_VA_PARSE_ARGS_REST);
int cp_va_space_parse(const String &, CP_VA_PARSE_ARGS_REST);
// ... is: cpEnd				stop
//     or: cpOptional				remaining args are optional
//     or: CpVaParseCmd type_id,		actual argument
//		const char *description,
//		[[from table above; usually T *value_store]]
// cp_va_parse stores no values in the value_store arguments
// unless it succeeds.

String cp_unparse_bool(bool);
String cp_unparse_real(int, int frac_bits);
String cp_unparse_ulonglong(unsigned long long, int base, bool uppercase);

#undef CP_VA_ARGS_REST
#undef CP_CONTEXT
#endif
