/*
 * confparse.{cc,hh} -- configuration string parsing
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/ipaddress.hh>
#include <click/ipaddressset.hh>
#include <click/ip6address.hh>
#include <click/etheraddress.hh>
#ifndef CLICK_TOOL
# include <click/router.hh>
# include "elements/standard/addressinfo.hh"
# define CP_CONTEXT_ARG , Element *context = 0
# define CP_PASS_CONTEXT , context
#else
# define CP_CONTEXT_ARG
# define CP_PASS_CONTEXT
#endif
#include <stdarg.h>

int cp_errno;

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

static void
cp_argvec(const String &conf, Vector<String> &args, bool split_comma)
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
	if (split_comma)
	  goto done;
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

       case '\"':
	for (i++; i < len && s[i] != '\"'; i++)
	  if (s[i] == '\\' && i < len - 1)
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

void
cp_argvec(const String &str, Vector<String> &v)
{
  cp_argvec(str, v, true);
}

String
cp_uncomment(const String &str)
{
  Vector<String> v;
  cp_argvec(str, v, false);
  return (v.size() ? v[0] : String());
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
      
     case '\\': case '\"': case '$':
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
  
  if (len == 1 && s[0] == '0')
    *return_value = false;
  else if (len == 1 && s[0] == '1')
    *return_value = true;
  else if (len == 5 && memcmp(s, "false", 5) == 0)
    *return_value = false;
  else if (len == 4 && memcmp(s, "true", 4) == 0)
    *return_value = true;
  else if (len == 2 && memcmp(s, "no", 2) == 0)
    *return_value = false;
  else if (len == 3 && memcmp(s, "yes", 3) == 0)
    *return_value = true;
  else
    return false;

  return true;
}

bool
cp_unsigned(const String &str, int base, unsigned *return_value)
{
  const char *s = str.data();
  int len = str.length();
  int i = 0;
  
  if (i < len && s[i] == '+')
    i++;

  if ((base <= 0 || base == 16) && i < len - 1
      && s[i] == '0' && (s[i+1] == 'x' || s[i+1] == 'X')) {
    i += 2;
    base = 16;
  } else if (base <= 0 && s[i] == '0')
    base = 8;
  else if (base <= 0)
    base = 10;

  if (i == len)			// no digits
    return false;
  
  unsigned val = 0;
  cp_errno = CPE_OK;
  while (i < len) {
    unsigned new_val = val * base;
    if (s[i] >= '0' && s[i] <= '9' && s[i] - '0' < base)
      new_val += s[i] - '0';
    else if (s[i] >= 'A' && s[i] <= 'Z' && s[i] - 'A' + 10 < base)
      new_val += s[i] - 'A' + 10;
    else if (s[i] >= 'a' && s[i] <= 'z' && s[i] - 'a' + 10 < base)
      new_val += s[i] - 'a' + 10;
    else
      break;
    if (new_val < val)
      cp_errno = CPE_OVERFLOW;
    val = new_val;
    i++;
  }

  if (i != len)			// bad characters
    return false;
  else {
    *return_value = (cp_errno ? 0xFFFFFFFFU : val);
    return true;
  }
}

bool
cp_unsigned(const String &str, unsigned *return_value)
{
  return cp_unsigned(str, 0, return_value);
}

bool
cp_integer(const String &str, int base, int *return_value)
{
  unsigned value;
  bool negative = false;
  bool ok;
  if (str.length() == 0)
    return false;
  else if (str[0] == '-') {
    negative = true;
    ok = cp_unsigned(str.substring(1), base, &value);
  } else
    ok = cp_unsigned(str, base, &value);

  if (!ok)
    return false;
  if (cp_errno == CPE_OVERFLOW)
    *return_value = 0x7FFFFFFF;
  else if (!negative && value >= 0x80000000U) {
    cp_errno = CPE_OVERFLOW;
    *return_value = 0x7FFFFFFF;
  } else if (negative && value > 0x80000000U) {
    cp_errno = CPE_OVERFLOW;
    *return_value = 0x80000000;
  } else if (negative)
    *return_value = -value;
  else
    *return_value = value;
  return true;
}

bool
cp_integer(const String &str, int *return_value)
{
  return cp_integer(str, -1, return_value);
}

bool
cp_real(const String &str, int frac_digits,
	int *return_int_part, int *return_frac_part)
{
  const char *s = str.data();
  const char *last = s + str.length();
  cp_errno = CPE_FORMAT;
  if (s == last)
    return false;
  if (frac_digits > 9) {
    cp_errno = CPE_INVALID;
    return false;
  }
  
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
  if (int_s == int_e && frac_s == frac_e)
    return false;
  
  // find exponent, if any
  int exponent = 0;
  if (s < last && (*s == 'E' || *s == 'e')) {
    if (++s == last)
      return false;
    bool negexp = (*s == '-');
    if (*s == '-' || *s == '+') s++;
    if (s >= last || !isdigit(*s))
      return false;
    
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
    cp_errno = CPE_OK;
    return true;
  }
}

bool
cp_real(const String &str, int frac_digits, int *return_value)
{
  int int_part, frac_part;
  if (!cp_real(str, frac_digits, &int_part, &frac_part))
    return false;
  
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
  if (frac_bits >= 29) {
    cp_errno = CPE_INVALID;
    return false;
  }
  
  int int_part, frac_part;
  if (!cp_real(str, 9, &int_part, &frac_part)) {
    cp_errno = CPE_FORMAT;
    return false;
  } else if (int_part < 0 || frac_part < 0) {
    cp_errno = CPE_NEGATIVE;
    return false;
  } else if (int_part > (1 << (32 - frac_bits)) - 1) {
    cp_errno = CPE_OVERFLOW;
    return false;
  } else {
    // method from Knuth's TeX, round_decimals. Works well with
    // cp_unparse_real below
    int fraction = 0;
    unsigned two = 2 << frac_bits;
    for (int i = 0; i < 9; i++) {
      unsigned digit = frac_part % 10;
      fraction = (fraction + digit*two) / 10;
      frac_part /= 10;
    }
    fraction = (fraction + 1) / 2;
    cp_errno = CPE_OK;
    *return_value = (int_part << frac_bits) + fraction;
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
      } else if (i < len - 1 && quote_state == '\"')
	i++;
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

static bool
cp_keyword(const String &str, String *return_value, String *rest)
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
     case '=':
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

     default:
      if (quote_state == 0 && !isalnum(s[i]) && s[i] != '_' && s[i] != '-'
	  && s[i] != '.')
	return false;
      break;
      
    }
  
 done:
  if (i == 0 || quote_state != 0)
    return false;

  *return_value = cp_unquote(str.substring(0, i));

  if (i < len && s[i] == '=')
    i++;
  for (; i < len; i++)
    if (!isspace(s[i]))
      break;
  *rest = str.substring(i);
  return true;
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
    if (d && pos < len && s[pos] == '.')
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
bad_ip_prefix(const String &str,
	      unsigned char *return_value, unsigned char *return_mask,
	      bool allow_bare_address
	      CP_CONTEXT_ARG)
{
#ifndef CLICK_TOOL
  if (AddressInfo::query_ip_prefix(str, return_value, return_mask, context))
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
cp_ip_prefix(const String &str,
	     unsigned char *return_value, unsigned char *return_mask,
	     bool allow_bare_address  CP_CONTEXT_ARG)
{
  unsigned char value[4], mask[4];

  int slash = str.find_right('/');
  String ip_part, mask_part;
  if (slash >= 0) {
    ip_part = str.substring(0, slash);
    mask_part = str.substring(slash + 1);
  } else if (!allow_bare_address)
    return bad_ip_prefix(str, return_value, return_mask, allow_bare_address CP_PASS_CONTEXT);
  else
    ip_part = str;
  
  if (!cp_ip_address(ip_part, value  CP_PASS_CONTEXT))
    return bad_ip_prefix(str, return_value, return_mask, allow_bare_address CP_PASS_CONTEXT);

  // move past /
  if (allow_bare_address && !mask_part.length()) {
    memcpy(return_value, value, 4);
    return_mask[0] = return_mask[1] = return_mask[2] = return_mask[3] = 255;
    return true;
  }

  // check for complete IP address
  int relevant_bits;
  if (cp_ip_address(mask_part, mask  CP_PASS_CONTEXT))
    /* OK */;
  
  else if (cp_integer(mask_part, &relevant_bits)
	   && relevant_bits >= 0 && relevant_bits <= 32) {
    // set bits
    unsigned umask = 0;
    if (relevant_bits > 0)
      umask = 0xFFFFFFFFU << (32 - relevant_bits);
    for (int i = 0; i < 4; i++, umask <<= 8)
      mask[i] = (umask >> 24) & 255;
    
  } else
    return bad_ip_prefix(str, return_value, return_mask, allow_bare_address CP_PASS_CONTEXT);

  memcpy(return_value, value, 4);
  memcpy(return_mask, mask, 4);
  return true;
}

bool
cp_ip_address(const String &str, IPAddress *address
	      CP_CONTEXT_ARG)
{
  return cp_ip_address(str, address->data()
		       CP_PASS_CONTEXT);
}

bool
cp_ip_prefix(const String &str, IPAddress *address, IPAddress *mask,
	     bool allow_bare_address  CP_CONTEXT_ARG)
{
  return cp_ip_prefix(str, address->data(), mask->data(),
		      allow_bare_address  CP_PASS_CONTEXT);
}

bool
cp_ip_prefix(const String &str, unsigned char *address, unsigned char *mask
	     CP_CONTEXT_ARG)
{
  return cp_ip_prefix(str, address, mask,
		      false  CP_PASS_CONTEXT);
}

bool
cp_ip_prefix(const String &str, IPAddress *address, IPAddress *mask
	     CP_CONTEXT_ARG)
{
  return cp_ip_prefix(str, address->data(), mask->data(),
		      false  CP_PASS_CONTEXT);
}

bool
cp_ip_address_set(const String &str, IPAddressSet *set
		  CP_CONTEXT_ARG)
{
  Vector<String> words;
  Vector<unsigned> additions;
  IPAddress ip;
  cp_spacevec(str, words);
  for (int i = 0; i < words.size(); i++) {
    if (!cp_ip_address(words[i], &ip  CP_PASS_CONTEXT))
      return false;
    additions.push_back(ip);
  }
  for (int i = 0; i < additions.size(); i++)
    set->insert(IPAddress(additions[i]));
  return true;
}


static bool
bad_ip6_address(const String &str, unsigned char *return_value
		CP_CONTEXT_ARG)
{
#ifndef CLICK_TOOL
  return AddressInfo::query_ip6(str, return_value, context);
#else
  (void)str, (void)return_value;
  return false;
#endif
}

bool
cp_ip6_address(const String &str, unsigned char *return_value
	       CP_CONTEXT_ARG)
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
      return bad_ip6_address(str, return_value  CP_PASS_CONTEXT);
    parts[d] = part;
  }

  // check if address ends in IPv4 address
  if (pos < len && d <= 7 && s[pos] == '.') {
    unsigned char ip4a[4];
    if (cp_ip_address(str.substring(last_part_pos), ip4a  CP_PASS_CONTEXT)) {
      parts[d-1] = (ip4a[0]<<8) + ip4a[1];
      parts[d] = (ip4a[2]<<8) + ip4a[3];
      d++;
      pos = len;
    }
  }

  // handle zero blocks surrounding ::
  if ((d < 8 && coloncolon < 0) || (d == 8 && coloncolon >= 0))
    return bad_ip6_address(str, return_value  CP_PASS_CONTEXT);
  else if (d < 8) {
    int num_zeros = 8 - d;
    for (int x = d - 1; x >= coloncolon; x--)
      parts[x + num_zeros] = parts[x];
    for (int x = coloncolon; x < coloncolon + num_zeros; x++)
      parts[x] = 0;
  }

  // return
  if (pos < len)
    return bad_ip6_address(str, return_value  CP_PASS_CONTEXT);
  else {
    for (d = 0; d < 8; d++) {
      return_value[d<<1] = (parts[d]>>8) & 0xFF;
      return_value[(d<<1) + 1] = parts[d] & 0xFF;
    }
    return true;
  }
}


static bool
bad_ip6_prefix(const String &str,
	      unsigned char *return_value, int *return_bits,
	      bool allow_bare_address
	      CP_CONTEXT_ARG)
{
#ifndef CLICK_TOOL
  if (AddressInfo::query_ip6_prefix(str, return_value, return_bits, context))
    return true;
  else if (allow_bare_address
	   && AddressInfo::query_ip6(str, return_value, context)) {
    *return_bits = 128;
    return true;
  }
#else
  // shut up, compiler!
  (void)str, (void)return_value, (void)return_bits, (void)allow_bare_address;
#endif
  return false;
}

bool
cp_ip6_prefix(const String &str,
	      unsigned char *return_value, int *return_bits,
	      bool allow_bare_address  CP_CONTEXT_ARG)
{
  unsigned char value[16], mask[16];

  int slash = str.find_right('/');
  String ip_part, mask_part;
  if (slash >= 0) {
    ip_part = str.substring(0, slash);
    mask_part = str.substring(slash + 1);
  } else if (!allow_bare_address)
    return bad_ip6_prefix(str, return_value, return_bits, allow_bare_address CP_PASS_CONTEXT);
  else
    ip_part = str;
  
  if (!cp_ip6_address(ip_part, value  CP_PASS_CONTEXT))
    return bad_ip6_prefix(str, return_value, return_bits, allow_bare_address CP_PASS_CONTEXT);

  // move past /
  if (allow_bare_address && !mask_part.length()) {
    memcpy(return_value, value, 16);
    *return_bits = 64;
    return true;
  }

  // check for complete IP address
  int relevant_bits = 0;
  if (cp_ip6_address(mask_part, mask  CP_PASS_CONTEXT)) {
    // check that it really is a prefix. if not, return false right away
    // (don't check with AddressInfo)
    int pos = 0;
    for (; pos < 16 && mask[pos] == 255; pos++)
      relevant_bits += 8;
    if (pos < 16) {
      int comp_plus_1 = ((~mask[pos]) & 255) + 1;
      for (int i = 0; i < 8; i++)
	if (comp_plus_1 == (1 << (8-i))) {
	  relevant_bits += i;
	  pos++;
	  break;
	}
    }
    for (; pos < 16 && mask[pos] == 0; pos++)
      pos++;
    if (pos < 16)
      return false;
    
  } else if (cp_integer(mask_part, &relevant_bits)
	     && relevant_bits >= 0 && relevant_bits <= 128)
    /* OK */;
    
  else
    return bad_ip6_prefix(str, return_value, return_bits, allow_bare_address CP_PASS_CONTEXT);

  memcpy(return_value, value, 16);
  *return_bits = relevant_bits;
  return true;
}

bool
cp_ip6_prefix(const String &str, unsigned char *address, unsigned char *mask,
	      bool allow_bare_address  CP_CONTEXT_ARG)
{
  int bits;
  if (cp_ip6_prefix(str, address, &bits, allow_bare_address  CP_PASS_CONTEXT)) {
    IP6Address m = IP6Address::make_prefix(bits);
    memcpy(mask, m.data(), 16);
    return true;
  } else
    return false;
}

bool
cp_ip6_prefix(const String &str, unsigned char *address, unsigned char *mask
	      CP_CONTEXT_ARG)
{
  int bits;
  if (cp_ip6_prefix(str, address, &bits, false  CP_PASS_CONTEXT)) {
    IP6Address m = IP6Address::make_prefix(bits);
    memcpy(mask, m.data(), 16);
    return true;
  } else
    return false;
}

bool
cp_ip6_prefix(const String &str, IP6Address *address, IP6Address *mask
	      CP_CONTEXT_ARG)
{
  int bits;
  if (cp_ip6_prefix(str, address->data(), &bits, false  CP_PASS_CONTEXT)) {
    *mask = IP6Address::make_prefix(bits);
    return true;
  } else
    return false;
}

bool
cp_ip6_address(const String &str, IP6Address *address
	       CP_CONTEXT_ARG)
{
  return cp_ip6_address(str, address->data()  CP_PASS_CONTEXT);
}

bool
cp_ip6_prefix(const String &str, IP6Address *address, IP6Address *prefix,
	      bool allow_bare_address  CP_CONTEXT_ARG)
{
  return cp_ip6_prefix(str, address->data(), prefix->data(), allow_bare_address
		       CP_PASS_CONTEXT);
}


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

bool
cp_ethernet_address(const String &str, EtherAddress *address
		    CP_CONTEXT_ARG)
{
  return cp_ethernet_address(str, address->data()
			     CP_PASS_CONTEXT);
}


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

CpVaParseCmd
  cpOptional		= "OPTIONAL",
  cpKeywords		= "KEYWORDS",
  cpIgnore		= "IGNORE",
  cpIgnoreRest		= "IGNORE_REST",
  cpArgument		= "arg",
  cpString		= "string",
  cpWord		= "word",
  cpBool		= "bool",
  cpByte		= "byte",
  cpShort		= "short",
  cpUnsignedShort	= "u_short",
  cpInteger		= "int",
  cpUnsigned		= "u_int",
  cpReal		= "real",
  cpNonnegReal		= "u_real",
  cpNonnegFixed		= "u_real_fixed",
  cpMilliseconds	= "msec",
  cpIPAddress		= "ip_addr",
  cpIPPrefix		= "ip_prefix",
  cpIPAddressOrPrefix	= "ip_addr_or_prefix",
  cpIPAddressSet	= "ip_addr_set",
  cpEthernetAddress	= "ether_addr",
  cpElement		= "element",
  cpIP6Address		= "ip6_addr",
  cpIP6Prefix		= "ip6_prefix",
  cpIP6AddressOrPrefix	= "ip6_addr_or_prefix",
  cpDesCblock		= "des_cblock";

enum {
  cpiEnd = 0,
  cpiOptional,
  cpiKeywords,
  cpiIgnore,
  cpiIgnoreRest,
  cpiArgument,
  cpiString,
  cpiWord,
  cpiBool,
  cpiByte,
  cpiShort,
  cpiUnsignedShort,
  cpiInteger,
  cpiUnsigned,
  cpiReal,
  cpiNonnegReal,
  cpiNonnegFixed,
  cpiMilliseconds,
  cpiIPAddress,
  cpiIPPrefix,
  cpiIPAddressOrPrefix,
  cpiIPAddressSet,
  cpiEthernetAddress,
  cpiElement,
  cpiIP6Address,
  cpiIP6Prefix,
  cpiIP6AddressOrPrefix,
  cpiDesCblock
};

#define NARGTYPE_HASH 128
static cp_argtype *argtype_hash[NARGTYPE_HASH];

static inline int
argtype_bucket(const char *command)
{
  const unsigned char *s = (const unsigned char *)command;
  return (s[0] ? (s[0]%32 + strlen(command)*32) % NARGTYPE_HASH : 0);
}

static cp_argtype *
find_argtype(const char *command)
{
  cp_argtype *t = argtype_hash[argtype_bucket(command)];
  while (t && strcmp(t->name, command) != 0)
    t = t->next;
  return t;
}

static int
cp_register_argtype(const char *name, const char *desc, int extra,
		    cp_parsefunc parse, cp_storefunc store, int internal)
{
  if (cp_argtype *t = find_argtype(name)) {
    t->use_count++;
    if (strcmp(desc, t->description) != 0
	|| extra != t->extra
	|| parse != t->parse
	|| store != t->store
	|| internal != t->internal)
      return -1;
    else
      return 0;
  }
  
  if (cp_argtype *t = new cp_argtype) {
    t->name = name;
    t->parse = parse;
    t->store = store;
    t->extra = extra;
    t->description = desc;
    t->internal = internal;
    t->use_count = 1;
    int bucket = argtype_bucket(name);
    t->next = argtype_hash[bucket];
    argtype_hash[bucket] = t;
    return 0;
  } else
    return -2;
}

int
cp_register_argtype(const char *name, const char *desc, int extra,
	       cp_parsefunc parse, cp_storefunc store)
{
  return cp_register_argtype(name, desc, extra, parse, store, -1);
}

void
cp_unregister_argtype(const char *name)
{
  cp_argtype **prev = &argtype_hash[argtype_bucket(name)];
  cp_argtype *trav = *prev;
  while (trav && strcmp(trav->name, name) != 0) {
    prev = &trav->next;
    trav = trav->next;
  }
  if (trav) {
    trav->use_count--;
    if (trav->use_count <= 0) {
      *prev = trav->next;
      delete trav;
    }
  }
}

static void
default_parsefunc(cp_value *v, const String &arg,
		  ErrorHandler *errh, const char *argname  CP_CONTEXT_ARG)
{
  const char *desc = v->description;
  int underflower = -0x80000000;
  unsigned overflower = 0xFFFFFFFFU;
  const cp_argtype *argtype = v->argtype;
  
  switch (argtype->internal) {
    
   case cpiArgument:
    v->v_string = arg;
    break;
    
   case cpiString:
    if (!cp_string(arg, &v->v_string))
      errh->error("%s should be %s (string)", argname, desc);
    break;
    
   case cpiWord:
    if (!cp_word(arg, &v->v_string))
      errh->error("%s should be %s (word)", argname, desc);
    break;
    
   case cpiBool:
    if (!cp_bool(arg, &v->v.b))
      errh->error("%s should be %s (bool)", argname, desc);
    break;

   case cpiByte:
    overflower = 255;
    goto handle_unsigned;

   case cpiShort:
    underflower = -0x8000;
    overflower = 0x7FFF;
    goto handle_signed;

   case cpiUnsignedShort:
    overflower = 0xFFFF;
    goto handle_unsigned;
    
   case cpiInteger:
    underflower = -0x80000000;
    overflower = 0x7FFFFFFF;
    goto handle_signed;

   case cpiUnsigned:
    overflower = 0xFFFFFFFFU;
    goto handle_unsigned;

   handle_signed:
    if (!cp_integer(arg, &v->v.i))
      errh->error("%s should be %s (%s)", argname, desc, argtype->description);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("integer overflow on %s (%s)", argname, desc);
    else if (v->v.i < underflower)
      errh->error("%s (%s) must be >= %d", argname, desc, underflower);
    else if (v->v.i > (int)overflower)
      errh->error("%s (%s) must be <= %u", argname, desc, overflower);
    break;

   handle_unsigned:
    if (!cp_unsigned(arg, &v->v.u))
      errh->error("%s should be %s (%s)", argname, desc, argtype->description);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("integer overflow on %s (%s)", argname, desc);
    else if (v->v.u > overflower)
      errh->error("%s (%s) must be <= %u", argname, desc, overflower);
    break;

   case cpiReal:
   case cpiNonnegReal:
    if (!cp_real(arg, v->extra, &v->v.i)) {
      if (cp_errno == CPE_OVERFLOW)
	errh->error("overflow on %s (%s)", argname, desc);
      else
	errh->error("%s should be %s (real)", argname, desc);
    } else if (argtype->internal == cpiNonnegReal && v->v.i < 0)
      errh->error("%s (%s) must be >= 0", argname, desc);
    break;

   case cpiMilliseconds:
    if (!cp_milliseconds(arg, &v->v.i)) {
      if (cp_errno == CPE_OVERFLOW)
	errh->error("overflow on %s (%s)", argname, desc);
      else if (cp_errno == CPE_NEGATIVE)
	errh->error("%s (%s) must be >= 0", argname, desc);
      else
	errh->error("%s should be %s (time in seconds)", argname, desc);
    }
    break;

   case cpiNonnegFixed:
    assert(v->extra > 0);
    if (!cp_real2(arg, v->extra, &v->v.i)) {
      if (cp_errno == CPE_NEGATIVE)
	errh->error("%s (%s) must be >= 0", argname, desc);
      else if (cp_errno == CPE_OVERFLOW)
	errh->error("overflow on %s (%s)", argname, desc);
      else if (cp_errno == CPE_INVALID)
	errh->error("%s (%s) is an invalid real", argname, desc);
      else
	errh->error("%s should be %s (real)", argname, desc);
    }
    break;

   case cpiIPAddress:
    if (!cp_ip_address(arg, v->v.address CP_PASS_CONTEXT))
      errh->error("%s should be %s (IP address)", argname, desc);
    break;

   case cpiIPPrefix:
   case cpiIPAddressOrPrefix: {
     bool mask_optional = (argtype->internal == cpiIPAddressOrPrefix);
     if (!cp_ip_prefix(arg, v->v.address, v->v.address + 4, mask_optional CP_PASS_CONTEXT))
       errh->error("%s should be %s (IP address prefix)", argname, desc);
     break;
   }

   case cpiIPAddressSet: {
     IPAddressSet crap;
     if (!cp_ip_address_set(arg, &crap CP_PASS_CONTEXT))
       errh->error("%s should be %s (set of IP addresses)", argname, desc);
     else
       v->v_string = arg;
     break;
   }     

   case cpiIP6Address:
    if (!cp_ip6_address(arg, (unsigned char *)v->v.address))
      errh->error("%s should be %s (IPv6 address)", argname, desc);
    break;

   case cpiIP6Prefix:
   case cpiIP6AddressOrPrefix: {
     bool mask_optional = (argtype->internal == cpiIP6AddressOrPrefix);
     if (!cp_ip6_prefix(arg, v->v.address, v->v.address + 16, mask_optional CP_PASS_CONTEXT))
       errh->error("%s should be %s (IPv6 address prefix)", argname, desc);
     break;
   }

   case cpiEthernetAddress:
    if (!cp_ethernet_address(arg, v->v.address CP_PASS_CONTEXT))
      errh->error("%s should be %s (Ethernet address)", argname, desc);
    break;

#ifdef HAVE_IPSEC
   case cpiDesCblock:
    if (!cp_des_cblock(arg, v->v.address))
      errh->error("%s should be %s (DES encryption block)", argname, desc);
    break;
#endif

#ifndef CLICK_TOOL
   case cpiElement:
    if (!arg)
      v->v.element = 0;
    else
      v->v.element = cp_element(arg, context, errh);
    break;
#endif
    
  }
}

static void
default_storefunc(cp_value *v  CP_CONTEXT_ARG)
{
  int address_bytes;
  const cp_argtype *argtype = v->argtype;
  
  switch (argtype->internal) {
    
   case cpiBool: {
     bool *bstore = (bool *)v->store;
     *bstore = v->v.b;
     break;
   }
   
   case cpiByte: {
     unsigned char *ucstore = (unsigned char *)v->store;
     *ucstore = v->v.i;
     break;
   }

   case cpiShort: {
     short *sstore = (short *)v->store;
     *sstore = v->v.i;
     break;
   }

   case cpiUnsignedShort: {
     unsigned short *usstore = (unsigned short *)v->store;
     *usstore = v->v.u;
     break;
   }
   
   case cpiInteger:
   case cpiReal:
   case cpiNonnegReal:
   case cpiMilliseconds:
   case cpiNonnegFixed: {
     int *istore = (int *)v->store;
     *istore = v->v.i;
     break;
   }
  
   case cpiUnsigned: { 
     unsigned *ustore = (unsigned *)v->store; 
     *ustore = v->v.u; 
     break; 
   }

   case cpiString:
   case cpiWord:
   case cpiArgument: {
     String *sstore = (String *)v->store;
     *sstore = v->v_string;
     break;
   }
   
   case cpiIPAddress:
    address_bytes = 4;
    goto address;
    
   case cpiIP6Address:
    address_bytes = 16;
    goto address;

   case cpiEthernetAddress:
    address_bytes = 6;
    goto address;

   case cpiDesCblock:
    address_bytes = 8;
    goto address;
   
   address: {
     unsigned char *addrstore = (unsigned char *)v->store;
     memcpy(addrstore, v->v.address, address_bytes);
     break;
   }

   case cpiIPPrefix:
   case cpiIPAddressOrPrefix: {
     unsigned char *addrstore = (unsigned char *)v->store;
     memcpy(addrstore, v->v.address, 4);
     unsigned char *maskstore = (unsigned char *)v->store2;
     memcpy(maskstore, v->v.address + 4, 4);
     break;
   }

   case cpiIP6Prefix:
   case cpiIP6AddressOrPrefix: {
     unsigned char *addrstore = (unsigned char *)v->store;
     memcpy(addrstore, v->v.address, 16);
     unsigned char *maskstore = (unsigned char *)v->store2;
     memcpy(maskstore, v->v.address + 16, 16);
     break;
   }

   case cpiIPAddressSet: {
     // oog... parse set into stored set only when we know there are no errors
     IPAddressSet *setstore = (IPAddressSet *)v->store;
     cp_ip_address_set(v->v_string, setstore  CP_PASS_CONTEXT);
     break;
   }

#ifndef CLICK_TOOL
   case cpiElement: {
     Element **elementstore = (Element **)v->store;
     *elementstore = v->v.element;
     break;
   }
#endif
   
   default:
    // no argument provided
    break;
    
  }
}


#define CP_VALUES_SIZE 80
static cp_value *cp_values;

static void
assign_keyword_argument(const Vector<String> &args, int argno,
			const char *argname, StringAccum &nonassigned_sa,
			int npositional, int nvalues)
{
  String keyword, rest;
  // find keyword
  if (!cp_keyword(args[argno], &keyword, &rest)) {
    if (nonassigned_sa.length())
      nonassigned_sa << ", ";
    nonassigned_sa << "<" << argname << " " << (argno + 1) << ">";
    return;
  }
  // look for keyword
  for (int i = npositional; i < nvalues; i++)
    if (keyword == cp_values[i].keyword) {
      if (cp_values[i].v.i) {
	if (nonassigned_sa.length())
	  nonassigned_sa << ", ";
	nonassigned_sa << keyword << " (duplicate keyword)";
      } else {
	cp_values[i].v.i = 1;
	cp_values[i].v_string = rest;
      }
      return;
    }
  // no keyboard found
  if (nonassigned_sa.length())
    nonassigned_sa << ", ";
  nonassigned_sa << keyword;
}

static int
cp_va_parsev(const Vector<String> &args,
#ifndef CLICK_TOOL
	     Element *context,
#endif
	     const char *argname, const char *separator,
	     bool keywords_only,
	     ErrorHandler *errh, va_list val)
{
  if (!cp_values)
    return errh->error("out of memory in cp_va_parse");

  int nvalues = 0;
  int nrequired = -1;
  int npositional = -1;
  int nerrors_in = errh->nerrors();
  if (keywords_only)
    nrequired = npositional = 0;

  while (1) {
    
    if (nvalues == CP_VALUES_SIZE - 1)
      // no more space to store information about the arguments; break
      return errh->error("too many arguments to cp_va_parsev!");

    cp_value *v = &cp_values[nvalues];
    v->argtype = 0;
    v->keyword = 0;
    
    if (npositional >= 0) {
      // read keyword if necessary
      v->keyword = va_arg(val, const char *);
      if (v->keyword == 0)
	goto done;
    }

    const char *command_name = va_arg(val, const char *);
    if (command_name == 0)
      goto done;

    // find cp_argtype
    const cp_argtype *argtype = find_argtype(command_name);
    if (!argtype) {
      errh->error("unknown argument type `%s'!", command_name);
      goto done;
    }
    
    // check for special commands
    if (argtype->internal == cpiOptional) {
      if (nrequired < 0)
	nrequired = nvalues;
      continue;
    } else if (argtype->internal == cpiKeywords) {
      if (nrequired < 0)
	nrequired = nvalues;
      if (npositional < 0)
	npositional = nvalues;
      continue;
    } else if (argtype->internal == cpiIgnore) {
      v->argtype = argtype;
      nvalues++;
      continue;
    } else if (argtype->internal == cpiIgnoreRest) {
      if (nrequired < 0)
	nrequired = nvalues;
      v->argtype = argtype;
      nvalues++;
      goto done;
    }

    // store stuff in v
    v->argtype = argtype;
    v->description = va_arg(val, const char *);
    if (argtype->extra == cpArgExtraInt)
      v->extra = va_arg(val, int);
    v->store = va_arg(val, void *);
    if (argtype->extra == cpArgStore2)
      v->store2 = va_arg(val, void *);
    v->v.i = 0;
    nvalues++;
  }

 done:
  
  if (nrequired < 0)
    nrequired = nvalues;
  if (npositional < 0)
    npositional = nvalues;

  // assign keyword arguments
  if (npositional < nvalues && npositional < args.size()) {
    StringAccum nonassigned_sa;
    for (int i = npositional; i < args.size(); i++)
      assign_keyword_argument(args, i, argname, nonassigned_sa, npositional, nvalues);
    if (nonassigned_sa.length() && !keywords_only) {
      StringAccum keywords_sa;
      for (int i = npositional; i < nvalues; i++) {
	if (i > npositional)
	  keywords_sa << ", ";
	keywords_sa << cp_values[i].keyword;
      }
      errh->error("bad keyword(s) %s\n(valid keywords are %s)", nonassigned_sa.cc(), keywords_sa.cc());
      return -1;
    }
  }
  
  // if wrong number of arguments, print signature
  if (args.size() > nvalues && nvalues && cp_values[nvalues-1].argtype->internal == cpiIgnoreRest)
    /* not an error */;
  else if (args.size() > nvalues || args.size() < nrequired) {
    StringAccum signature;
    for (int i = 0; i < npositional; i++) {
      if (i == nrequired)
	signature << (nrequired > 0 ? " [" : "[");
      if (i)
	signature << separator;
      if (const cp_argtype *t = cp_values[i].argtype) {
	if (t->internal == cpiIgnoreRest)
	  signature << "...";
	else
	  signature << t->description;
      } else
	signature << "??";
    }
    if (nrequired < npositional)
      signature << "]";
    if (npositional < nvalues) {
      if (npositional)
	signature << separator;
      signature << "[keywords]";
    }

    const char *whoops = (args.size() > nvalues ? "too many" : "too few");
    if (signature.length())
      errh->error("%s %ss; expected `%s'", whoops, argname, signature.cc());
    else
      errh->error("expected empty %s list", argname);
    return -1;
  }

  // parse arguments
  char argname_buf[128];
  int argname_offset;
  sprintf(argname_buf, "%s %n", argname, &argname_offset);

  for (int i = 0; i < nrequired; i++)
    if (const cp_argtype *t = cp_values[i].argtype) {
      sprintf(argname_buf + argname_offset, "%d", i + 1);
      t->parse(&cp_values[i], args[i], errh, argname_buf  CP_PASS_CONTEXT);
    }
  for (int i = nrequired; i < npositional && i < args.size(); i++)
    if (const cp_argtype *t = cp_values[i].argtype) {
      sprintf(argname_buf + argname_offset, "%d", i + 1);
      t->parse(&cp_values[i], args[i], errh, argname_buf  CP_PASS_CONTEXT);
    }
  for (int i = npositional; i < nvalues; i++) {
    cp_value *v = &cp_values[i];
    if (v->argtype && v->v.i) {
      StringAccum sa;
      sa << "keyword " << v->keyword;
      v->argtype->parse(v, v->v_string, errh, sa.cc()  CP_PASS_CONTEXT);
    } else
      v->argtype = 0;
  }

  // check for failure
  if (errh->nerrors() != nerrors_in)
    return -1;
  
  // if success, actually set the values
  // mark unset optional arguments as dead
  int nset = 0;
  for (int i = args.size(); i < npositional; i++)
    cp_values[i].argtype = 0;
  for (int i = 0; i < nvalues; i++)
    if (const cp_argtype *t = cp_values[i].argtype) {
      t->store(&cp_values[i]  CP_PASS_CONTEXT);
      nset++;
    }

  return nset;
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
  int retval = cp_va_parsev(conf, context, "argument", ", ", false, errh, val);
#else
  int retval = cp_va_parsev(conf, "argument", ", ", false, errh, val);
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
  int retval = cp_va_parsev(conf, context, "argument", ", ", false, errh, val);
#else
  int retval = cp_va_parsev(conf, "argument", ", ", false, errh, val);
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
  int retval = cp_va_parsev(args, context, "word", " ", false, errh, val);
#else
  int retval = cp_va_parsev(args, "word", " ", false, errh, val);
#endif
  va_end(val);
  return retval;
}

int
cp_va_parse_keyword(const String &arg,
#ifndef CLICK_TOOL
		    Element *context,
#endif
		    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> conf;
  conf.push_back(arg);
#ifndef CLICK_TOOL
  int retval = cp_va_parsev(conf, context, "argument", ", ", true, errh, val);
#else
  int retval = cp_va_parsev(conf, "argument", ", ", true, errh, val);
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
cp_unparse_real(unsigned real, int frac_bits)
{
  // adopted from Knuth's TeX function print_scaled, hope it is correct in
  // this context but not sure.
  // Works well with cp_real2 above; as an invariant,
  // unsigned x, y;
  // cp_real2(cp_unparse_real(x, FRAC_BITS), FRAC_BITS, &y) == true && x == y
  
  StringAccum sa;
  assert(frac_bits < 29);

  unsigned int_part = real >> frac_bits;
  sa << int_part;

  unsigned one = 1 << frac_bits;
  real &= one - 1;
  if (!real) return sa.take_string();
  
  sa << ".";
  real = (10 * real) + 5;
  unsigned allowable_inaccuracy = 10;

  unsigned inaccuracy_rounder = 5;
  while (inaccuracy_rounder * 10 < one)
    inaccuracy_rounder *= 10;
  
  do {
    if (allowable_inaccuracy > one)
      real += (one >> 1) - inaccuracy_rounder;
    sa << static_cast<char>('0' + (real >> frac_bits));
    real = 10 * (real & (one - 1));
    allowable_inaccuracy *= 10;
  } while (real > allowable_inaccuracy);

  return sa.take_string();
}

String
cp_unparse_real(int real, int frac_bits)
{
  if (real < 0)
    return "-" + cp_unparse_real(static_cast<unsigned>(-real), frac_bits);
  else
    return cp_unparse_real(static_cast<unsigned>(real), frac_bits);
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


// initialization and cleanup

void
cp_va_static_initialize()
{
  assert(!cp_values);
  
  cp_register_argtype(cpOptional, "<optional arguments marker>", 0, default_parsefunc, default_storefunc, cpiOptional);
  cp_register_argtype(cpKeywords, "<keyword arguments marker>", 0, default_parsefunc, default_storefunc, cpiKeywords);
  cp_register_argtype(cpIgnore, "<ignored argument>", 0, default_parsefunc, default_storefunc, cpiIgnore);
  cp_register_argtype(cpIgnoreRest, "<ignore rest marker>", 0, default_parsefunc, default_storefunc, cpiIgnoreRest);
  
  cp_register_argtype(cpArgument, "??", 0, default_parsefunc, default_storefunc, cpiArgument);
  cp_register_argtype(cpString, "string", 0, default_parsefunc, default_storefunc, cpiString);
  cp_register_argtype(cpWord, "word", 0, default_parsefunc, default_storefunc, cpiWord);
  cp_register_argtype(cpBool, "bool", 0, default_parsefunc, default_storefunc, cpiBool);
  cp_register_argtype(cpByte, "byte", 0, default_parsefunc, default_storefunc, cpiByte);
  cp_register_argtype(cpShort, "short", 0, default_parsefunc, default_storefunc, cpiShort);
  cp_register_argtype(cpUnsignedShort, "unsigned short", 0, default_parsefunc, default_storefunc, cpiUnsignedShort);
  cp_register_argtype(cpInteger, "int", 0, default_parsefunc, default_storefunc, cpiInteger);
  cp_register_argtype(cpUnsigned, "unsigned", 0, default_parsefunc, default_storefunc, cpiUnsigned);
  cp_register_argtype(cpReal, "real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiReal);
  cp_register_argtype(cpNonnegReal, "unsigned real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiNonnegReal);
  cp_register_argtype(cpNonnegFixed, "unsigned real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiNonnegFixed);
  cp_register_argtype(cpMilliseconds, "time in seconds", 0, default_parsefunc, default_storefunc, cpiMilliseconds);
  cp_register_argtype(cpIPAddress, "IP address", 0, default_parsefunc, default_storefunc, cpiIPAddress);
  cp_register_argtype(cpIPPrefix, "IP address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIPPrefix);
  cp_register_argtype(cpIPAddressOrPrefix, "IP address or prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIPAddressOrPrefix);
  cp_register_argtype(cpIPAddressSet, "set of IP addresses", 0, default_parsefunc, default_storefunc, cpiIPAddressSet);
  cp_register_argtype(cpEthernetAddress, "Ethernet address", 0, default_parsefunc, default_storefunc, cpiEthernetAddress);
  cp_register_argtype(cpElement, "element name", 0, default_parsefunc, default_storefunc, cpiElement);
  cp_register_argtype(cpIP6Address, "IPv6 address", 0, default_parsefunc, default_storefunc, cpiIP6Address);
  cp_register_argtype(cpIP6Prefix, "IPv6 address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6Prefix);
  cp_register_argtype(cpIP6AddressOrPrefix, "IPv6 address or prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6AddressOrPrefix);

  cp_values = new cp_value[CP_VALUES_SIZE];
}

void
cp_va_static_cleanup()
{
  for (int i = 0; i < NARGTYPE_HASH; i++) {
    cp_argtype *t = argtype_hash[i];
    while (t) {
      cp_argtype *n = t->next;
      delete t;
      t = n;
    }
  }
  memset(argtype_hash, 0, sizeof(argtype_hash));

  delete[] cp_values;
}
