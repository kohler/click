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
# include "ip6address.hh"
# include "etheraddress.hh"
# include "elements/standard/addressinfo.hh"
# define CP_CONTEXT_ARG , Element *context = 0
# define CP_PASS_CONTEXT , context
#else
# define CP_CONTEXT_ARG
# define CP_PASS_CONTEXT
#endif
#include <stdarg.h>

enum CpErrors {
  CPE_OK = 0,
  CPE_FORMAT,
  CPE_NEGATIVE,
  CPE_OVERFLOW,
  CPE_INVALID,
};
static int cp_errno;

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

bool
cp_is_space(const String &str)
{
  const char *s = str.data();
  int len = str.length();
  for (int i = 0; i < len; i++)
    if (!isspace(s[i]))
      return false;
  return true;
}

bool
cp_is_word(const String &str)
{
  const char *s = str.data();
  int len = str.length();
  for (int i = 0; i < len; i++)
    if (s[i] == '\"' || s[i] == '\'' || s[i] == ','
	|| s[i] <= 32 || s[i] >= 127)
      return false;
  return len > 0;
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

void
cp_argvec(const String &conf, Vector<String> &args)
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

    for (; i < len; i++)
      switch (s[i]) {
	
       case ',':
	goto done;
	
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

       case '\"':
	for (i++; i < len && s[i] != '\"'; i++)
	  if (s[i] == '\\')
	    i++;
	break;
	
       case '\'':
	for (i++; i < len && s[i] != '\''; i++)
	  /* nada */;
	break;
	
      }
    
   done:
    int comma_pos = i;
    if (i > start)
      sa << conf.substring(start, i - start);
    
    // remove leading & trailing spaces
    const char *data = sa.data();
    int first_pos, last_pos = sa.length();
    for (first_pos = 0; first_pos < last_pos; first_pos++)
      if (!isspace(data[first_pos]))
	break;
    for (; last_pos > first_pos; last_pos--)
      if (!isspace(data[last_pos - 1]))
	break;
    
    // add the argument if it is nonempty, or this isn't the first argument
    String arg = sa.take_string().substring(first_pos, last_pos - first_pos);
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
  for (int i = 0; i < args.size(); i++) {
    if (i) sa << ", ";
    sa << args[i];
  }
  return sa.take_string();
}

String
cp_unspacevec(const Vector<String> &args)
{
  StringAccum sa;
  for (int i = 0; i < args.size(); i++) {
    if (i) sa << " ";
    sa << args[i];
  }
  return sa.take_string();
}

String
cp_subst(const String &str)
{
  Vector<String> v;
  cp_argvec(str, v);
  if (v.size() > 1) {
    cp_errno = CPE_FORMAT;
    return String();
  } else {
    cp_errno = CPE_OK;
    return (v.size() ? v[0] : String());
  }
}

void
cp_spacevec(const String &conf, Vector<String> &vec)
{
  const char *s = conf.data();
  int len = conf.length();
  int i = 0;
  
  // common case: no configuration
  if (len == 0)
    return;

  // dump arguments into `vec'
  int start = -1;
  
  for (; i < len; i++)
    switch (s[i]) {
      
     case '/':
      // skip comments
      if (i == len - 1 || (s[i+1] != '/' && s[i+1] != '*'))
	goto normal;
      if (start >= 0)
	vec.push_back(conf.substring(start, i - start));
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
      start = -1;
      break;
      
     case '\"':
      if (start < 0)
	start = i;
      for (i++; i < len && s[i] != '\"'; i++)
	if (s[i] == '\\')
	  i++;
      break;
      
     case '\'':
      if (start < 0)
	start = i;
      for (i++; i < len && s[i] != '\''; i++)
	/* nada */;
      break;

     case '\\':			// check for \<...> strings
      if (start < 0)
	start = i;
      if (i < len - 1 && s[i+1] == '<')
	for (i += 2; i < len && s[i] != '>'; i++)
	  /* nada */;
      break;
      
     case ' ':
     case '\f':
     case '\n':
     case '\r':
     case '\t':
     case '\v':
      if (start >= 0)
	vec.push_back(conf.substring(start, i - start));
      start = -1;
      break;

     default:
     normal:
      if (start < 0)
	start = i;
      break;
      
    }

  if (start >= 0)
    vec.push_back(conf.substring(start, len - start));
}

String
cp_unquote(const String &str)
{
  const char *s = str.data();
  int len = str.length();
  int i = 0;

  // accumulate a word
  StringAccum sa;
  int start = i;
  int quote_state = 0;
  
  for (; i < len; i++)
    switch (s[i]) {
      
     case '\"':
     case '\'':
      if (quote_state == 0) {
	if (start < i) sa << str.substring(start, i - start);
	start = i + 1;
	quote_state = s[i];
      } else if (quote_state == s[i]) {
	if (start < i) sa << str.substring(start, i - start);
	start = i + 1;
	quote_state = 0;
      }
      break;
      
     case '\\':
      if (i < len - 1 && (quote_state == '\"'
			  || (quote_state == 0 && s[i+1] == '<'))) {
	sa << str.substring(start, i - start);
	switch (s[i+1]) {
	  
	 case '\r':
	  if (i < len - 2 && s[i+2] == '\n') i++;
	  /* FALLTHRU */
	 case '\n':
	  i++;
	  break;
	  
	 case 'a': sa << '\a'; i++; break;
	 case 'b': sa << '\b'; i++; break;
	 case 'f': sa << '\f'; i++; break;
	 case 'n': sa << '\n'; i++; break;
	 case 'r': sa << '\r'; i++; break;
	 case 't': sa << '\t'; i++; break;
	 case 'v': sa << '\v'; i++; break;
	  
	 case '0': case '1': case '2': case '3':
	 case '4': case '5': case '6': case '7': {
	   int c = 0, d = 0;
	   for (i++; i < len && s[i] >= '0' && s[i] <= '7' && d < 3;
		i++, d++)
	     c = c*8 + s[i] - '0';
	   sa << (char)c;
	   i--;
	   break;
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
	   i--;
	   break;
	 }
	 
	 case '<': {
	   int c = 0, d = 0;
	   for (i += 2; i < len; i++) {
	     if (s[i] == '>')
	       break;
	     else if (s[i] >= '0' && s[i] <= '9')
	       c = c*16 + s[i] - '0';
	     else if (s[i] >= 'A' && s[i] <= 'F')
	       c = c*16 + s[i] - 'A' + 10;
	     else if (s[i] >= 'a' && s[i] <= 'f')
	       c = c*16 + s[i] - 'a' + 10;
	     else
	       continue;	// space (ignore it) or random (error)
	     if (++d == 2) {
	       sa << (char)c;
	       c = d = 0;
	     }
	   }
	   break;
	 }
	 
	 default:
	  sa << s[i+1];
	  i++;
	  break;
	  
	}
	start = i + 1;
      }
      break;
      
    }

  if (start == 0)
    return str;
  else {
    sa << str.substring(start, i - start);
    return sa.take_string();
  }
}

String
cp_quote(const String &str, bool allow_newlines = false)
{
  if (!str)
    return String("\"\"");
  
  const unsigned char *s = (const unsigned char *)str.data();
  int len = str.length();
  int i = 0;
  
  StringAccum sa;
  int start = i;

  sa << '\"';
  
  for (; i < len; i++)
    switch (s[i]) {
      
     case '\\': case '\"':
      sa << str.substring(start, i - start) << '\\' << s[i];
      start = i + 1;
      break;
      
     case '\t':
      sa << str.substring(start, i - start) << "\\t";
      start = i + 1;
      break;

     case '\r':
      sa << str.substring(start, i - start) << "\\r";
      start = i + 1;
      break;

     case '\n':
      if (!allow_newlines) {
	sa << str.substring(start, i - start) << "\\n";
	start = i + 1;
      }
      break;

     default:
      if (s[i] < 32 || s[i] >= 127) {
	unsigned u = s[i];
	sa << str.substring(start, i - start)
	   << '\\' << (char)('0' + (u >> 6))
	   << (char)('0' + ((u >> 3) & 7))
	   << (char)('0' + (u & 7));
	start = i + 1;
      }
      break;
      
    }
  
  sa << str.substring(start, i - start) << '\"';
  return sa.take_string();
}

bool
cp_bool(const String &str, bool *return_value)
{
  const char *s = str.data();
  int len = str.length();
  int take;

  bool value;
  if (len >= 1 && s[0] == '0') {
    value = false;
    take = 1;
  } else if (len >= 1 && s[0] == '1') {
    value = true;
    take = 1;
  } else if (len >= 5 && strncmp(s, "false", 5) == 0) {
    value = false;
    take = 5;
  } else if (len >= 4 && strncmp(s, "true", 4) == 0) {
    value = true;
    take = 4;
  } else if (len >= 2 && strncmp(s, "no", 2) == 0) {
    value = false;
    take = 2;
  } else if (len >= 3 && strncmp(s, "yes", 3) == 0) {
    value = true;
    take = 3;
  } else
    return false;

  if (take != len)
    return false;
  else {
    *return_value = value;
    return true;
  }
}

bool
cp_integer(String str, int base, int *return_value)
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
  int value = strtol(s + i, &end, base);
  if (negative) value = -value;

  if (end - s == i)		// no characters in integer
    return false;
  
  if (network_byte_order) {
    if (end - s <= i + 2)
      /* do nothing */;
    else if (end - s <= i + 4)
      value = htons(value);
    else if (end - s <= i + 8)
      value = htonl(value);
    else
      return false;
  }

  if (end - s != len)
    return false;
  else {
    *return_value = value;
    return true;
  }
}

bool
cp_integer(String str, int *return_value)
{
  return cp_integer(str, -1, return_value);
}

bool
cp_ulong(String str, unsigned long *return_value)
{
  const char *s = str.cc();
  int len = str.length();
  char *end;

  unsigned long value = strtoul(s, &end, 10);
  
  if (end == s)        // no characters in integer
    return false;
  else if (end - s != len) 
    return false;
  else {
    *return_value = value;
    return true;
  }
}

bool
cp_real(const String &str, int frac_digits,
	int *return_int_part, int *return_frac_part)
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
  int int_part = 0;
  const char *c;
  for (c = int_s; c < int_e && c < int_e + exponent; c++)
    int_part = 10*int_part + *c - '0';
  for (c = frac_s; c < frac_e && c < frac_s + exponent; c++)
    int_part = 10*int_part + *c - '0';
  for (c = frac_e; c < frac_s + exponent; c++)
    int_part = 10*int_part;
  if (negative) int_part = -int_part;
  
  // determine fraction part
  int frac_part = 0;
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
  if (s - str.data() != str.length())
    return false;
  else {
    *return_int_part = int_part;
    *return_frac_part = frac_part;
    return true;
  }
}

bool
cp_real(const String &str, int frac_digits, int *return_value)
{
  int int_part, frac_part;
  if (!cp_real(str, frac_digits, &int_part, &frac_part)) {
    cp_errno = CPE_FORMAT;
    return false;
  }
  
  int one = 1;
  for (int i = 0; i < frac_digits; i++)
    one *= 10;
  int max = 0x7FFFFFFF / one;
  if (int_part >= 0 ? int_part >= max : -int_part >= max) {
    cp_errno = CPE_OVERFLOW;
    return false;
  }
  
  *return_value = (int_part * one + frac_part);
  cp_errno = CPE_OK;  
  return true;
}

bool
cp_real2(const String &str, int frac_bits, int *return_value)
{
  int frac_digits = 1;
  int base_ten_one = 10;
  for (; base_ten_one < (1 << frac_bits); base_ten_one *= 10)
    frac_digits++;
  
  int int_part, frac_part;
  if (!cp_real(str, frac_digits, &int_part, &frac_part)) {
    cp_errno = CPE_FORMAT;
    return false;
  } else if (int_part < 0 || frac_part < 0) {
    cp_errno = CPE_NEGATIVE;
    return false;
  } else if (int_part > (1 << (32 - frac_bits)) - 1) {
    cp_errno = CPE_OVERFLOW;
    return false;
  } else {
    int value = int_part << frac_bits;
    base_ten_one /= 2;
    for (int i = frac_bits - 1; i >= 0; i--, base_ten_one /= 2)
      if (frac_part >= base_ten_one) {
	frac_part -= base_ten_one;
	value |= (1 << i);
      }
    cp_errno = CPE_OK;
    *return_value = value;
    return true;
  }
}

bool
cp_milliseconds(const String &str, int *return_value)
{
  int v;
  if (!cp_real(str, 3, &v))
    return false;
  else if (v < 0) {
    cp_errno = CPE_NEGATIVE;
    return false;
  } else {
    *return_value = v;
    return true;
  }
}

bool
cp_string(const String &str, String *return_value, String *rest = 0)
{
  const char *s = str.data();
  int len = str.length();
  int i = 0;

  // accumulate a word
  int quote_state = 0;
  
  for (; i < len; i++)
    switch (s[i]) {
      
     case ' ':
     case '\f':
     case '\n':
     case '\r':
     case '\t':
     case '\v':
      if (quote_state == 0)
	goto done;
      break;

     case '\"':
     case '\'':
      if (quote_state == 0)
	quote_state = s[i];
      else if (quote_state == s[i])
	quote_state = 0;
      break;
      
     case '\\':
      if (i < len - 1 && s[i+1] == '<' && quote_state == 0) {
	for (i += 2; i < len && s[i] != '>'; i++)
	  /* nada */;
      }
      break;
      
    }
  
 done:
  if (i == 0 || (!rest && i != len))
    return false;
  else {
    if (rest)
      *rest = str.substring(i);
    *return_value = cp_unquote(str.substring(0, i));
    return true;
  }
}

bool
cp_word(const String &str, String *return_value, String *rest = 0)
{
  String word;
  if (!cp_string(str, &word, rest))
    return false;
  else if (!cp_is_word(word))
    return false;
  else {
    *return_value = word;
    return true;
  }
}

bool
cp_ip_address(const String &str, unsigned char *return_value
	      CP_CONTEXT_ARG)
{
  int pos = 0, part;
  const char *s = str.data();
  int len = str.length();

  unsigned char value[4];
  for (int d = 0; d < 4; d++) {
    if (d && s[pos] == '.')
      pos++;
    if (pos >= len || !isdigit(s[pos]))
      goto bad;
    for (part = 0; pos < len && isdigit(s[pos]) && part <= 255; pos++)
      part = part*10 + s[pos] - '0';
    if (part > 255)
      goto bad;
    value[d] = part;
  }

  if (pos == len) {
    memcpy(return_value, value, 4);
    return true;
  }

 bad:
#ifndef CLICK_TOOL
  return AddressInfo::query_ip(str, return_value, context);
#else
  return false;
#endif
}


static bool
bad_ip_address_mask(const String &str,
		    unsigned char *return_value, unsigned char *return_mask,
		    bool allow_bare_address
		    CP_CONTEXT_ARG)
{
#ifndef CLICK_TOOL
  if (AddressInfo::query_ip_mask(str, return_value, return_mask, context))
    return true;
  else if (allow_bare_address
	   && AddressInfo::query_ip(str, return_value, context)) {
    return_mask[0] = return_mask[1] = return_mask[2] = return_mask[3] = 255;
    return true;
  }
#else
  // shut up, compiler!
  (void)str, (void)return_value, (void)return_mask, (void)allow_bare_address;
#endif
  return false;
}

bool
cp_ip_address_mask(const String &str,
		   unsigned char *return_value, unsigned char *return_mask,
		   bool allow_bare_address
		   CP_CONTEXT_ARG)
{
  unsigned char value[4], mask[4];

  int slash = str.find_left('/');
  String ip_part, mask_part;
  if (slash >= 0) {
    ip_part = str.substring(0, slash);
    mask_part = str.substring(slash + 1);
  } else if (!allow_bare_address)
    return bad_ip_address_mask(str, return_value, return_mask, allow_bare_address CP_PASS_CONTEXT);
  else
    ip_part = str;
  
  if (!cp_ip_address(ip_part, value))
    return bad_ip_address_mask(str, return_value, return_mask, allow_bare_address CP_PASS_CONTEXT);

  // move past /
  if (allow_bare_address && !mask_part.length()) {
    memcpy(return_value, value, 4);
    return_mask[0] = return_mask[1] = return_mask[2] = return_mask[3] = 255;
    return true;
  }

  // check for complete IP address
  int relevant_bits;
  if (cp_ip_address(mask_part, mask))
    /* OK */;
  
  else if (cp_integer(mask_part, &relevant_bits)
	   && relevant_bits >= 0 && relevant_bits <= 32) {
    // set bits
    mask[0] = mask[1] = mask[2] = mask[3] = 0;
    unsigned char *pos = mask;
    unsigned char bit = 0x80;
    for (int i = 0; i < relevant_bits; i++) {
      *pos |= bit;
      bit >>= 1;
      if (!bit) {
	pos++;
	bit = 0x80;
      }
    }
    /* OK */;
    
  } else
    return bad_ip_address_mask(str, return_value, return_mask, allow_bare_address CP_PASS_CONTEXT);

  memcpy(return_value, value, 4);
  memcpy(return_mask, mask, 4);
  return true;
}

#ifndef CLICK_TOOL
bool
cp_ip_address(const String &str, IPAddress &address
	      CP_CONTEXT_ARG)
{
  return cp_ip_address(str, address.data()
		       CP_PASS_CONTEXT);
}

bool
cp_ip_address_mask(const String &str, IPAddress &address, IPAddress &mask,
		   bool allow_bare_address
		   CP_CONTEXT_ARG)
{
  return cp_ip_address_mask(str, address.data(), mask.data(),
			    allow_bare_address
			    CP_PASS_CONTEXT);
}
#endif

bool
cp_ip6_address(const String &str, unsigned char *return_value)
{
  unsigned short parts[8];
  int coloncolon = -1;
  const char *s = str.data();
  int len = str.length();
  int pos = 0;

  int d;
  int last_part_pos = 0;
  for (d = 0; d < 8; d++) {
    if (coloncolon < 0 && pos < len - 1 && s[pos] == ':' && s[pos+1] == ':') {
      coloncolon = d;
      pos += 2;
    } else if (d && pos < len - 1 && s[pos] == ':' && isxdigit(s[pos+1]))
      pos++;
    if (pos >= len || !isxdigit(s[pos]))
      break;
    unsigned part = 0;
    last_part_pos = pos;
    for (; pos < len && isxdigit(s[pos]) && part <= 0xFFFF; pos++)
      part = (part<<4) + xvalue(s[pos]);
    if (part > 0xFFFF)
      return false;
    parts[d] = part;
  }

  // check if address ends in IPv4 address
  if (pos < len && d <= 7 && s[pos] == '.') {
    unsigned char ip4a[4];
    if (cp_ip_address(str.substring(last_part_pos), ip4a)) {
      parts[d-1] = (ip4a[0]<<8) + ip4a[1];
      parts[d] = (ip4a[2]<<8) + ip4a[3];
      d++;
      pos = len;
    }
  }

  // handle zero blocks surrounding ::
  if ((d < 8 && coloncolon < 0) || (d == 8 && coloncolon >= 0))
    return false;
  else if (d < 8) {
    int num_zeros = 8 - d;
    for (int x = d - 1; x >= coloncolon; x--)
      parts[x + num_zeros] = parts[x];
    for (int x = coloncolon; x < coloncolon + num_zeros; x++)
      parts[x] = 0;
  }

  // return
  if (pos < len)
    return false;
  else {
    for (d = 0; d < 8; d++) {
      return_value[d<<1] = (parts[d]>>8) & 0xFF;
      return_value[(d<<1) + 1] = parts[d] & 0xFF;
    }
    return true;
  }
}

#ifndef CLICK_TOOL
bool
cp_ip6_address(const String &str, IP6Address &address)
{
  return cp_ip6_address(str, address.data());
}
#endif

bool
cp_ethernet_address(const String &str, unsigned char *return_value
		    CP_CONTEXT_ARG)
{
  int i = 0;
  const char *s = str.data();
  int len = str.length();

  unsigned char value[6];
  for (int d = 0; d < 6; d++) {
    if (i < len - 1 && isxdigit(s[i]) && isxdigit(s[i+1])) {
      value[d] = xvalue(s[i])*16 + xvalue(s[i+1]);
      i += 2;
    } else if (i < len && isxdigit(s[i])) {
      value[d] = xvalue(s[i]);
      i += 1;
    } else
      goto bad;
    if (d == 5) break;
    if (i >= len - 1 || s[i] != ':')
      goto bad;
    i++;
  }

  if (i == len) {
    memcpy(return_value, value, 6);
    return true;
  }

 bad:
#ifndef CLICK_TOOL
  return AddressInfo::query_ethernet(str, return_value, context);
#else
  return false;
#endif
}


#ifndef CLICK_TOOL
bool
cp_ethernet_address(const String &str, EtherAddress &address
		    CP_CONTEXT_ARG)
{
  return cp_ethernet_address(str, address.data()
			     CP_PASS_CONTEXT);
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

  unsigned char value[8];
  for (int d = 0; d < 8; d++) {
    if (i < len - 1 && isxdigit(s[i]) && isxdigit(s[i+1])) {
      value[d] = xvalue(s[i])*16 + xvalue(s[i+1]);
      i += 2;
    } else
      return false;
  }

  if (!rest && len != i)
    return false;
  else {
    if (rest)
      *rest = str.substring(i);
    memcpy(return_value, value, 8);
    return true;
  }
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
    unsigned long ul;
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
   case cpUnsignedLong: return "unsigned long";
   case cpReal: return "real";
   case cpMilliseconds: return "time in seconds";
   case cpNonnegReal: case cpNonnegFixed: return "unsigned real";
   case cpString: return "string";
   case cpWord: return "word";
   case cpArgument: return "??";
   case cpIPAddress: return "IP address";
   case cpIPAddressMask: return "IP address with netmask";
   case cpIPAddressOptMask: return "IP address with optional netmask";
   case cpIP6Address: return "IP6 address";
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
   case cpReal:
   case cpNonnegReal:
   case cpMilliseconds:
   case cpNonnegFixed: {
     int *istore = (int *)v.store;
     *istore = v.v.i;
     break;
   }
  
   case cpUnsignedLong: { 
     unsigned long *istore = (unsigned long *)v.store; 
     *istore = v.v.ul; 
     break; 
   }

   case cpString:
   case cpWord:
   case cpArgument: {
     String *sstore = (String *)v.store;
     *sstore = v.v_string;
     break;
   }
   
   case cpIPAddress:
    address_bytes = 4;
    goto address;
    
  case cpIP6Address:
    address_bytes = 16;
    goto address;

   case cpEthernetAddress:
    address_bytes = 6;
    goto address;

   case cpDesCblock:
    address_bytes = 8;
    goto address;
   
   address: {
     unsigned char *addrstore = (unsigned char *)v.store;
     memcpy(addrstore, v.v.address, address_bytes);
     break;
   }

   case cpIPAddressMask:
   case cpIPAddressOptMask: {
     unsigned char *addrstore = (unsigned char *)v.store;
     memcpy(addrstore, v.v.address, 4);
     unsigned char *maskstore = (unsigned char *)v.store2;
     memcpy(maskstore, v.v.address + 4, 4);
     break;
   }

#ifndef CLICK_TOOL
   case cpElement: {
     Element **elementstore = (Element **)v.store;
     *elementstore = v.v.element;
     break;
   }
#endif
   
   default:
    // no argument provided
    break;
    
  }
}

static int
cp_va_parsev(const Vector<String> &args,
#ifndef CLICK_TOOL
	     Element *context,
#endif
	     const char *argname, const char *separator,
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
       if (!cp_bool(args[argno], &v.v.b))
	 errh->error("%s %d should be %s (bool)", argname, argno+1, desc);
       break;
     }
     
     case cpByte: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       if (skip) break;
       if (!cp_integer(args[argno], &v.v.i))
	 errh->error("%s %d should be %s (byte)", argname, argno+1, desc);
       if (v.v.i < 0 || v.v.i > 255)
	 errh->error("%s %d (%s) must be >= 0 and < 256", argname, argno+1, desc);
       break;
     }
     
     case cpInteger:
     case cpUnsigned: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, int *);
       if (skip) break;
       if (!cp_integer(args[argno], &v.v.i))
	 errh->error("%s %d should be %s (integer)", argname, argno+1, desc);
       else if (cp_command == cpUnsigned && v.v.i < 0)
	 errh->error("%s %d (%s) must be >= 0", argname, argno+1, desc);
       break;
     }

     case cpUnsignedLong: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, int *);
       if (skip) break;
       if (!cp_ulong(args[argno], &v.v.ul))
	 errh->error("%s %d should be %s (unsigned long)", argname, argno+1, desc);
       break;
     }
     
     case cpReal:
     case cpNonnegReal: {
       const char *desc = va_arg(val, const char *);
       int frac_digits = va_arg(val, int);
       v.store = va_arg(val, int *);
       if (skip) break;
       if (!cp_real(args[argno], frac_digits, &v.v.i)) {
	 if (cp_errno == CPE_OVERFLOW)
	   errh->error("overflow on %s %d (%s)", argname, argno+1, desc);
	 else
	   errh->error("%s %d should be %s (real)", argname, argno+1, desc);
       } else if (cp_command == cpNonnegReal && v.v.i < 0)
	 errh->error("%s %d (%s) must be >= 0", argname, argno+1, desc);
       break;
     }
     
     case cpMilliseconds: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, int *);
       if (skip) break;
       if (!cp_milliseconds(args[argno], &v.v.i)) {
	 if (cp_errno == CPE_OVERFLOW)
	   errh->error("overflow on %s %d (%s)", argname, argno+1, desc);
	 else if (cp_errno == CPE_NEGATIVE)
	   errh->error("%s %d (%s) must be >= 0", argname, argno+1, desc);
	 else
	   errh->error("%s %d should be %s (real)", argname, argno+1, desc);
       }
       break;
     }
     
     case cpNonnegFixed: {
       const char *desc = va_arg(val, const char *);
       int frac_bits = va_arg(val, int);
       v.store = va_arg(val, int *);
       assert(frac_bits > 0);
       if (skip) break;
       
       if (!cp_real2(args[argno], frac_bits, &v.v.i)) {
	 if (cp_errno == CPE_NEGATIVE)
	   errh->error("%s %d (%s) must be >= 0", argname, argno+1, desc);
	 else if (cp_errno == CPE_OVERFLOW)
	   errh->error("overflow on %s %d (%s)", argname, argno+1, desc);
	 else
	   errh->error("%s %d should be %s (real)", argname, argno+1, desc);
       }
       break;
     }
     
     case cpString: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, String *);
       if (skip) break;
       if (!cp_string(args[argno], &v.v_string))
	 errh->error("%s %d should be %s (string)", argname, argno+1, desc);
       break;
     }
     
     case cpWord: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, String *);
       if (skip) break;
       if (!cp_word(args[argno], &v.v_string))
	 errh->error("%s %d should be %s (word)", argname, argno+1, desc);
       break;
     }
     
     case cpArgument: {
       (void)va_arg(val, const char *);
       v.store = va_arg(val, String *);
       if (skip) break;
       v.v_string = args[argno];
       break;
     }
     
     case cpIPAddress: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       if (skip) break;
       if (!cp_ip_address(args[argno], v.v.address CP_PASS_CONTEXT))
	 errh->error("%s %d should be %s (IP address)", argname, argno+1, desc);
       break;
     }     
    
     case cpIPAddressMask:
     case cpIPAddressOptMask: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       v.store2 = va_arg(val, unsigned char *);
       if (skip) break;
       bool mask_optional = (cp_command == cpIPAddressOptMask);
       if (!cp_ip_address_mask(args[argno], v.v.address, v.v.address + 4, mask_optional CP_PASS_CONTEXT))
	 errh->error("%s %d should be %s (IP network address)", argname, argno+1, desc);
       break;
     }
     
      case cpIP6Address: {
      const char *desc = va_arg(val, const char *);
      v.store = va_arg(val, unsigned char *);  
      if (skip) break;
      if (!cp_ip6_address(args[argno], (unsigned char *)v.v.address))
	errh->error("%s %d should be %s (IP6 address)", argname, argno+1, desc);
      else
	break;
     }

     case cpEthernetAddress: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       if (skip) break;
       if (!cp_ethernet_address(args[argno], v.v.address CP_PASS_CONTEXT))
	 errh->error("%s %d should be %s (Ethernet address)", argname, argno+1,
		     desc);
       break;
     }
     
#ifdef HAVE_IPSEC
     case cpDesCblock: {
       const char *desc = va_arg(val, const char *);
       v.store = va_arg(val, unsigned char *);
       if (skip) break;
       if (!cp_des_cblock(args[argno], v.v.address))
	 errh->error("%s %d should be %s (DES encryption block)", argname,
		     argno+1, desc);
       break;
     }
#endif

#ifndef CLICK_TOOL
     case cpElement: {
       (void) va_arg(val, const char *);
       v.store = va_arg(val, Element **);
       if (skip) break;
       const String &name = args[argno];
       if (!name)
	 v.v.element = 0;
       else
	 v.v.element = cp_element(name, context, errh);
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
      if (i) signature += separator;
      signature += cp_command_name(cp_commands[i]);
    }
    if (optional < argno) {
      signature += (optional > 0 ? " [" : "[");
      for (int i = optional; i < argno; i++) {
	if (i) signature += separator;
	signature += cp_command_name(cp_commands[i]);
      }
      signature += "]";
    }
    
    const char *whoops = (too_few_args ? "few" : "many");
    errh->error("too %s %ss; expected `%s'", whoops, argname, signature.cc());
  }
  
 done:
  
  // if success, actually set the values
  if (errh->nerrors() == nerrors_in) {
    for (int i = 0; i < args.size() && i < argno; i++)
      store_value(cp_commands[i], values[i]);
  }
  
  delete[] cp_commands;
  delete[] values;
  return (errh->nerrors() == nerrors_in ? 0 : -1);
}

int
cp_va_parse(const Vector<String> &conf,
#ifndef CLICK_TOOL
	    Element *context,
#endif
	    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
#ifndef CLICK_TOOL
  int retval = cp_va_parsev(conf, context, "argument", ", ", errh, val);
#else
  int retval = cp_va_parsev(conf, "argument", ", ", errh, val);
#endif
  va_end(val);
  return retval;
}

int
cp_va_parse(const String &confstr,
#ifndef CLICK_TOOL
	    Element *context,
#endif
	    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> conf;
  cp_argvec(confstr, conf);
#ifndef CLICK_TOOL
  int retval = cp_va_parsev(conf, context, "argument", ", ", errh, val);
#else
  int retval = cp_va_parsev(conf, "argument", ", ", errh, val);
#endif
  va_end(val);
  return retval;
}

int
cp_va_space_parse(const String &argument,
#ifndef CLICK_TOOL
		  Element *context,
#endif
		  ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> args;
  cp_spacevec(argument, args);
#ifndef CLICK_TOOL
  int retval = cp_va_parsev(args, context, "word", " ", errh, val);
#else
  int retval = cp_va_parsev(args, "word", " ", errh, val);
#endif
  va_end(val);
  return retval;
}


// UNPARSING

String
cp_unparse_bool(bool b)
{
  return String(b ? "true" : "false");
}

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

String
cp_unparse_ulonglong(unsigned long long q, int base, bool uppercase)
{
  // Unparse an unsigned long long. Linux kernel sprintf can't handle %L,
  // so we provide our own function.
  
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
