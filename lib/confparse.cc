/*
 * confparse.{cc,hh} -- configuration string parsing
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "straccum.hh"
#ifndef CLICK_TOOL
# include "router.hh"
# include "ipaddress.hh"
# include "etheraddress.hh"
#endif
#include <stdarg.h>

bool
cp_eat_space(String &str)
{
  const char *s = str.data();
  int len = str.length();
  int i = 0;
  while (i < len) {
    if (!isspace(s[i]))
      break;
    i++;
  }
  str = str.substring(i);
  return true;
}

static bool
only_spaces(const char *s, int len)
{
  for (int i = 0; i < len; i++)
    if (!isspace(s[i]))
      return false;
  return true;
}

static int
xvalue(int x)
{
  if (x >= '0' && x <= '9')
    return x - '0';
  else if (x >= 'A' && x <= 'F')
    return x - 'A' + 10;
  else if (x >= 'a' && x <= 'f')
    return x - 'a' + 10;
  else
    return -1;
}

bool
cp_is_space(const String &str)
{
  return only_spaces(str.data(), str.length());
}

void
cp_argvec_2(const String &conf, Vector<String> &args, bool commas)
{
  const char *s = conf.data();
  int len = conf.length();
  int i = 0;
  bool first_arg = true;
  
  // common case: no configuration
  if (len == 0)
    return;
  
  // <= to handle case where `conf' ends in `,' (= an extra empty string
  // argument)
  while (i <= len) {
    
    // accumulate an argument
    StringAccum sa;
    int start = i;
    
    // we are allowed to eliminate spaces before `first_keep_spaces' and after
    // `keep_spaces' in the completed string
    int keep_spaces = 0, first_keep_spaces = -1;
    
    for (; i < len; i++)
      switch (s[i]) {
	
       case ',':
	if (commas) goto done;
	break;
	
       case '/':
	// skip comments
	if (i == len - 1 || (s[i+1] != '/' && s[i+1] != '*'))
	  break;
	sa << conf.substring(start, i - start);
	sa << ' ';
	if (s[i+1] == '/') {
	  while (i < len && s[i] != '\n' && s[i] != '\r')
	    i++;
	  if (i < len - 1 && s[i] == '\r' && s[i+1] == '\n')
	    i++;
	} else {
	  i += 2;
	  while (i < len && (s[i] != '*' || i == len - 1 || s[i+1] != '/'))
	    i++;
	  i++;
	}
	start = i + 1;
	break;
	
       case '\\':
	if (i < len - 1) {
	  sa << conf.substring(start, i - start);
	  switch (s[i+1]) {
	    
	   case '\r':
	    if (i < len - 2 && s[i+2] == '\n') i++;
	    /* FALLTHRU */
	   case '\n':
	    i++;
	    start = i + 1;
	    break;
	    
	   case 'a': sa << '\a'; goto keep_space_skip_2;
	   case 'b': sa << '\b'; goto keep_space_skip_2;
	   case 'f': sa << '\f'; goto keep_space_skip_2;
	   case 'n': sa << '\n'; goto keep_space_skip_2;
	   case 'r': sa << '\r'; goto keep_space_skip_2;
	   case 't': sa << '\t'; goto keep_space_skip_2;
	   case 'v': sa << '\v'; goto keep_space_skip_2;

	   case '0': case '1': case '2': case '3':
	   case '4': case '5': case '6': case '7': {
	     int c = 0, d = 0;
	     for (i++; i < len && s[i] >= '0' && s[i] <= '7' && d < 3;
		  i++, d++)
	       c = c*8 + s[i] - '0';
	     sa << (char)c;
	     goto keep_space_unskip_1;
	   }

	   case 'x': {
	     int c = 0;
	     for (i += 2; i < len; i++)
	       if (s[i] >= '0' && s[i] <= '9')
		 c = c*16 + s[i] - '0';
	       else if (s[i] >= 'A' && s[i] <= 'F')
		 c = c*16 + s[i] - 'A' + 10;
	       else if (s[i] >= 'a' && s[i] <= 'f')
		 c = c*16 + s[i] - 'a' + 10;
	       else
		 break;
	     sa << (char)c;
	     goto keep_space_unskip_1;
	   }
	   
	   case '<': {
	     int c = 0, d = 0;
	     first_keep_spaces = sa.length();
	     for (i += 2; i < len; i++) {
	       if (isspace(s[i]))
		 continue;
	       else if (s[i] == '>') {
		 i++;		// skip past it
		 break;
	       } else if (s[i] >= '0' && s[i] <= '9')
		 c = c*16 + s[i] - '0';
	       else if (s[i] >= 'A' && s[i] <= 'F')
		 c = c*16 + s[i] - 'A' + 10;
	       else if (s[i] >= 'a' && s[i] <= 'f')
		 c = c*16 + s[i] - 'a' + 10;
	       else
		 continue;	// error: random character
	       if (++d == 2) {
		 sa << (char)c;
		 c = d = 0;
	       }
	     }
	     goto keep_space_unskip_1;
	   }
	    
	   keep_space_unskip_1:
	    keep_spaces = sa.length();
	    if (first_keep_spaces < 0) first_keep_spaces = keep_spaces - 1;
	    i--;
	    start = i + 1;
	    break;
	    
	   default:
	    // keep the next character; if it's a space, don't let it be eaten
	    sa << s[i+1];
	   keep_space_skip_2:
	    keep_spaces = sa.length();
	    if (first_keep_spaces < 0) first_keep_spaces = keep_spaces - 1;
	    i++;
	    start = i + 1;
	    break;
	    
	  }
	}
	break;
	
      }
    
   done:
    int comma_pos = i;
    if (i > start)
      sa << conf.substring(start, i - start);
    
    // remove trailing spaces
    const char *data = sa.data();
    for (int j = sa.length() - 1; j >= keep_spaces && isspace(data[j]); j--)
      sa.pop(1);
    
    // remove leading spaces
    if (first_keep_spaces < 0) first_keep_spaces = sa.length();
    int space_prefix = 0;
    for (; space_prefix < first_keep_spaces && isspace(data[space_prefix]);
	 space_prefix++)
      /* nada */;
    
    // add the argument if it is nonempty, or this isn't the first argument
    String arg = sa.take_string();
    if (space_prefix) arg = arg.substring(space_prefix);
    if (arg || comma_pos < len || !first_arg)
      args.push_back(arg);
    
    // bump `i' past the comma
    i = comma_pos + 1;
    first_arg = false;
  }
}

String
cp_unargvec(const Vector<String> &args)
{
  StringAccum sa;
  for (int an = 0; an < args.size(); an++) {
    const char *s = args[an].data();
    int len = args[an].length();
    if (an) sa << ", ";
    if (len && isspace(s[0])) sa << "\\\n";
    int start = 0, i;
    for (i = 0; i < len; i++)
      if (s[i] == ',' || s[i] == '\\' ||
	  (s[i] == '/' && i < len - 1
	   && (s[i+1] == '/' || s[i+1] == '*'))) {
	sa << args[an].substring(start, i - start) << '\\';
	start = i;
      } else if (s[i] < ' ' || s[i] > '\x7E') {
	sa << args[an].substring(start, i - start) << "\\";
	int c = (unsigned char)s[i];
	sa << (char)('0' + ((c >> 6) & 0x7))
	   << (char)('0' + ((c >> 3) & 0x7))
	   << (char)('0' + ((c) & 0x7));
	start = i + 1;
      }
    sa << args[an].substring(start, i - start);
    if (i > start && isspace(s[i-1])) sa << "\\\n";
  }
  return sa.take_string();
}

void
cp_argvec_unsubst(const String &conf, Vector<String> &args)
{
  const char *s = conf.data();
  int len = conf.length();
  int pos = 0;
  int last_pos = 0;
  bool nonblank = false;
  if (!conf)			// common case
    return;
  
  for (; pos < len; pos++)
    switch (s[pos]) {
      
     case ',':
      args.push_back(conf.substring(last_pos, pos - last_pos));
      last_pos = pos + 1;
      break;
      
     case '/':
      if (pos < len - 1 && s[pos+1] == '/') {
	while (pos < len && s[pos] != '\n' && s[pos] != '\r')
	  pos++;
      } else if (pos < len - 1 && s[pos+1] == '*') {
	pos += 2;
	while (pos < len && (s[pos] != '*' || pos == len - 1 || s[pos+1] != '/'))
	  pos++;
	if (pos < len - 1)
	  pos += 2;
      } else
	nonblank = true;
      break;
      
     case '\\':
      if (pos < len - 1)
	pos++;
      nonblank = true;
      break;

     case ' ': case '\t': case '\r': case '\n': case '\f': case '\v':
      break;

     default:
      nonblank = true;
      break;

    }

  if (last_pos != 0 || nonblank)
    args.push_back(conf.substring(last_pos, pos - last_pos));
}

String
cp_subst(const String &str)
{
  Vector<String> v;
  cp_argvec_2(str, v, false);
  return (v.size() ? v[0] : String());
}

String
cp_unsubst(const String &str)
{
  Vector<String> v;
  v.push_back(str);
  return cp_unargvec(v);
}

void
cp_spacevec(const String &conf, Vector<String> &vec)
{
  const char *s = conf.data();
  int len = conf.length();
  int i = 0;
  
  while (1) {
    while (i < len && isspace(s[i]))
      i++;
    if (i >= len)
      return;

    // accumulate an argument
    int start = i;
    while (i < len && !isspace(s[i]))
      i++;
    vec.push_back(conf.substring(start, i - start));
  }
}

bool
cp_bool(String str, bool &return_value, String *rest = 0)
{
  const char *s = str.data();
  int len = str.length();
  int take;
  
  if (len >= 1 && s[0] == '0') {
    return_value = false;
    take = 1;
  } else if (len >= 1 && s[0] == '1') {
    return_value = true;
    take = 1;
  } else if (len >= 5 && strncmp(s, "false", 5) == 0) {
    return_value = false;
    take = 5;
  } else if (len >= 4 && strncmp(s, "true", 4) == 0) {
    return_value = true;
    take = 4;
  } else if (len >= 2 && strncmp(s, "no", 2) == 0) {
    return_value = false;
    take = 2;
  } else if (len >= 3 && strncmp(s, "yes", 3) == 0) {
    return_value = true;
    take = 3;
  } else
    return false;
  
  if (rest) {
    *rest = str.substring(take);
    return true;
  } else
    return take == len;
}

bool
cp_integer(String str, int base, int &return_value, String *rest = 0)
{
  int i = 0;
  const char *s = str.cc();
  int len = str.length();
  
  bool network_byte_order = false;
  bool negative = false;
  if (i < len && s[i] == '-') {
    negative = true;
    i++;
  } else if (i < len && s[i] == '+')
    i++;

  if (base < 0) {
    if (i < len && s[i] == '0') {
      if (i < len - 1 && (s[i+1] == 'x' || s[i+1] == 'X')) {
	i += 2;
	base = 16;
      } else if (i < len - 1 && (s[i+1] == 'n' || s[i+1] == 'N')) {
	i += 2;
	base = 16;
	network_byte_order = true;
      } else
	base = 8;
    } else
      base = 10;
  }
  
  if (i >= len)			// all spaces
    return false;
  
  char *end;
  return_value = strtol(s + i, &end, base);
  if (negative) return_value = -return_value;
  
  if (end - s == i)		// no characters in integer
    return false;
  
  if (network_byte_order) {
    if (end - s <= i + 2)
      /* do nothing */;
    else if (end - s <= i + 4)
      return_value = htons(return_value);
    else if (end - s <= i + 8)
      return_value = htonl(return_value);
    else
      return false;
  }
  
  if (rest) {
    *rest = str.substring(end - s);
    return true;
  } else
    return end - s == len;
}

bool
cp_integer(String str, int &return_value, String *rest = 0)
{
  return cp_integer(str, -1, return_value, rest);
}

bool
cp_real(const String &str, int frac_digits, int &int_part, int &frac_part,
	String *rest = 0)
{
  const char *s = str.data();
  const char *last = s + str.length();
  if (s == last) return false;
  
  bool negative = (*s == '-');
  if (*s == '-' || *s == '+') s++;
  
  // find integer part of string
  const char *int_s = s;
  while (s < last && isdigit(*s)) s++;
  const char *int_e = s;
  
  // find fractional part of string
  const char *frac_s, *frac_e;
  if (s < last && *s == '.') {
    frac_s = ++s;
    while (s < last && isdigit(*s)) s++;
    frac_e = s;
  } else
    frac_s = frac_e = s;
  
  // no integer or fraction? illegal real
  if (int_s == int_e && frac_s == frac_e) return false;
  
  // find exponent, if any
  int exponent = 0;
  if (s < last && (*s == 'E' || *s == 'e')) {
    if (++s == last) return false;
    bool negexp = (*s == '-');
    if (*s == '-' || *s == '+') s++;
    if (s >= last || !isdigit(*s)) return false;
    
    // XXX overflow?
    for (; s < last && isdigit(*s); s++)
      exponent = 10*exponent + *s - '0';
    
    if (negexp) exponent = -exponent;
  }
  
  // determine integer part
  int_part = 0;
  const char *c;
  for (c = int_s; c < int_e && c < int_e + exponent; c++)
    int_part = 10*int_part + *c - '0';
  for (c = frac_s; c < frac_e && c < frac_s + exponent; c++)
    int_part = 10*int_part + *c - '0';
  for (c = frac_e; c < frac_s + exponent; c++)
    int_part = 10*int_part;
  if (negative) int_part = -int_part;
  
  // determine fraction part
  frac_part = 0;
  for (c = int_e + exponent; c < int_s && frac_digits > 0; c++, frac_digits--)
    /* do nothing */;
  for (; c < int_e && frac_digits > 0; c++, frac_digits--)
    frac_part = 10*frac_part + *c - '0';
  c = frac_s + (exponent > 0 ? exponent : 0);
  for (; c < frac_e && frac_digits > 0; c++, frac_digits--)
    frac_part = 10*frac_part + *c - '0';
  for (; frac_digits > 0; frac_digits--)
    frac_part = 10*frac_part;
  if (negative) frac_part = -frac_part;
  
  // done!
  if (rest) {
    *rest = str.substring(s - str.data());
    return true;
  } else
    return s - str.data() == str.length();
}

int
cp_real2(const String &str, int frac_bits, int &value, String *rest = 0)
{
  int frac_digits = 1;
  int base_ten_one = 10;
  for (; base_ten_one < (1 << frac_bits); base_ten_one *= 10)
    frac_digits++;
  
  int store_int, store_frac;
  if (!cp_real(str, frac_digits, store_int, store_frac, rest))
    return -1;
  else if (store_int < 0 || store_frac < 0)
    return -2;
  else if (store_int > (1 << (32 - frac_bits)) - 1)
    return -3;
  else {
    value = store_int << frac_bits;
    base_ten_one /= 2;
    for (int i = frac_bits - 1; i >= 0; i--, base_ten_one /= 2)
      if (store_frac >= base_ten_one) {
	store_frac -= base_ten_one;
	value |= (1 << i);
      }
    return 0;
  }
}

bool
cp_word(String str, String &return_value, String *rest = 0)
{
  const char *s = str.data();
  int len = str.length();
  int pos = 0;
  while (pos < len && !isspace(s[pos]))
    pos++;
  return_value = str.substring(0, pos);
  if (rest) {
    *rest = str.substring(pos);
    return true;
  } else
    return pos == len;
}

bool
cp_ip_address(String str, unsigned char *return_value, String *rest = 0)
{
  int i = 0;
  const char *s = str.cc();
  int len = str.length();
  
  for (int d = 0; d < 4; d++) {
    char *end;
    int value = strtol(s + i, &end, 10);
    if (end - s == i || value < 0 || value > 255)
      return false;
    if (d != 3 && (end[0] != '.' || !isdigit(end[1])))
      return false;
    return_value[d] = value;
    i = (end - s) + 1;
  }
  
  if (rest) {
    *rest = str.substring(i);
    return true;
  } else
    return i >= len;
}

#ifndef CLICK_TOOL
bool
cp_ip_address(String str, IPAddress &address, String *rest = 0)
{
  return cp_ip_address(str, address.data(), rest);
}
#endif

bool
cp_ethernet_address(const String &str, unsigned char *return_value,
		    String *rest = 0)
{
  int i = 0;
  const char *s = str.data();
  int len = str.length();
  
  for (int d = 0; d < 6; d++) {
    if (i < len - 1 && isxdigit(s[i]) && isxdigit(s[i+1])) {
      return_value[d] = xvalue(s[i])*16 + xvalue(s[i+1]);
      i += 2;
    } else if (i < len && isxdigit(s[i])) {
      return_value[d] = xvalue(s[i]);
      i += 1;
    } else
      return false;
    if (d == 5) break;
    if (i >= len || s[i] != ':')
      return false;
    i++;
  }
  
  if (rest) {
    *rest = str.substring(i);
    return true;
  } else
    return i >= len;
}

#ifndef CLICK_TOOL
bool
cp_ethernet_address(String str, EtherAddress &address, String *rest = 0)
{
  return cp_ethernet_address(str, address.data(), rest);
}
#endif

#ifndef CLICK_TOOL
Element *
cp_element(const String &name, Element *owner, ErrorHandler *errh)
{
  String id = owner->id();
  Router *router = owner->router();
  int i = id.length();
  const char *data = id.data();
  while (true) {
    for (i--; i >= 0 && data[i] != '/'; i--)
      /* nothing */;
    if (i < 0)
      break;
    String n = id.substring(0, i + 1) + name;
    Element *f = router->find(n, 0);
    if (f) return f;
  }
  return router->find(name, errh);
}
#endif

#ifdef HAVE_IPSEC
bool
cp_des_cblock(const String &str, unsigned char *return_value,
		    String *rest = 0)
{
  int i = 0;
  const char *s = str.data();
  int len = str.length();
  
  if (len != 16)
    return false;
  
  for (int d = 0; d < 8; d++) {
    if (i < len - 1 && isxdigit(s[i]) && isxdigit(s[i+1])) {
      return_value[d] = xvalue(s[i])*16 + xvalue(s[i+1]);
      i += 2;
    } else
      return false;
  }
  
  if (rest) {
    *rest = str.substring(i);
    return true;
  } else
    return len - i == 0;
}
#endif

//
// CP_VA_PARSE AND FRIENDS
//

struct Values {
  void *store;
  void *store2;
  union {
    bool b;
    int i;
    struct {
      int int_part;
      int frac_part;
    } real10;
    int real2;
    unsigned char address[8];
#ifndef CLICK_TOOL
    Element *element;
#endif
  } v;
  String v_string;
};

static const char *
cp_command_name(int cp_command)
{
  // negative cp_command means no argument was provided; but we still want to
  // return the type name
  if (cp_command < 0) cp_command = -cp_command;
  switch (cp_command) {
   case cpIgnoreRest: return "...";
   case cpBool: return "bool";
   case cpByte: return "byte";
   case cpInteger: return "int";
   case cpUnsigned: return "unsigned";
   case cpReal: return "real";
   case cpNonnegReal: case cpNonnegReal2: return "unsigned real";
   case cpString: return "string";
   case cpIPAddress: return "IP address";
   case cpEthernetAddress: return "Ethernet address";
   case cpElement: return "element name";
   case cpDesCblock: return "DES encryption block";
   default: return "??";
  }
}

static void
store_value(int cp_command, Values &v)
{
  int address_bytes;
  switch (cp_command) {
    
   case cpBool: {
     bool *bstore = (bool *)v.store;
     *bstore = v.v.b;
     break;
   }
   
   case cpByte: {
     unsigned char *ucstore = (unsigned char *)v.store;
     *ucstore = v.v.i;
     break;
   }
   
   case cpInteger:
   case cpUnsigned:
   case cpNonnegReal2:
   case cpInterval: {
     int *istore = (int *)v.store;
     *istore = v.v.i;
     break;
   }
   
   case cpReal:
   case cpNonnegReal: {
     int *istore = (int *)v.store;
     int *fracstore = (int *)v.store2;
     *istore = v.v.real10.int_part;
     *fracstore = v.v.real10.frac_part;
     break;
   }
   
   case cpString: {
     String *sstore = (String *)v.store;
     *sstore = v.v_string;
     break;
   }
   
   case cpIPAddress:
    address_bytes = 4;
    goto address;
    
   case cpEthernetAddress:
    address_bytes = 6;
    goto address;
    
   case cpDesCblock:
    address_bytes = 8;
    goto address;

#ifndef CLICK_TOOL
   case cpElement: {
     Element **elementstore = (Element **)v.store;
     *elementstore = v.v.element;
     break;
   }
#endif
   
   address: {
     unsigned char *addrstore = (unsigned char *)v.store;
     memcpy(addrstore, v.v.address, address_bytes);
     break;
   }
   
   default:
    // no argument provided
    break;
    
  }
}

static int
cp_va_parsev(Vector<String> &args,
#ifndef CLICK_TOOL
	     Element *element,
#endif
	     ErrorHandler *errh, va_list val)
{
  int argno = 0;
  
  int *cp_commands = new int[args.size() + 11];
  Values *values = new Values[args.size() + 11];
  if (!cp_commands || !values) {
    delete[] cp_commands;
    delete[] values;
    return errh->error("out of memory in cp_va_parse");
  }
  
  int optional = -1;
  bool too_few_args = false;
  int nerrors_in = errh->nerrors();
  
  while (int cp_command = va_arg(val, int)) {

    if (cp_command > cpLastControl && argno >= args.size() && optional < 0)
      too_few_args = true;
    if (argno == args.size() + 10) {
      // no more space to store information about the arguments; append
      // a `...' and break
      cp_commands[argno] = cpIgnoreRest;
      break;
    }
    Values &v = values[argno];
    
    // skip over unspecified optional arguments
    bool skip = (argno >= args.size());
    
    switch (cp_command) {
      
     case cpOptional:
      optional = argno;
      break;
      
     case cpIgnoreRest:
      goto done;
      
     case cpIgnore:
      break;
      
     case cpBool: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, bool *);
       if (skip) break;
       if (!cp_bool(args[argno], v.v.b))
	 errh->error("argument %d should be %s (bool)", argno+1, desc);
       break;
     }
     
     case cpByte: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       if (skip) break;
       if (!cp_integer(args[argno], v.v.i))
	 errh->error("argument %d should be %s (byte)", argno+1, desc);
       if (v.v.i < 0 || v.v.i > 255)
	 errh->error("argument %d (%s) must be >= 0 and < 256", argno+1, desc);
       break;
     }
     
     case cpInteger:
     case cpUnsigned: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, int *);
       if (skip) break;
       if (!cp_integer(args[argno], v.v.i))
	 errh->error("argument %d should be %s (integer)", argno+1, desc);
       else if (cp_command == cpUnsigned && v.v.i < 0)
	 errh->error("argument %d (%s) must be >= 0", argno+1, desc);
       break;
     }
     
     case cpReal:
     case cpNonnegReal:
     case cpInterval: {
       const char *desc = va_arg(val, const char *);
       int frac_digits = 3; // default for cpInterval
       if (cp_command != cpInterval) frac_digits = va_arg(val, int);
       v.store = va_arg(val, int *);
       if (cp_command != cpInterval) v.store2 = va_arg(val, int *);
       if (skip) break;
       if (!cp_real(args[argno], frac_digits, v.v.real10.int_part,
		    v.v.real10.frac_part))
	 errh->error("argument %d should be %s (real)", argno+1, desc);
       else if (cp_command == cpNonnegReal
		&& (v.v.real10.int_part < 0 || v.v.real10.frac_part < 0))
	 errh->error("argument %d (%s) must be >= 0", argno+1, desc);
       else if (cp_command == cpInterval) {
	 if (v.v.real10.int_part >= 0x7FFFFFFF / 1000)
	   errh->error("overflow on argument %d (%s)", argno+1, desc);
	 v.v.i = (v.v.real10.int_part * 1000 + v.v.real10.frac_part);
       }
       break;
     }
     
     case cpNonnegReal2: {
       const char *desc = va_arg(val, const char *);
       int frac_bits = va_arg(val, int);
       v.store = va_arg(val, int *);
       assert(frac_bits > 0);
       if (skip) break;
       
       int retval = cp_real2(args[argno], frac_bits, v.v.i);
       if (retval == -2)
	 errh->error("argument %d (%s) must be >= 0", argno+1, desc);
       else if (retval == -3)
	 errh->error("overflow on argument %d (%s)", argno+1, desc);
       else if (retval < 0)
	 errh->error("argument %d should be %s (real)", argno+1, desc);
       break;
     }
     
     case cpString: {
       (void) va_arg(val, const char *);
       v.store = va_arg(val, String *);
       if (skip) break;
       v.v_string = args[argno];
       break;
     }
     
     case cpIPAddress: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       if (skip) break;
       if (!cp_ip_address(args[argno], v.v.address))
	 errh->error("argument %d should be %s (IP address)", argno+1, desc);
       break;
     }
     
     case cpEthernetAddress: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       if (skip) break;
       if (!cp_ethernet_address(args[argno], v.v.address))
	 errh->error("argument %d should be %s (Ethernet address)", argno+1,
		     desc);
       break;
     }
     
#ifdef HAVE_IPSEC
     case cpDesCblock: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       if (skip) break;
       if (!cp_des_cblock(args[argno], v.v.address))
	 errh->error("argument %d should be %s (DES encryption block)",
		     argno+1, desc);
       break;
     }
#endif

#ifndef CLICK_TOOL
     case cpElement: {
       (void) va_arg(val, const char *);
       v.store = va_arg(val, Element **);
       if (skip) break;
       const char *lookup_name = args[argno].cc();
       if (!lookup_name)
	 v.v.element = 0;
       else
	 v.v.element = cp_element(lookup_name, element, errh);
       break;
     }
#endif
     
     default:
      assert(0 && "bad cp type");
      break;
      
    }
    
    if (cp_command > cpLastControl) {
      cp_commands[argno] = (skip ? -cp_command : cp_command);
      argno++;
    }
  }
  
  if (too_few_args || argno < args.size()) {
    String signature;
    if (optional < 0)
      optional = argno;
    for (int i = 0; i < optional; i++) {
      if (i) signature += ", ";
      signature += cp_command_name(cp_commands[i]);
    }
    if (optional < argno) {
      signature += (optional > 0 ? " [" : "[");
      for (int i = optional; i < argno; i++) {
	if (i) signature += ", ";
	signature += cp_command_name(cp_commands[i]);
      }
      signature += "]";
    }
    
    const char *whoops = (too_few_args ? "few" : "many");
    errh->error("too %s arguments; expected `%s(%s)'", whoops,
#ifndef CLICK_TOOL
		String(element->class_name()).cc(),
#else
		"??",
#endif
		signature.cc());
  }
  
 done:
  
  // if success, actually set the values
  if (errh->nerrors() == nerrors_in) {
    assert(args.size() <= argno);
    for (int i = 0; i < args.size(); i++)
      store_value(cp_commands[i], values[i]);
  }
  
  delete[] cp_commands;
  delete[] values;
  return (errh->nerrors() == nerrors_in ? 0 : -1);
}

int
cp_va_parse(const String &argument,
#ifndef CLICK_TOOL
	    Element *element,
#endif
	    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> args;
  cp_argvec(argument, args);
#ifndef CLICK_TOOL
  int retval = cp_va_parsev(args, element, errh, val);
#else
  int retval = cp_va_parsev(args, errh, val);
#endif
  va_end(val);
  return retval;
}

int
cp_va_parse(Vector<String> &args,
#ifndef CLICK_TOOL
	    Element *element,
#endif
	    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
#ifndef CLICK_TOOL
  int retval = cp_va_parsev(args, element, errh, val);
#else
  int retval = cp_va_parsev(args, errh, val);
#endif
  va_end(val);
  return retval;
}

int
cp_va_space_parse(const String &argument,
#ifndef CLICK_TOOL
		  Element *element,
#endif
		  ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> args;
  cp_spacevec(argument, args);
#ifndef CLICK_TOOL
  int retval = cp_va_parsev(args, element, errh, val);
#else
  int retval = cp_va_parsev(args, errh, val);
#endif
  va_end(val);
  return retval;
}


// UNPARSING

String
cp_unparse_real(int real, int frac_bits)
{
  // XXX signed reals
  StringAccum sa;
  sa << (real >> frac_bits);
  int frac = real & ((1<<frac_bits) - 1);
  if (!frac) return sa.take_string();
  
  sa << ".";
  
  int base_ten_one = 1;
  for (; base_ten_one*10 < (1 << frac_bits); base_ten_one *= 10)
    /* do nothing */;
  int place = base_ten_one;
  base_ten_one *= 10;
  
  // slow
  int num_zeros = 0;
  while (frac && place) {
    
    for (int d = 9; d >= 1; d--) {
      // convert d*10^(-place) to binary, and compare that against frac
      int val = place * d;
      int b101 = base_ten_one / 2;
      int result = 0;
      for (int i = frac_bits - 1; i >= 0; i--, b101 /= 2)
	if (val >= b101) {
	  val -= b101;
	  result |= 1<<i;
	}
      
      if (frac >= result) {
	for (int i = 0; i < num_zeros; i++) sa << '0';
	sa << (char)(d + '0');
	frac -= result;
	goto next_place;
      }
    }
    
    num_zeros++;
    
   next_place:
    place /= 10;
  }
  
  return sa.take_string();
}

#if defined(__KERNEL__) || 1 /* always use this code for testing purposes */

String
cp_unparse_ulonglong(unsigned long long q, int base, bool uppercase)
{
  char buf[256];
  char *lastbuf = buf + 255;
  char *trav;
  
  if (base == 16 || base == 8) {
    // different code.
    const char *digits = (uppercase ? "0123456789ABCDEF" : "0123456789abcdef");
    int shift = (base == 16 ? 4 : 3);
    for (trav = lastbuf; q > 0; trav--) {
      *trav = digits[q & (base - 1)];
      q >>= shift;
    }
  } else {
    assert(base == 10);
    
    // Linux kernel sprintf can't handle %L, and doesn't have long long
    // multiply or divide. THIS REQUIRES RIGAMAROLE.
    for (trav = lastbuf; q > 0; trav--) {
      
      // k = Approx[q/10] -- know that k <= q/10
      unsigned long long k = (q >> 4) + (q >> 5) + (q >> 8) + (q >> 9)
	+ (q >> 12) + (q >> 13) + (q >> 16) + (q >> 17);
      unsigned long long m;
      
      // increase k until it exactly equals floor(q/10). on exit, m is the
      // remainder: m < 10 and q == 10*k + m.
      while (1) {
	// d = 10*k
	unsigned long long d = (k << 3) + (k << 1);
	m = q - d;
	if (m < 10) break;
	
	// delta = Approx[m/10] -- know that delta <= m/10
	unsigned long long delta = (m >> 4) + (m >> 5) + (m >> 8) + (m >> 9);
	if (m >= 0x1000)
	  delta += (m >> 12) + (m >> 13) + (m >> 16) + (m >> 17);
	
	// delta might have underflowed: add at least 1
	k += (delta ? delta : 1);
      }
      
      *trav = '0' + (unsigned)m;
      q = k;
    }
  }
  
  // make sure at least one 0 is written
  if (trav == lastbuf)
    *trav-- = '0';
  
  return String(trav + 1, lastbuf - trav);
}

#else
# error "fixme"
#endif
