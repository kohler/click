/*
 * confparse.{cc,hh} -- configuration string parsing
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000-2001 Mazu Networks, Inc.
 * Copyright (c) 2001 ACIRI
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
    if (s[i] == '\"' || s[i] == '\'' || s[i] == '\\' || s[i] == ','
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

static int
skip_comment(const char *s, int pos, int len)
{
  assert(pos < len - 1 && s[pos] == '/' && (s[pos+1] == '/' || s[pos+1] == '*'));

  if (s[pos+1] == '/') {
    for (pos += 2; pos < len - 1 && s[pos] != '\n' && s[pos] != '\r'; pos++)
      /* nada */;
    if (pos < len - 1 && s[pos] == '\r' && s[pos+1] == '\n')
      pos++;
    return pos + 1;
  } else { /* s[pos+1] == '*' */
    for (pos += 2; pos < len - 2 && (s[pos] != '*' || s[pos+1] != '/'); pos++)
      /* nada */;
    return pos + 2;
  }
}

static int
skip_backslash_angle(const char *s, int pos, int len)
{
  assert(pos < len - 1 && s[pos] == '\\' && s[pos+1] == '<');
  
  for (pos += 2; pos < len; )
    if (s[pos] == '>')
      return pos + 1;
    else if (s[pos] == '/' && pos < len - 1 && (s[pos+1] == '/' || s[pos+1] == '*'))
      pos = skip_comment(s, pos, len);
    else
      pos++;
  
  return len;
}

static int
skip_double_quote(const char *s, int pos, int len)
{
  assert(pos < len && s[pos] == '\"');

  for (pos++; pos < len; )
    if (pos < len - 1 && s[pos] == '\\') {
      if (s[pos+1] == '<')
	pos = skip_backslash_angle(s, pos, len);
      else
	pos += 2;
    } else if (s[pos] == '\"')
      return pos + 1;
    else
      pos++;

  return len;
}

static int
skip_single_quote(const char *s, int pos, int len)
{
  assert(pos < len && s[pos] == '\'');

  for (pos++; pos < len; pos++)
    if (s[pos] == '\'')
      return pos + 1;

  return len;
}

static String
partial_uncomment(const String &str, int i, int *comma_pos)
{
  const char *s = str.data();
  int len = str.length();

  // skip initial spaces
  for (; i < len; i++) {
    if (s[i] == '/' && i < len - 1 && (s[i+1] == '/' || s[i+1] == '*'))
      i = skip_comment(s, i, len) - 1;
    else if (!isspace(s[i]))
      break;
  }

  // accumulate text, skipping comments
  StringAccum sa;
  int left = i;
  int right = i;
  bool closed = false;

  while (i < len) {
    if (isspace(s[i]))
      i++;
    else if (s[i] == '/' && i < len - 1 && (s[i+1] == '/' || s[i+1] == '*')) {
      i = skip_comment(s, i, len);
      closed = true;
    } else if (s[i] == ',' && comma_pos)
      break;
    else {
      if (closed) {
	sa << str.substring(left, right - left) << ' ';
	left = i;
	closed = false;
      }
      if (s[i] == '\'')
	i = skip_single_quote(s, i, len);
      else if (s[i] == '\"')
	i = skip_double_quote(s, i, len);
      else if (s[i] == '\\' && i < len - 1 && s[i+1] == '<')
	i = skip_backslash_angle(s, i, len);
      else
	i++;
      right = i;
    }
  }

  if (comma_pos)
    *comma_pos = i;
  if (!sa)
    return str.substring(left, right - left);
  else {
    sa << str.substring(left, right - left);
    return sa.take_string();
  }
}

String
cp_uncomment(const String &str)
{
  return partial_uncomment(str, 0, 0);
}

static int
process_backslash(const char *s, int i, int len, StringAccum &sa)
{
  assert(i < len - 1 && s[i] == '\\');
  
  switch (s[i+1]) {
    
   case '\r':
    return (i < len - 2 && s[i+2] == '\n' ? i + 3 : i + 2);

   case '\n':
    return i + 2;
    
   case 'a': sa << '\a'; return i + 2;
   case 'b': sa << '\b'; return i + 2;
   case 'f': sa << '\f'; return i + 2;
   case 'n': sa << '\n'; return i + 2;
   case 'r': sa << '\r'; return i + 2;
   case 't': sa << '\t'; return i + 2;
   case 'v': sa << '\v'; return i + 2;
    
   case '0': case '1': case '2': case '3':
   case '4': case '5': case '6': case '7': {
     int c = 0, d = 0;
     for (i++; i < len && s[i] >= '0' && s[i] <= '7' && d < 3;
	  i++, d++)
       c = c*8 + s[i] - '0';
     sa << (char)c;
     return i;
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
     return i;
   }
   
   case '<': {
     int c = 0, d = 0;
     for (i += 2; i < len; i++) {
       if (s[i] == '>')
	 return i + 1;
       else if (s[i] >= '0' && s[i] <= '9')
	 c = c*16 + s[i] - '0';
       else if (s[i] >= 'A' && s[i] <= 'F')
	 c = c*16 + s[i] - 'A' + 10;
       else if (s[i] >= 'a' && s[i] <= 'f')
	 c = c*16 + s[i] - 'a' + 10;
       else if (s[i] == '/' && i < len - 1 && (s[i+1] == '/' || s[i+1] == '*')) {
	 i = skip_comment(s, i, len) - 1;
	 continue;
       } else
	 continue;	// space (ignore it) or random (error)
       if (++d == 2) {
	 sa << (char)c;
	 c = d = 0;
       }
     }
     // ran out of space in string
     return len;
   }

   case '\\': case '\'': case '\"': case '$':
   default:
    sa << s[i+1];
    return i + 2;
    
  }
}

String
cp_unquote(const String &in_str)
{
  String str = partial_uncomment(in_str, 0, 0);
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
	start = process_backslash(s, i, len, sa);
	i = start - 1;
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

void
cp_argvec(const String &conf, Vector<String> &args)
{
  int len = conf.length();
  int i = 0;
  bool first_arg = true;
  
  // common case: no configuration
  if (len == 0)
    return;

  // <= to handle case where `conf' ends in `,' (= an extra empty string
  // argument)
  while (i <= len) {
    String arg = partial_uncomment(conf, i, &i);

    // add the argument if it is nonempty, or this isn't the first argument
    if (arg || i < len || !first_arg)
      args.push_back(arg);
    
    // bump `i' past the comma
    i++;
    first_arg = false;
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
      i = skip_comment(s, i, len) - 1;
      start = -1;
      break;
      
     case '\"':
      if (start < 0)
	start = i;
      i = skip_double_quote(s, i, len) - 1;
      break;
      
     case '\'':
      if (start < 0)
	start = i;
      i = skip_single_quote(s, i, len) - 1;
      break;

     case '\\':			// check for \<...> strings
      if (start < 0)
	start = i;
      if (i < len - 1 && s[i+1] == '<')
	i = skip_backslash_angle(s, i, len) - 1;
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


// PARSING STRINGS

bool
cp_string(const String &str, String *return_value, String *rest = 0)
{
  const char *s = str.data();
  int len = str.length();
  int i = 0;

  // accumulate a word
  for (; i < len; i++)
    switch (s[i]) {
      
     case ' ':
     case '\f':
     case '\n':
     case '\r':
     case '\t':
     case '\v':
      goto done;

     case '\"':
      i = skip_double_quote(s, i, len) - 1;
      break;

     case '\'':
      i = skip_single_quote(s, i, len) - 1;
      break;

     case '\\':
      if (i < len - 1 && s[i+1] == '<')
	i = skip_backslash_angle(s, i, len) - 1;
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
cp_keyword(const String &str, String *return_value, String *rest)
{
  const char *s = str.data();
  int len = str.length();
  int i = 0;

  // accumulate a word
  for (; i < len; i++)
    switch (s[i]) {
      
     case ' ':
     case '\f':
     case '\n':
     case '\r':
     case '\t':
     case '\v':
      goto done;

      // characters allowed unquoted in keywords
     case '_':
     case '.':
     case ':':
      break;
      
     default:
      if (!isalnum(s[i]))
	return false;
      break;
      
    }
  
 done:
  if (i == 0 || (!rest && i < len))
    return false;
  else {
    *return_value = str.substring(0, i);
    if (rest) {
      for (; i < len; i++)
	if (!isspace(s[i]))
	  break;
      *rest = str.substring(i);
    }
    return true;
  }
}


// PARSING INTEGERS

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

  if ((base == 0 || base == 16) && i < len - 1
      && s[i] == '0' && (s[i+1] == 'x' || s[i+1] == 'X')) {
    i += 2;
    base = 16;
  } else if (base == 0 && s[i] == '0')
    base = 8;
  else if (base == 0)
    base = 10;
  else if (base < 2 || base > 36) {
    cp_errno = CPE_INVALID;
    return false;
  }

  if (i == len)			// no digits
    return false;

  unsigned overflow_val = 0xFFFFFFFFU / base;
  int overflow_digit = 0xFFFFFFFFU - (overflow_val * base);
  
  unsigned val = 0;
  cp_errno = CPE_OK;
  while (i < len) {
    // find digit
    int digit;
    if (s[i] >= '0' && s[i] <= '9')
      digit = s[i] - '0';
    else if (s[i] >= 'A' && s[i] <= 'Z')
      digit = s[i] - 'A' + 10;
    else if (s[i] >= 'a' && s[i] <= 'z')
      digit = s[i] - 'a' + 10;
    else
      digit = 36;
    if (digit >= base)
      return false;
    // check for overflow
    if (val > overflow_val || (val == overflow_val && digit > overflow_digit))
      cp_errno = CPE_OVERFLOW;
    // assign new value
    val = val * base + digit;
    i++;
  }

  *return_value = (cp_errno ? 0xFFFFFFFFU : val);
  return true;
}

bool
cp_unsigned(const String &str, unsigned *return_value)
{
  return cp_unsigned(str, 0, return_value);
}

bool
cp_integer(const String &in_str, int base, int *return_value)
{
  String str = in_str;
  bool negative = false;
  if (str.length() > 1 && str[0] == '-' && str[1] != '+') {
    negative = true;
    str = in_str.substring(1);
  }

  unsigned value;
  if (!cp_unsigned(str, base, &value))
    return false;

  unsigned max = (negative ? 0x80000000U : 0x7FFFFFFFU);
  if (value > max) {
    cp_errno = CPE_OVERFLOW;
    value = max;
  }

  *return_value = (negative ? -value : value);
  return true;
}

bool
cp_integer(const String &str, int *return_value)
{
  return cp_integer(str, 0, return_value);
}

static unsigned
exp10(int exponent)
{
  assert(exponent >= 0);
  unsigned val = 1;
  while (exponent-- > 0)
    val *= 10;
  return val;
}

bool
cp_unsigned_real10(const String &str, int frac_digits,
		   unsigned *return_int_part, unsigned *return_frac_part)
{
  const char *s = str.data();
  const char *last = s + str.length();
  
  cp_errno = CPE_FORMAT;
  if (s == last)
    return false;
  if (frac_digits < 0 || frac_digits > 9) {
    cp_errno = CPE_INVALID;
    return false;
  }
  
  if (*s == '+')
    s++;
  
  // find integer part of string
  const char *int_s = s;
  while (s < last && isdigit(*s))
    s++;
  int int_chars = s - int_s;
  
  // find fractional part of string
  const char *frac_s;
  int frac_chars;
  if (s < last && *s == '.') {
    frac_s = ++s;
    while (s < last && isdigit(*s))
      s++;
    frac_chars = s - frac_s;
  } else
    frac_s = s, frac_chars = 0;
  
  // no integer or fraction? illegal real
  if (int_chars == 0 && frac_chars == 0)
    return false;
  
  // find exponent, if any
  int exponent = 0;
  if (s < last && (*s == 'E' || *s == 'e')) {
    if (++s == last)
      return false;
    
    bool negexp = (*s == '-');
    if (*s == '-' || *s == '+')
      s++;
    if (s >= last || !isdigit(*s))
      return false;
    
    // XXX overflow?
    for (; s < last && isdigit(*s); s++)
      exponent = 10*exponent + *s - '0';
    
    if (negexp)
      exponent = -exponent;
  }
  
  if (s != last)
    return false;

  // OK! now create the result
  // determine integer part; careful about overflow
  unsigned int_part = 0;
  cp_errno = CPE_OK;
  
  for (int i = 0; i < int_chars + exponent; i++) {
    int digit;
    if (i < int_chars)
      digit = int_s[i] - '0';
    else if (i - int_chars < frac_chars)
      digit = frac_s[i - int_chars] - '0';
    else
      digit = 0;
    if (int_part > 0x19999999U || (int_part == 0x19999999U && digit > 5))
      cp_errno = CPE_OVERFLOW;
    int_part = int_part * 10 + digit;
  }
  
  // determine fraction part
  unsigned frac_part = 0;
  int digit = 0;
  
  for (int i = 0; i <= frac_digits; i++) {
    if (i + exponent + int_chars < 0)
      digit = 0;
    else if (i + exponent < 0)
      digit = int_s[i + exponent + int_chars] - '0';
    else if (i + exponent < frac_chars)
      digit = frac_s[i + exponent] - '0';
    else
      digit = 0;
    // skip out on the last digit
    if (i == frac_digits)
      break;
    // no overflow possible b/c frac_digits was limited
    frac_part = frac_part * 10 + digit;
  }

  // round fraction part if required
  if (digit >= 5) {
    if (frac_part == exp10(frac_digits) - 1) {
      frac_part = 0;
      if (int_part == 0xFFFFFFFFU)
	cp_errno = CPE_OVERFLOW;
      int_part++;
    } else
      frac_part++;
  }
  
  // done!
  if (cp_errno) {		// overflow
    int_part = 0xFFFFFFFFU;
    frac_part = exp10(frac_digits) - 1;
  }

  //click_chatter("%d: %u %u", frac_digits, int_part, frac_part);
  *return_int_part = int_part;
  *return_frac_part = frac_part;
  return true;
}

bool
cp_unsigned_real10(const String &str, int frac_digits, unsigned *return_value)
{
  unsigned int_part, frac_part;
  if (!cp_unsigned_real10(str, frac_digits, &int_part, &frac_part))
    return false;

  // check for overflow
  unsigned one = exp10(frac_digits);
  unsigned int_max = 0xFFFFFFFFU / one;
  unsigned frac_max = 0xFFFFFFFFU - int_max * one;
  if (int_part > int_max || (int_part == int_max && frac_part > frac_max)) {
    cp_errno = CPE_OVERFLOW;
    *return_value = 0xFFFFFFFFU;
  } else
    *return_value = int_part * one + frac_part;
  
  return true;
}

bool
cp_unsigned_real2(const String &str, int frac_bits, unsigned *return_value)
{
  if (frac_bits < 0 || frac_bits >= 29) {
    cp_errno = CPE_INVALID;
    return false;
  }
  
  unsigned int_part, frac_part;
  if (!cp_unsigned_real10(str, 9, &int_part, &frac_part)) {
    cp_errno = CPE_FORMAT;
    return false;
  }

  // method from Knuth's TeX, round_decimals. Works well with
  // cp_unparse_real2 below
  unsigned fraction = 0;
  unsigned two = 2U << frac_bits;
  for (int i = 0; i < 9; i++) {
    unsigned digit = frac_part % 10;
    fraction = (fraction + digit*two) / 10;
    frac_part /= 10;
  }
  fraction = (fraction + 1) / 2;

  // This can happen! (for example, 16 bits of fraction, .999999) Why?
  if (fraction == (1U << frac_bits) && int_part < 0xFFFFFFFFU)
    int_part++, fraction = 0;

  // check for overflow
  if (cp_errno || int_part > (1U << (32 - frac_bits)) - 1) {
    cp_errno = CPE_OVERFLOW;
    *return_value = 0xFFFFFFFFU;
  } else
    *return_value = (int_part << frac_bits) + fraction;
  
  return true;
}

static bool
cp_real_base(const String &in_str, int frac_digits, int *return_value,
	     bool (*func)(const String &, int, unsigned *))
{
  String str = in_str;
  bool negative = false;
  if (str.length() > 1 && str[0] == '-' && str[1] != '+') {
    negative = true;
    str = str.substring(1);
  }

  unsigned value;
  if (!func(str, frac_digits, &value))
    return false;

  // check for overflow
  unsigned umax = (negative ? 0x80000000 : 0x7FFFFFFF);
  if (value > umax) {
    cp_errno = CPE_OVERFLOW;
    value = umax;
  }

  *return_value = (negative ? -value : value);
  return true;
}

bool
cp_real10(const String &str, int frac_digits, int *return_value)
{
  return cp_real_base(str, frac_digits, return_value, cp_unsigned_real10);
}

bool
cp_real2(const String &str, int frac_bits, int *return_value)
{
  return cp_real_base(str, frac_bits, return_value, cp_unsigned_real2);
}

bool
cp_milliseconds(const String &str, int *return_value)
{
  int v;
  if (!cp_real10(str, 3, &v))
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
cp_timeval(const String &str, struct timeval *return_value)
{
  int dot = str.find_left('.');
  if (dot < 0)
    dot = str.length();
  unsigned sec = 0;
  if (dot > 0) {
    if (!cp_unsigned(str.substring(0, dot), &sec))
      return false;
  }
  int usec = 0;
  if (dot < str.length() - 1) {
    if (!cp_real10(str.substring(dot), 6, &usec))
      return false;
  }
  return_value->tv_sec = sec;
  return_value->tv_usec = usec;
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

bool
cp_ip6_address(const String &str, IP6Address *address
	       CP_CONTEXT_ARG)
{
  return cp_ip6_address(str, address->data()  CP_PASS_CONTEXT);
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
    relevant_bits = IP6Address(mask).mask_to_prefix_bits();
    if (relevant_bits < 0)
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
cp_ip6_prefix(const String &str, IP6Address *address, int *prefix,
	      bool allow_bare_address  CP_CONTEXT_ARG)
{
  return cp_ip6_prefix(str, address->data(), prefix, allow_bare_address  CP_PASS_CONTEXT);
}

bool
cp_ip6_prefix(const String &str, IP6Address *address, IP6Address *prefix,
	      bool allow_bare_address  CP_CONTEXT_ARG)
{
  return cp_ip6_prefix(str, address->data(), prefix->data(), allow_bare_address  CP_PASS_CONTEXT);
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
cp_element(const String &text_in, Element *context, ErrorHandler *errh)
{
  String name;
  if (!cp_string(text_in, &name)) {
    if (errh)
      errh->error("bad name format");
    return 0;
  } else
    return context->router()->find(name, context, errh);
}

Element *
cp_element(const String &text_in, Router *router, ErrorHandler *errh)
{
  String name;
  if (!cp_string(text_in, &name)) {
    if (errh)
      errh->error("bad name format");
    return 0;
  } else
    return router->find(name, errh);
}

bool
cp_handler(const String &str, Element *context, Element **result_element,
	   String *result_hname, ErrorHandler *errh)
{
  if (!errh)
    errh = ErrorHandler::silent_handler();
  
  String text;
  if (!cp_string(str, &text)) {
    errh->error("bad name format");
    return false;
  }

  int leftmost_dot = text.find_left('.');
  if (leftmost_dot < 0) {
    errh->error("handler name syntax error: should have contained a dot `.'");
    return false;
  } else if (leftmost_dot == text.length() - 1) {
    errh->error("empty handler name");
    return false;
  }

  Element *e = context->router()->find(text.substring(0, leftmost_dot), context, errh);
  if (!e)
    return false;

  *result_element = e;
  *result_hname = text.substring(leftmost_dot + 1);
  return true;
}

bool
cp_handler(const String &str, Element *context, bool need_read,
	   bool need_write, Element **result_element, int *result_hid,
	   ErrorHandler *errh)
{
  if (!errh)
    errh = ErrorHandler::silent_handler();
  
  Element *e;
  String hname;
  if (!cp_handler(str, context, &e, &hname, errh))
    return false;

  int hid = context->router()->find_handler(e, hname);
  if (hid < 0) {
    errh->error("element `%s' has no `%s' handler", e->id().cc(), hname.cc());
    if (context->router()->nhandlers() == 0)
      errh->error("because handlers have not been added yet!");
    return false;
  }

  const Router::Handler &h = context->router()->handler(hid);
  if (need_read && !h.read) {
    errh->error("`%s.%s' is not a read handler", e->id().cc(), hname.cc());
    return false;
  } else if (need_write && !h.write) {
    errh->error("`%s.%s' is not a write handler", e->id().cc(), hname.cc());
    return false;
  } else {
    *result_element = e;
    *result_hid = hid;
    return true;
  }
}

bool
cp_handler(const String &str, Element *context, Element **result_element, int *result_hid, ErrorHandler *errh)
{
  return cp_handler(str, context, false, false, result_element, result_hid, errh);
}
#endif

#ifdef HAVE_IPSEC
bool
cp_des_cblock(const String &str, unsigned char *return_value)
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

  if (len != i)
    return false;
  else {
    memcpy(return_value, value, 8);
    return true;
  }
}
#endif

//
// CP_VA_PARSE AND FRIENDS
//

// parse commands; those which must be recognized inside a keyword section
// must begin with "\377"

CpVaParseCmd
  cpOptional		= "OPTIONAL",
  cpKeywords		= "\377KEYWORDS",
  cpMandatoryKeywords	= "\377MANDATORY_KEYWORDS",
  cpIgnore		= "IGNORE",
  cpIgnoreRest		= "\377IGNORE_REST",
  cpArgument		= "arg",
  cpArguments		= "args",
  cpString		= "string",
  cpWord		= "word",
  cpKeyword		= "keyword",
  cpBool		= "bool",
  cpByte		= "byte",
  cpShort		= "short",
  cpUnsignedShort	= "u_short",
  cpInteger		= "int",
  cpUnsigned		= "u_int",
  cpReal2		= "real2",
  cpUnsignedReal2	= "u_real2",
  cpNonnegReal2		= "u_real2", // synonym
  cpReal10		= "real10",
  cpUnsignedReal10	= "u_real10",
  cpNonnegReal10	= "u_real10", // synonym
  cpMilliseconds	= "msec",
  cpTimeval		= "timeval",
  cpIPAddress		= "ip_addr",
  cpIPPrefix		= "ip_prefix",
  cpIPAddressOrPrefix	= "ip_addr_or_prefix",
  cpIPAddressSet	= "ip_addr_set",
  cpEthernetAddress	= "ether_addr",
  cpEtherAddress	= "ether_addr", // synonym
  cpElement		= "element",
  cpHandlerName		= "handler_name",
  cpHandler		= "handler",
  cpReadHandler		= "read_handler",
  cpWriteHandler	= "write_handler",
  cpIP6Address		= "ip6_addr",
  cpIP6Prefix		= "ip6_prefix",
  cpIP6AddressOrPrefix	= "ip6_addr_or_prefix",
  cpDesCblock		= "des_cblock";

enum {
  cpiEnd = 0,
  cpiOptional,
  cpiKeywords,
  cpiMandatoryKeywords,
  cpiIgnore,
  cpiIgnoreRest,
  cpiArgument,
  cpiArguments,
  cpiString,
  cpiWord,
  cpiKeyword,
  cpiBool,
  cpiByte,
  cpiShort,
  cpiUnsignedShort,
  cpiInteger,
  cpiUnsigned,
  cpiReal2,
  cpiUnsignedReal2,
  cpiReal10,
  cpiUnsignedReal10,
  cpiMilliseconds,
  cpiTimeval,
  cpiIPAddress,
  cpiIPPrefix,
  cpiIPAddressOrPrefix,
  cpiIPAddressSet,
  cpiEthernetAddress,
  cpiElement,
  cpiHandlerName,
  cpiHandler,
  cpiReadHandler,
  cpiWriteHandler,
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
   case cpiArguments:
    // nothing to do
    break;
    
   case cpiString:
    if (!cp_string(arg, &v->v_string))
      errh->error("%s takes string (%s)", argname, desc);
    break;
    
   case cpiWord:
    if (!cp_word(arg, &v->v_string))
      errh->error("%s takes word (%s)", argname, desc);
    break;
    
   case cpiKeyword:
    if (!cp_keyword(arg, &v->v_string))
      errh->error("%s takes keyword (%s)", argname, desc);
    break;
    
   case cpiBool:
    if (!cp_bool(arg, &v->v.b))
      errh->error("%s takes bool (%s)", argname, desc);
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
      errh->error("%s takes %s (%s)", argname, argtype->description, desc);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("overflow on %s (%s); max %d", argname, desc, v->v.i);
    else if (v->v.i < underflower)
      errh->error("%s (%s) must be >= %d", argname, desc, underflower);
    else if (v->v.i > (int)overflower)
      errh->error("%s (%s) must be <= %u", argname, desc, overflower);
    break;

   handle_unsigned:
    if (!cp_unsigned(arg, &v->v.u))
      errh->error("%s takes %s (%s)", argname, argtype->description, desc);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("overflow on %s (%s); max %u", argname, desc, v->v.u);
    else if (v->v.u > overflower)
      errh->error("%s (%s) must be <= %u", argname, desc, overflower);
    break;

   case cpiReal10:
    if (!cp_real10(arg, v->extra, &v->v.i))
      errh->error("%s takes real (%s)", argname, desc);
    else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_real10(v->v.i, v->extra);
      errh->error("overflow on %s (%s); max %s", argname, desc, m.cc());
    }
    break;

   case cpiUnsignedReal10:
    if (!cp_unsigned_real10(arg, v->extra, &v->v.u))
      errh->error("%s takes unsigned real (%s)", argname, desc);
    else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_real10(v->v.u, v->extra);
      errh->error("overflow on %s (%s); max %s", argname, desc, m.cc());
    }
    break;

   case cpiMilliseconds:
    if (!cp_milliseconds(arg, &v->v.i)) {
      if (cp_errno == CPE_NEGATIVE)
	errh->error("%s (%s) must be >= 0", argname, desc);
      else
	errh->error("%s takes time in seconds (%s)", argname, desc);
    } else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_milliseconds(v->v.i);
      errh->error("overflow on %s (%s); max %s", argname, desc, m.cc());
    }
    break;

   case cpiTimeval: {
     struct timeval tv;
     if (!cp_timeval(arg, &tv)) {
       if (cp_errno == CPE_NEGATIVE)
	 errh->error("%s (%s) must be >= 0", argname, desc);
       else
	 errh->error("%s takes seconds since the epoch (%s)", argname, desc);
     } else if (cp_errno == CPE_OVERFLOW)
       errh->error("overflow on %s (%s)", argname, desc);
     else {
       v->v.i = tv.tv_sec;
       v->v2.i = tv.tv_usec;
     }
     break;
   }

   case cpiReal2:
    if (!cp_real2(arg, v->extra, &v->v.i)) {
      // CPE_INVALID would indicate a bad 'v->extra'
      errh->error("%s takes real (%s)", argname, desc);
    } else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_real2(v->v.i, v->extra);
      errh->error("overflow on %s (%s); max %s", argname, desc, m.cc());
    }
    break;

   case cpiUnsignedReal2:
    if (!cp_unsigned_real2(arg, v->extra, &v->v.u)) {
      // CPE_INVALID would indicate a bad 'v->extra'
      errh->error("%s takes unsigned real (%s)", argname, desc);
    } else if (cp_errno == CPE_OVERFLOW) {
      String m  = cp_unparse_real2(v->v.u, v->extra);
      errh->error("overflow on %s (%s); max %s", argname, desc, m.cc());
    }
    break;

   case cpiIPAddress:
    if (!cp_ip_address(arg, v->v.address CP_PASS_CONTEXT))
      errh->error("%s takes IP address (%s)", argname, desc);
    break;

   case cpiIPPrefix:
   case cpiIPAddressOrPrefix: {
     bool mask_optional = (argtype->internal == cpiIPAddressOrPrefix);
     if (!cp_ip_prefix(arg, v->v.address, v->v2.address, mask_optional CP_PASS_CONTEXT))
       errh->error("%s takes IP address prefix (%s)", argname, desc);
     break;
   }

   case cpiIPAddressSet: {
     IPAddressSet crap;
     if (!cp_ip_address_set(arg, &crap CP_PASS_CONTEXT))
       errh->error("%s takes set of IP addresses (%s)", argname, desc);
     break;
   }     

   case cpiIP6Address:
    if (!cp_ip6_address(arg, (unsigned char *)v->v.address))
      errh->error("%s takes IPv6 address (%s)", argname, desc);
    break;

   case cpiIP6Prefix:
   case cpiIP6AddressOrPrefix: {
     bool mask_optional = (argtype->internal == cpiIP6AddressOrPrefix);
     if (!cp_ip6_prefix(arg, v->v.address, v->v2.address, mask_optional CP_PASS_CONTEXT))
       errh->error("%s takes IPv6 address prefix (%s)", argname, desc);
     break;
   }

   case cpiEthernetAddress:
    if (!cp_ethernet_address(arg, v->v.address CP_PASS_CONTEXT))
      errh->error("%s takes Ethernet address (%s)", argname, desc);
    break;

#ifdef HAVE_IPSEC
   case cpiDesCblock:
    if (!cp_des_cblock(arg, v->v.address))
      errh->error("%s takes DES encryption block (%s)", argname, desc);
    break;
#endif

#ifndef CLICK_TOOL
   case cpiElement: {
     ContextErrorHandler cerrh(errh, String(argname) + " (" + desc + "):");
     v->v.element = cp_element(arg, context, &cerrh);
     break;
   }
   
   case cpiHandlerName: {
     ContextErrorHandler cerrh(errh, String(argname) + " (" + desc + "):");
     cp_handler(arg, context, &v->v.element, &v->v2_string, &cerrh);
     break;
   }

   case cpiHandler:
   case cpiReadHandler:
   case cpiWriteHandler: {
     ContextErrorHandler cerrh(errh, String(argname) + " (" + desc + "):");
     cp_handler(arg, context, argtype->internal == cpiReadHandler,
		argtype->internal == cpiWriteHandler,
		&v->v.element, &v->v2.i, &cerrh);
     break;
   }
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
   case cpiReal2:
   case cpiReal10:
   case cpiMilliseconds: {
     int *istore = (int *)v->store;
     *istore = v->v.i;
     break;
   }
  
   case cpiUnsigned:
   case cpiUnsignedReal2:
   case cpiUnsignedReal10: {
     unsigned *ustore = (unsigned *)v->store; 
     *ustore = v->v.u; 
     break; 
   }

   case cpiTimeval: {
     struct timeval *tvstore = (struct timeval *)v->store;
     tvstore->tv_sec = v->v.i;
     tvstore->tv_usec = v->v2.i;
     break;
   }

   case cpiArgument:
   case cpiString:
   case cpiWord:
   case cpiKeyword: {
     String *sstore = (String *)v->store;
     *sstore = v->v_string;
     break;
   }

   case cpiArguments: {
     Vector<String> *vstore = (Vector<String> *)v->store;
     int pos = 0, pos2;
     for (int len_pos = 0; len_pos < v->v2_string.length(); ) {
       int next = v->v2_string.find_left(' ', len_pos);
       cp_integer(v->v2_string.substring(len_pos, next - len_pos), &pos2);
       vstore->push_back(v->v_string.substring(pos, pos2 - pos));
       pos = pos2;
       len_pos = next + 1;
     }
     vstore->push_back(v->v_string.substring(pos));
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

#ifdef HAVE_IPSEC
   case cpiDesCblock:
    address_bytes = 8;
    goto address;
#endif
   
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
     memcpy(maskstore, v->v2.address, 4);
     break;
   }

   case cpiIP6Prefix:
   case cpiIP6AddressOrPrefix: {
     unsigned char *addrstore = (unsigned char *)v->store;
     memcpy(addrstore, v->v.address, 16);
     unsigned char *maskstore = (unsigned char *)v->store2;
     memcpy(maskstore, v->v2.address, 16);
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

   case cpiHandlerName: {
     Element **elementstore = (Element **)v->store;
     String *hnamestore = (String *)v->store2;
     *elementstore = v->v.element;
     *hnamestore = v->v2_string;
     break;
   }

   case cpiHandler:
   case cpiReadHandler:
   case cpiWriteHandler: {
     Element **elementstore = (Element **)v->store;
     int *hidstore = (int *)v->store2;
     *elementstore = v->v.element;
     *hidstore = v->v2.i;
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

enum {
  kwSuccess = 0,
  kwDupKeyword = -1,
  kwNoKeyword = -2,
  kwUnkKeyword = -3,
  kwMissingKeyword = -4
};

static inline bool
special_argtype_for_keyword(const cp_argtype *t)
{
  return t->internal == cpiArguments;
}

static int
handle_special_argtype_for_keyword(cp_value *val, const String &rest)
{
  if (val->argtype->internal == cpiArguments) {
    if (val->v.i > 0) {
      val->v2_string += String(val->v_string.length()) + " ";
      val->v_string += rest;
    } else {
      val->v.i = 1;
      val->v_string = rest;
    }
    return kwSuccess;
  } else {
    assert(0);
    return kwUnkKeyword;
  }
}

static int
assign_keyword_argument(const String &arg, int npositional, int nvalues)
{
  String keyword, rest;
  // extract keyword
  if (!cp_keyword(arg, &keyword, &rest))
    return kwNoKeyword;
  // doesn't count as a keyword if there was no accompanying data
  // (XXX is this a great idea?)
  if (!rest)
    return kwNoKeyword;
  // look for keyword value
  for (int i = npositional; i < nvalues; i++)
    if (keyword == cp_values[i].keyword) {
      cp_value *val = &cp_values[i];
      // handle special types differently
      if (special_argtype_for_keyword(val->argtype))
	return handle_special_argtype_for_keyword(val, rest);
      else {
	// do not report error if keyword used already
	// if (cp_values[i].v.i > 0)
	//   return kwDupKeyword;
	val->v.i = 1;
	val->v_string = rest;
	return kwSuccess;
      }
    }
  // no keyword found
  return kwUnkKeyword;
}

static void
add_keyword_error(StringAccum &sa, int err, const String &arg,
		  const char *argname, int argno)
{
  if (err >= 0)
    return;
  if (sa.length())
    sa << ", ";
  if (err == kwNoKeyword)
    sa << '<' << argname << ' ' << (argno + 1) << '>';
  else {
    String keyword, rest;
    (void) cp_keyword(arg, &keyword, &rest);
    sa << keyword;
    if (err == kwDupKeyword)
      sa << " (duplicate keyword)";
  }
}

static int
finish_keyword_error(const char *format, const char *bad_keywords,
		     int npositional, int nvalues, ErrorHandler *errh)
{
  StringAccum keywords_sa;
  for (int i = npositional; i < nvalues; i++) {
    if (i > npositional)
      keywords_sa << ", ";
    keywords_sa << cp_values[i].keyword;
  }
  errh->error(format, bad_keywords, keywords_sa.cc());
  return -1;
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
  bool mandatory_keywords = false;
  bool ignore_rest = false;
  int nerrors_in = errh->nerrors();
  
  if (keywords_only) {
    nrequired = npositional = 0;
    ignore_rest = true;
  }

  while (1) {
    
    if (nvalues == CP_VALUES_SIZE - 1)
      // no more space to store information about the arguments; break
      return errh->error("too many arguments to cp_va_parsev!");

    cp_value *v = &cp_values[nvalues];
    v->argtype = 0;
    v->keyword = 0;
    const char *command_name = 0;
    
    if (npositional >= 0) {
      // read keyword if necessary; be careful of special "cp" values,
      // which begin with "\377"
      v->keyword = va_arg(val, const char *);
      if (v->keyword == 0)
	goto done;
      else if (v->keyword[0] == '\377')
	command_name = v->keyword;
    }

    // read command name
    if (command_name == 0)
      command_name = va_arg(val, const char *);
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
    } else if (argtype->internal == cpiKeywords
	       || argtype->internal == cpiMandatoryKeywords) {
      if (nrequired < 0)
	nrequired = nvalues;
      if (npositional < 0)
	npositional = nvalues;
      mandatory_keywords = (argtype->internal == cpiMandatoryKeywords);
      continue;
    } else if (argtype->internal == cpiIgnore) {
      v->argtype = argtype;
      nvalues++;
      continue;
    } else if (argtype->internal == cpiIgnoreRest) {
      if (nrequired < 0)
	nrequired = nvalues;
      ignore_rest = true;
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
    v->v.i = (mandatory_keywords ? -1 : 0);
    nvalues++;
  }

 done:
  
  if (nrequired < 0)
    nrequired = nvalues;
  if (npositional < 0)
    npositional = nvalues;

  // assign arguments to positions
  int npositional_supplied = 0;
  StringAccum keyword_error_sa;
  bool any_keywords = false;
  
  for (int i = 0; i < args.size(); i++) {
    // check for keyword if past mandatory positional arguments
    if (npositional_supplied >= nrequired) {
      int result = assign_keyword_argument(args[i], npositional, nvalues);
      if (result == kwSuccess)
	any_keywords = true;
      else if (!any_keywords)
	goto positional_argument; // optional argument, or too many arguments
      else if (ignore_rest)
	/* no error */;
      else
	add_keyword_error(keyword_error_sa, result, args[i], argname, i);
      continue;
    }

    // otherwise, assign positional argument
   positional_argument:
    if (npositional_supplied < npositional)
      cp_values[npositional_supplied].v_string = args[i];
    npositional_supplied++;
  }
  
  // report keyword argument errors
  if (keyword_error_sa.length() && !keywords_only)
    return finish_keyword_error("bad keyword(s) %s\n(valid keywords are %s)", keyword_error_sa.cc(), npositional, nvalues, errh);
  
  // report missing mandatory keywords
  for (int i = npositional; i < nvalues; i++)
    if (cp_values[i].v.i < 0) {
      if (keyword_error_sa.length()) keyword_error_sa << ", ";
      keyword_error_sa << cp_values[i].keyword;
    }
  if (keyword_error_sa.length())
    return errh->error("missing mandatory keyword(s) %s", keyword_error_sa.cc());
  
  // if wrong number of arguments, print signature
  if (npositional_supplied < nrequired
      || (npositional_supplied > npositional && !ignore_rest)) {
    StringAccum signature;
    for (int i = 0; i < npositional; i++) {
      if (i == nrequired)
	signature << (nrequired > 0 ? " [" : "[");
      if (i)
	signature << separator;
      if (const cp_argtype *t = cp_values[i].argtype)
	signature << t->description;
      else
	signature << "??";
    }
    if (ignore_rest)
      signature << "...";
    if (nrequired < npositional)
      signature << "]";
    if (npositional < nvalues) {
      if (npositional)
	signature << separator;
      signature << "[keywords]";
    }

    const char *whoops = (npositional_supplied > npositional ? "too many" : "too few");
    if (signature.length())
      errh->error("%s %ss; expected `%s'", whoops, argname, signature.cc());
    else
      errh->error("expected empty %s list", argname);
    return -1;
  }

  // clear 'argtype' on unused arguments
  for (int i = npositional_supplied; i < npositional; i++)
    cp_values[i].argtype = 0;
  for (int i = npositional; i < nvalues; i++)
    if (cp_values[i].v.i <= 0)
      cp_values[i].argtype = 0;
  
  // parse arguments
  char argname_buf[128];
  int argname_offset;
  sprintf(argname_buf, "%s %n", argname, &argname_offset);

  for (int i = 0; i < npositional; i++) {
    cp_value *v = &cp_values[i];
    if (v->argtype) {
      sprintf(argname_buf + argname_offset, "%d", i + 1);
      v->argtype->parse(v, v->v_string, errh, argname_buf  CP_PASS_CONTEXT);
    }
  }
  for (int i = npositional; i < nvalues; i++) {
    cp_value *v = &cp_values[i];
    if (v->argtype) {
      StringAccum sa;
      sa << "keyword " << v->keyword;
      v->argtype->parse(v, v->v_string, errh, sa.cc()  CP_PASS_CONTEXT);
    }
  }

  // check for failure
  if (errh->nerrors() != nerrors_in)
    return -1;
  
  // if success, actually set the values
  int nset = 0;
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

String
cp_unparse_real2(unsigned real, int frac_bits)
{
  // adopted from Knuth's TeX function print_scaled, hope it is correct in
  // this context but not sure.
  // Works well with cp_real2 above; as an invariant,
  // unsigned x, y;
  // cp_real2(cp_unparse_real2(x, FRAC_BITS), FRAC_BITS, &y) == true && x == y
  
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
cp_unparse_real2(int real, int frac_bits)
{
  if (real < 0)
    return "-" + cp_unparse_real2(static_cast<unsigned>(-real), frac_bits);
  else
    return cp_unparse_real2(static_cast<unsigned>(real), frac_bits);
}

String
cp_unparse_real10(unsigned real, int frac_digits)
{
  unsigned one = exp10(frac_digits);
  unsigned int_part = real / one;
  unsigned frac_part = real - (int_part * one);

  if (frac_part == 0)
    return String(int_part);

  StringAccum sa(30);
  sa << int_part << '.';
  if (char *x = sa.extend(frac_digits))
    sprintf(x, "%0*d", frac_digits, frac_part);
  else				// out of memory
    return sa.take_string();

  // remove trailing zeros from fraction part
  while (sa.back() == '0')
    sa.pop_back();

  return sa.take_string();
}

String
cp_unparse_real10(int real, int frac_digits)
{
  if (real < 0)
    return "-" + cp_unparse_real10(static_cast<unsigned>(-real), frac_digits);
  else
    return cp_unparse_real10(static_cast<unsigned>(real), frac_digits);
}

String
cp_unparse_milliseconds(int ms)
{
  return cp_unparse_real10(ms, 3);
}


// initialization and cleanup

void
cp_va_static_initialize()
{
  assert(!cp_values);
  
  cp_register_argtype(cpOptional, "<optional arguments marker>", 0, default_parsefunc, default_storefunc, cpiOptional);
  cp_register_argtype(cpKeywords, "<keyword arguments marker>", 0, default_parsefunc, default_storefunc, cpiKeywords);
  cp_register_argtype(cpMandatoryKeywords, "<mandatory keyword arguments marker>", 0, default_parsefunc, default_storefunc, cpiMandatoryKeywords);
  cp_register_argtype(cpIgnore, "<ignored argument>", 0, default_parsefunc, default_storefunc, cpiIgnore);
  cp_register_argtype(cpIgnoreRest, "<ignore rest marker>", 0, default_parsefunc, default_storefunc, cpiIgnoreRest);
  
  cp_register_argtype(cpArgument, "arg", 0, default_parsefunc, default_storefunc, cpiArgument);
  cp_register_argtype(cpArguments, "args", 0, default_parsefunc, default_storefunc, cpiArguments);
  cp_register_argtype(cpString, "string", 0, default_parsefunc, default_storefunc, cpiString);
  cp_register_argtype(cpWord, "word", 0, default_parsefunc, default_storefunc, cpiWord);
  cp_register_argtype(cpKeyword, "keyword", 0, default_parsefunc, default_storefunc, cpiKeyword);
  cp_register_argtype(cpBool, "bool", 0, default_parsefunc, default_storefunc, cpiBool);
  cp_register_argtype(cpByte, "byte", 0, default_parsefunc, default_storefunc, cpiByte);
  cp_register_argtype(cpShort, "short", 0, default_parsefunc, default_storefunc, cpiShort);
  cp_register_argtype(cpUnsignedShort, "unsigned short", 0, default_parsefunc, default_storefunc, cpiUnsignedShort);
  cp_register_argtype(cpInteger, "int", 0, default_parsefunc, default_storefunc, cpiInteger);
  cp_register_argtype(cpUnsigned, "unsigned", 0, default_parsefunc, default_storefunc, cpiUnsigned);
  cp_register_argtype(cpReal2, "real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiReal2);
  cp_register_argtype(cpUnsignedReal2, "unsigned real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiUnsignedReal2);
  cp_register_argtype(cpReal10, "real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiReal10);
  cp_register_argtype(cpUnsignedReal10, "unsigned real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiUnsignedReal10);
  cp_register_argtype(cpMilliseconds, "time in seconds", 0, default_parsefunc, default_storefunc, cpiMilliseconds);
  cp_register_argtype(cpTimeval, "seconds since the epoch", 0, default_parsefunc, default_storefunc, cpiTimeval);
  cp_register_argtype(cpIPAddress, "IP address", 0, default_parsefunc, default_storefunc, cpiIPAddress);
  cp_register_argtype(cpIPPrefix, "IP address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIPPrefix);
  cp_register_argtype(cpIPAddressOrPrefix, "IP address or prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIPAddressOrPrefix);
  cp_register_argtype(cpIPAddressSet, "set of IP addresses", 0, default_parsefunc, default_storefunc, cpiIPAddressSet);
  cp_register_argtype(cpEthernetAddress, "Ethernet address", 0, default_parsefunc, default_storefunc, cpiEthernetAddress);
#ifndef CLICK_TOOL
  cp_register_argtype(cpElement, "element name", 0, default_parsefunc, default_storefunc, cpiElement);
  cp_register_argtype(cpHandlerName, "handler name", cpArgStore2, default_parsefunc, default_storefunc, cpiHandlerName);
  cp_register_argtype(cpHandler, "handler name", cpArgStore2, default_parsefunc, default_storefunc, cpiHandler);
  cp_register_argtype(cpReadHandler, "read handler name", cpArgStore2, default_parsefunc, default_storefunc, cpiReadHandler);
  cp_register_argtype(cpWriteHandler, "write handler name", cpArgStore2, default_parsefunc, default_storefunc, cpiWriteHandler);
#endif
  cp_register_argtype(cpIP6Address, "IPv6 address", 0, default_parsefunc, default_storefunc, cpiIP6Address);
  cp_register_argtype(cpIP6Prefix, "IPv6 address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6Prefix);
  cp_register_argtype(cpIP6AddressOrPrefix, "IPv6 address or prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6AddressOrPrefix);
#ifdef HAVE_IPSEC
  cp_register_argtype(cpDesCblock, "DES cipher block", 0, default_parsefunc, default_storefunc, cpiDesCblock);
#endif

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
