#ifndef CONFPARSE_HH
#define CONFPARSE_HH
#include "string.hh"
#include "vector.hh"
class ErrorHandler;
#ifndef CLICK_TOOL
class Element;
# define CP_VA_PARSE_ARGS_REST Element *, ErrorHandler *, ...
#else
# define CP_VA_PARSE_ARGS_REST ErrorHandler *, ...
#endif

bool cp_eat_space(String &);
bool cp_is_space(const String &);
bool cp_is_word(const String &);

// argument lists <-> lists of arguments
String cp_subst(const String &);
void cp_argvec(const String &, Vector<String> &);
void cp_spacevec(const String &, Vector<String> &);
String cp_unargvec(const Vector<String> &);
String cp_quote_string(const String &);

// numbers
bool cp_bool(String, bool *, String *rest = 0);
bool cp_integer(String, int *, String *rest = 0);
bool cp_integer(String, int base, int *, String *rest = 0);
bool cp_ulong(String, unsigned long *, String *rest = 0);
bool cp_real(const String &, int frac_digits, int *, int *, String *rest = 0);
bool cp_real(const String &, int frac_digits, int *, String *rest = 0);
bool cp_real2(const String &, int frac_bits, int *, String *rest = 0);
bool cp_milliseconds(const String &, int *, String *rest = 0);
bool cp_word(String, String *, String *rest = 0);
bool cp_string(String, String *, String *rest = 0);

// network addresses
bool cp_ip_address(String, unsigned char *, String *rest = 0);
bool cp_ip_address_mask(String, unsigned char *, unsigned char *, String *rest = 0);
bool cp_ethernet_address(const String &, unsigned char *, String *rest = 0);
#ifndef CLICK_TOOL
class IPAddress; class EtherAddress;
bool cp_ip_address(String, IPAddress &, String *rest = 0);
bool cp_ip_address_mask(String, IPAddress &, IPAddress &, String *rest = 0);
bool cp_ethernet_address(String, EtherAddress &, String *rest = 0);
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
  cpEthernetAddress, // unsigned char value[6] (or EtherAddress *)
  cpElement,	// Element **value
  cpDesCblock,  // unsigned char value[8]
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

void cp_argvec(const String &, Vector<String> &);

#undef CP_VA_ARGS_REST
#endif
