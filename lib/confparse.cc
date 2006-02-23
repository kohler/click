// -*- c-basic-offset: 4; related-file-name: "../include/click/confparse.hh" -*-
/*
 * confparse.{cc,hh} -- configuration string parsing
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000-2001 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2004-2006 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/ipaddress.hh>
#include <click/ipaddresslist.hh>
#include <click/etheraddress.hh>
#ifdef HAVE_IP6
# include <click/ip6address.hh>
# include <click/ip6flowid.hh>
#endif
#ifndef CLICK_TOOL
# include <click/router.hh>
# include <click/handlercall.hh>
# include <click/nameinfo.hh>
# include <click/standard/addressinfo.hh>
# define CP_CONTEXT_ARG , Element *context
# define CP_PASS_CONTEXT , context
#else
# include <click/hashmap.hh>
# include <click/timestamp.hh>
# define CP_CONTEXT_ARG
# define CP_PASS_CONTEXT
#endif
#ifdef CLICK_USERLEVEL
# include <pwd.h>
#endif
#include <stdarg.h>
CLICK_DECLS

int cp_errno;

const char *
cp_skip_space(const char *begin, const char *end)
{
    while (begin < end && isspace((unsigned char) *begin))
	begin++;
    return begin;
}

bool
cp_eat_space(String &str)
{
    const char *begin = str.begin(), *end = str.end();
    const char *space = cp_skip_space(begin, end);
    str = str.substring(space, end);
    return space != begin;
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

bool
cp_is_click_id(const String &str)
{
  const unsigned char *s = reinterpret_cast<const unsigned char*>(str.data());
  int len = str.length();
  for (int i = 0; i < len; i++)
    if (isalnum(s[i]) || s[i] == '_' || s[i] == '@')
      /* character OK */;
    else if (s[i] != '/' || i == 0 || i == len - 1 || s[i+1] == '/')
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

static const char *
skip_comment(const char *s, const char *end)
{
  assert(s + 1 < end && *s == '/' && (s[1] == '/' || s[1] == '*'));

  if (s[1] == '/') {
    for (s += 2; s + 1 < end && *s != '\n' && *s != '\r'; s++)
      /* nada */;
    if (s + 1 < end && *s == '\r' && s[1] == '\n')
      s++;
    return s + 1;
  } else { /* s[1] == '*' */
    for (s += 2; s + 2 < end && (*s != '*' || s[1] != '/'); s++)
      /* nada */;
    return s + 2;
  }
}

static const char *
skip_backslash(const char *s, const char *end)
{
  assert(s < end && *s == '\\');

  if (s + 1 >= end)
    return s + 1;
  else if (s[1] == '<') {
    for (s += 2; s < end; )
      if (*s == '>')
	return s + 1;
      else if (*s == '/' && s + 1 < end && (s[1] == '/' || s[1] == '*'))
	s = skip_comment(s, end);
      else
	s++;
    return s;
  } else if (s[1] == '\r' && s + 2 < end && s[2] == '\n')
    return s + 3;
  else
    return s + 2;
}

static const char *
skip_double_quote(const char *s, const char *end)
{
  assert(s < end && *s == '\"');

  for (s++; s < end; )
    if (*s == '\\')
      s = skip_backslash(s, end);
    else if (*s == '\"')
      return s + 1;
    else
      s++;

  return end;
}

static const char *
skip_single_quote(const char *s, const char *end)
{
  assert(s < end && *s == '\'');

  for (s++; s < end; s++)
    if (*s == '\'')
      return s + 1;

  return end;
}

const char *
cp_skip_comment_space(const char *begin, const char *end)
{
  for (; begin < end; begin++) {
    if (isspace((unsigned char) *begin))
      /* nada */;
    else if (*begin == '/' && begin + 1 < end && (begin[1] == '/' || begin[1] == '*'))
      begin = skip_comment(begin, end) - 1;
    else
      break;
  }
  return begin;
}

static String
partial_uncomment(const String &str, int i, int *comma_pos)
{
  const char *s = str.data() + i;
  const char *end = str.end();

  // skip initial spaces
  s = cp_skip_comment_space(s, end);

  // accumulate text, skipping comments
  StringAccum sa;
  const char *left = s;
  const char *right = s;
  bool closed = false;

  while (s < end) {
    if (isspace((unsigned char) *s))
      s++;
    else if (*s == '/' && s + 1 < end && (s[1] == '/' || s[1] == '*')) {
      s = skip_comment(s, end);
      closed = true;
    } else if (*s == ',' && comma_pos)
      break;
    else {
      if (closed) {
	sa << str.substring(left, right) << ' ';
	left = s;
	closed = false;
      }
      if (*s == '\'')
	s = skip_single_quote(s, end);
      else if (*s == '\"')
	s = skip_double_quote(s, end);
      else if (*s == '\\' && s + 1 < end && s[1] == '<')
	s = skip_backslash(s, end);
      else
	s++;
      right = s;
    }
  }

  if (comma_pos)
    *comma_pos = s - str.begin();
  if (!sa)
    return str.substring(left, right);
  else {
    sa << str.substring(left, right);
    return sa.take_string();
  }
}

String
cp_uncomment(const String &str)
{
  return partial_uncomment(str, 0, 0);
}

const char *
cp_process_backslash(const char *s, const char *end, StringAccum &sa)
{
  assert(s < end && *s == '\\');

  if (s == end - 1) {
    sa << '\\';
    return end;
  }
  
  switch (s[1]) {
    
   case '\r':
    return (s + 2 < end && s[2] == '\n' ? s + 3 : s + 2);

   case '\n':
    return s + 2;
    
   case 'a': sa << '\a'; return s + 2;
   case 'b': sa << '\b'; return s + 2;
   case 'f': sa << '\f'; return s + 2;
   case 'n': sa << '\n'; return s + 2;
   case 'r': sa << '\r'; return s + 2;
   case 't': sa << '\t'; return s + 2;
   case 'v': sa << '\v'; return s + 2;
    
   case '0': case '1': case '2': case '3':
   case '4': case '5': case '6': case '7': {
     int c = 0, d = 0;
     for (s++; s < end && *s >= '0' && *s <= '7' && d < 3; s++, d++)
       c = c*8 + *s - '0';
     sa << (char)c;
     return s;
   }
   
   case 'x': {
     int c = 0;
     for (s += 2; s < end; s++)
       if (*s >= '0' && *s <= '9')
	 c = c*16 + *s - '0';
       else if (*s >= 'A' && *s <= 'F')
	 c = c*16 + *s - 'A' + 10;
       else if (*s >= 'a' && *s <= 'f')
	 c = c*16 + *s - 'a' + 10;
       else
	 break;
     sa << (char)c;
     return s;
   }
   
   case '<': {
     int c = 0, d = 0;
     for (s += 2; s < end; s++) {
       if (*s == '>')
	 return s + 1;
       else if (*s >= '0' && *s <= '9')
	 c = c*16 + *s - '0';
       else if (*s >= 'A' && *s <= 'F')
	 c = c*16 + *s - 'A' + 10;
       else if (*s >= 'a' && *s <= 'f')
	 c = c*16 + *s - 'a' + 10;
       else if (*s == '/' && s + 1 < end && (s[1] == '/' || s[1] == '*')) {
	 s = skip_comment(s, end) - 1;
	 continue;
       } else
	 continue;	// space (ignore it) or random (error)
       if (++d == 2) {
	 sa << (char)c;
	 c = d = 0;
       }
     }
     // ran out of space in string
     return end;
   }

   case '\\': case '\'': case '\"': case '$':
   default:
    sa << s[1];
    return s + 2;
    
  }
}

String
cp_unquote(const String &in_str)
{
  String str = partial_uncomment(in_str, 0, 0);
  const char *s = str.data();
  const char *end = str.end();

  // accumulate a word
  StringAccum sa;
  const char *start = s;
  int quote_state = 0;

  for (; s < end; s++)
    switch (*s) {

     case '\"':
     case '\'':
      if (quote_state == 0) {
	sa << str.substring(start, s); // null string if start >= s
	start = s + 1;
	quote_state = *s;
      } else if (quote_state == *s) {
	sa << str.substring(start, s);
	start = s + 1;
	quote_state = 0;
      }
      break;
      
     case '\\':
      if (s + 1 < end && (quote_state == '\"'
			  || (quote_state == 0 && s[1] == '<'))) {
	sa << str.substring(start, s);
	start = cp_process_backslash(s, end, sa);
	s = start - 1;
      }
      break;
      
    }

  if (start == str.begin())
    return str;
  else {
    sa << str.substring(start, s);
    return sa.take_string();
  }
}

String
cp_quote(const String &str, bool allow_newlines)
{
  if (!str)
    return String("\"\"");
  
  const char *s = str.data();
  const char *end = str.end();
  
  StringAccum sa;
  const char *start = s;

  sa << '\"';
  
  for (; s < end; s++)
    switch (*s) {
      
     case '\\': case '\"': case '$':
      sa << str.substring(start, s) << '\\' << *s;
      start = s + 1;
      break;
      
     case '\t':
      sa << str.substring(start, s) << "\\t";
      start = s + 1;
      break;

     case '\r':
      sa << str.substring(start, s) << "\\r";
      start = s + 1;
      break;

     case '\n':
      if (!allow_newlines) {
	sa << str.substring(start, s) << "\\n";
	start = s + 1;
      }
      break;

     default:
      if ((unsigned char)*s < 32 || (unsigned char)*s >= 127) {
	unsigned u = (unsigned char)*s;
	sa << str.substring(start, s)
	   << '\\' << (char)('0' + (u >> 6))
	   << (char)('0' + ((u >> 3) & 7))
	   << (char)('0' + (u & 7));
	start = s + 1;
      }
      break;
      
    }
  
  sa << str.substring(start, s) << '\"';
  return sa.take_string();
}

void
cp_argvec(const String &conf, Vector<String> &args)
{
  // common case: no configuration
  int len = conf.length();
  if (len == 0)
    return;
  
  for (int pos = 0; pos < len; pos++) {
    String arg = partial_uncomment(conf, pos, &pos);
    // add the argument if it is nonempty or not the last argument
    if (arg || pos < len)
      args.push_back(arg);
  }
}

static const char *
skip_spacevec_item(const char *s, const char *end)
{
  while (s < end)
    switch (*s) {
      
     case '/':
      // a comment ends the item
      if (s + 1 < end && (s[1] == '/' || s[1] == '*'))
	return s;
      s++;
      break;
      
     case '\"':
      s = skip_double_quote(s, end);
      break;
      
     case '\'':
      s = skip_single_quote(s, end);
      break;

     case '\\':			// check for \<...> strings
      if (s + 1 < end && s[1] == '<')
	s = skip_backslash(s, end);
      else
	s++;
      break;
      
     case ' ':
     case '\f':
     case '\n':
     case '\r':
     case '\t':
     case '\v':
      return s;

     default:
      s++;
      break;
      
    }
  return s;
}

void
cp_spacevec(const String &conf, Vector<String> &vec)
{
  // common case: no configuration
  if (conf.length() == 0)
    return;

  // collect arguments with cp_pop_spacevec
  const char *s = conf.data();
  const char *end = conf.end();
  while ((s = cp_skip_comment_space(s, end)) < end) {
    const char *t = skip_spacevec_item(s, end);
    vec.push_back(conf.substring(s, t));
    s = t;
  }
}

String
cp_pop_spacevec(String &conf)
{
  const char *item = cp_skip_comment_space(conf.begin(), conf.end());
  const char *item_end = skip_spacevec_item(item, conf.end());
  String answer = conf.substring(item, item_end);
  item_end = cp_skip_comment_space(item_end, conf.end());
  conf = conf.substring(item_end, conf.end());
  return answer;
}

String
cp_unargvec(const Vector<String> &args)
{
  if (args.size() == 0)
    return String();
  else if (args.size() == 1)
    return args[0];
  else {
    StringAccum sa;
    sa << args[0];
    for (int i = 1; i < args.size(); i++)
      sa << ", " << args[i];
    return sa.take_string();
  }
}

String
cp_unspacevec(const String *begin, const String *end)
{
  StringAccum sa;
  for (; begin < end; begin++)
    sa << *begin << ' ';
  sa.pop_back();
  return sa.take_string();
}


// PARSING STRINGS

bool
cp_string(const String &str, String *return_value, String *rest)
{
  const char *s = str.data();
  const char *end = str.end();

  // accumulate a word
  while (s < end)
    switch (*s) {
      
     case ' ':
     case '\f':
     case '\n':
     case '\r':
     case '\t':
     case '\v':
      goto done;

     case '\"':
      s = skip_double_quote(s, end);
      break;

     case '\'':
      s = skip_single_quote(s, end);
      break;

     case '\\':
      if (s + 1 < end && s[1] == '<')
	s = skip_backslash(s, end);
      else
	s++;
      break;

     default:
      s++;
      break;
      
    }
  
 done:
  if (s == str.begin() || (!rest && s != end))
    return false;
  else {
    if (rest)
      *rest = str.substring(s, end);
    *return_value = cp_unquote(str.substring(str.begin(), s));
    return true;
  }
}

bool
cp_word(const String &str, String *return_value, String *rest)
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
  const char *end = str.end();

  // accumulate a word
  for (; s < end; s++)
    switch (*s) {
      
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
      if (!isalnum((unsigned char) *s))
	return false;
      break;
      
    }
  
 done:
  if (s == str.begin() || (!rest && s < end))
    return false;
  else {
    *return_value = str.substring(str.begin(), s);
    if (rest) {
      for (; s < end; s++)
	if (!isspace((unsigned char) *s))
	  break;
      *rest = str.substring(s, end);
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
  
  if (len == 1 && (s[0] == '0' || s[0] == 'n' || s[0] == 'f'))
    *return_value = false;
  else if (len == 1 && (s[0] == '1' || s[0] == 'y' || s[0] == 't'))
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

const char *
cp_unsigned(const char *begin, const char *end, int base, uint32_t *return_value)
{
  const char *s = begin;
  if (s < end && *s == '+')
    s++;

  if ((base == 0 || base == 16) && s + 1 < end && *s == '0'
      && (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
    base = 16;
  } else if (base == 0 && s < end && *s == '0')
    base = 8;
  else if (base == 0)
    base = 10;
  else if (base < 2 || base > 36) {
    cp_errno = CPE_INVALID;
    return begin;
  }

  if (s >= end || *s == '_')	// no digits or initial underscore
    return begin;

  uint32_t overflow_val = 0xFFFFFFFFU / base;
  int32_t overflow_digit = 0xFFFFFFFFU - (overflow_val * base);
  
  uint32_t val = 0;
  cp_errno = CPE_FORMAT;
  
  while (s < end) {
    // find digit
    int digit;
    if (*s >= '0' && *s <= '9')
      digit = *s - '0';
    else if (*s >= 'A' && *s <= 'Z')
      digit = *s - 'A' + 10;
    else if (*s >= 'a' && *s <= 'z')
      digit = *s - 'a' + 10;
    else if (*s == '_' && s + 1 < end && s[1] != '_')
      // skip underscores between digits
      goto next_digit;
    else
      digit = 36;
    if (digit >= base)
      break;
    else if (val == 0 && cp_errno == CPE_FORMAT)
      cp_errno = CPE_OK;
    // check for overflow
    if (val > overflow_val || (val == overflow_val && digit > overflow_digit))
      cp_errno = CPE_OVERFLOW;
    // assign new value
    val = val * base + digit;
   next_digit:
    s++;
  }

  if (cp_errno == CPE_FORMAT)
    return begin;
  else {
    *return_value = (cp_errno ? 0xFFFFFFFFU : val);
    return (s > begin && s[-1] == '_' ? s - 1 : s);
  }
}

bool cp_unsigned(const String &str, int base, uint32_t *return_value)
{
  uint32_t u;
  const char *s = cp_unsigned(str.begin(), str.end(), base, &u);
  if (s == str.end() && str.length()) {
    *return_value = u;
    return true;
  } else
    return false;
}

const char *
cp_integer(const char *begin, const char *end, int base, int32_t *return_value)
{
  const char *s = begin;
  bool negative = false;
  if (s + 1 < end && *s == '-' && s[1] != '+') {
    negative = true;
    s++;
  }

  uint32_t value;
  if ((end = cp_unsigned(s, end, base, &value)) == s)
    return begin;

  uint32_t max = (negative ? 0x80000000U : 0x7FFFFFFFU);
  if (value > max) {
    cp_errno = CPE_OVERFLOW;
    value = max;
  }

  *return_value = (negative ? -value : value);
  return end;
}

bool cp_integer(const String &str, int base, int32_t *return_value)
{
  int32_t i;
  const char *s = cp_integer(str.begin(), str.end(), base, &i);
  if (s == str.end() && str.length()) {
    *return_value = i;
    return true;
  } else
    return false;
}


#ifdef HAVE_INT64_TYPES

static uint64_t unsigned64_overflow_vals[] = { 0, 0, 9223372036854775807ULL, 6148914691236517205ULL, 4611686018427387903ULL, 3689348814741910323ULL, 3074457345618258602ULL, 2635249153387078802ULL, 2305843009213693951ULL, 2049638230412172401ULL, 1844674407370955161ULL, 1676976733973595601ULL, 1537228672809129301ULL, 1418980313362273201ULL, 1317624576693539401ULL, 1229782938247303441ULL, 1152921504606846975ULL, 1085102592571150095ULL, 1024819115206086200ULL, 970881267037344821ULL, 922337203685477580ULL, 878416384462359600ULL, 838488366986797800ULL, 802032351030850070ULL, 768614336404564650ULL, 737869762948382064ULL, 709490156681136600ULL, 683212743470724133ULL, 658812288346769700ULL, 636094623231363848ULL, 614891469123651720ULL, 595056260442243600ULL, 576460752303423487ULL, 558992244657865200ULL, 542551296285575047ULL, 527049830677415760ULL };

const char *
cp_unsigned(const char *begin, const char *end, int base, uint64_t *return_value)
{
  const char *s = begin;
  if (s < end && *s == '+')
    s++;

  if ((base == 0 || base == 16) && s + 1 < end && *s == '0'
      && (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
    base = 16;
  } else if (base == 0 && *s == '0')
    base = 8;
  else if (base == 0)
    base = 10;
  else if (base < 2 || base > 36) {
    cp_errno = CPE_INVALID;
    return begin;
  }

  if (s >= end || *s == '_')	// no digits or initial underscore
    return begin;

  uint64_t overflow_val = unsigned64_overflow_vals[base];
  int64_t overflow_digit = 0xFFFFFFFFFFFFFFFFULL - (overflow_val * base);

  uint64_t val = 0;
  cp_errno = CPE_FORMAT;
  
  while (s < end) {
    // find digit
    int digit;
    if (*s >= '0' && *s <= '9')
      digit = *s - '0';
    else if (*s >= 'A' && *s <= 'Z')
      digit = *s - 'A' + 10;
    else if (*s >= 'a' && *s <= 'z')
      digit = *s - 'a' + 10;
    else if (*s == '_' && s + 1 < end && s[1] != '_')
      // skip underscores between digits
      goto next_digit;
    else
      digit = 36;
    if (digit >= base)
      break;
    else if (val == 0 && cp_errno == CPE_FORMAT)
      cp_errno = CPE_OK;
    // check for overflow
    if (val > overflow_val || (val == overflow_val && digit > overflow_digit))
      cp_errno = CPE_OVERFLOW;
    // assign new value
    val = val * base + digit;
   next_digit:
    s++;
  }

  if (cp_errno == CPE_FORMAT)
    return begin;
  else {
    *return_value = (cp_errno ? 0xFFFFFFFFFFFFFFFFULL : val);
    return (s > begin && s[-1] == '_' ? s - 1 : s);
  }
}

bool
cp_unsigned(const String &str, int base, uint64_t *return_value)
{
  uint64_t q;
  const char *s = cp_unsigned64(str.begin(), str.end(), base, &q);
  if (s == str.end() && str.length()) {
    *return_value = q;
    return true;
  } else
    return false;
}

const char *
cp_integer(const char *begin, const char *end, int base, int64_t *return_value)
{
  const char *s = begin;
  bool negative = false;
  if (s + 1 < end && *s == '-' && s[1] != '+') {
    negative = true;
    s++;
  }

  uint64_t value;
  if ((end = cp_unsigned(s, end, base, &value)) == s)
    return begin;

  uint64_t max = (negative ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL);
  if (value > max) {
    cp_errno = CPE_OVERFLOW;
    value = max;
  }

  *return_value = (negative ? -value : value);
  return end;
}

bool
cp_integer(const String &str, int base, int64_t *return_value)
{
  int64_t q;
  const char *s = cp_integer(str.begin(), str.end(), base, &q);
  if (s == str.end() && str.length()) {
    *return_value = q;
    return true;
  } else
    return false;
}

#endif

#ifdef CLICK_USERLEVEL

bool
cp_file_offset(const String &str, off_t *return_value)
{
# if SIZEOF_OFF_T == 4
  return cp_unsigned(str, reinterpret_cast<uint32_t *>(return_value));
# elif SIZEOF_OFF_T == 8
  return cp_unsigned(str, reinterpret_cast<uint64_t *>(return_value));
# else
#  error "unexpected sizeof(off_t)"
# endif
}

#endif


// PARSING REAL NUMBERS

static uint32_t exp10val[] = { 1, 10, 100, 1000, 10000, 100000, 1000000,
			       10000000, 100000000, 1000000000 };

bool
cp_unsigned_real10(const String &str, int frac_digits, int exponent_delta,
		   uint32_t *return_int_part, uint32_t *return_frac_part)
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
  for (int_s = s; s < last; s++)
    if (!(isdigit((unsigned char) *s) || (*s == '_' && s > int_s && s < last - 1 && s[1] != '_')))
      break;
  int int_chars = s - int_s;
  
  // find fractional part of string
  const char *frac_s;
  int frac_chars;
  if (s < last && *s == '.') {
    for (frac_s = ++s; s < last; s++)
      if (!(isdigit((unsigned char) *s) || (*s == '_' && s > frac_s && s < last - 1 && s[1] != '_')))
	break;
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
    if (s >= last || !isdigit((unsigned char) *s))
      return false;
    
    // XXX overflow?
    for (; s < last; s++)
      if (isdigit((unsigned char) *s))
	exponent = 10*exponent + *s - '0';
      else if (*s != '_' || s == last - 1 || s[1] == '_')
	break;
    
    if (negexp)
      exponent = -exponent;
  }
  
  if (s != last)
    return false;

  // OK! now create the result
  // determine integer part; careful about overflow
  uint32_t int_part = 0;
  cp_errno = CPE_OK;
  exponent += exponent_delta;
  int digit;
  
  for (int i = 0; i < int_chars + exponent; i++) {
    if (i < int_chars)
      digit = int_s[i] - '0';
    else if (i - int_chars < frac_chars)
      digit = frac_s[i - int_chars] - '0';
    else
      digit = 0;
    if (digit == ('_' - '0'))
      continue;
    if (int_part > 0x19999999U || (int_part == 0x19999999U && digit > 5))
      cp_errno = CPE_OVERFLOW;
    int_part = int_part * 10 + digit;
  }
  
  // determine fraction part
  uint32_t frac_part = 0;
  digit = 0;
  
  for (int i = 0; i <= frac_digits; i++) {
    if (i + exponent + int_chars < 0)
      digit = 0;
    else if (i + exponent < 0)
      digit = int_s[i + exponent + int_chars] - '0';
    else if (i + exponent < frac_chars)
      digit = frac_s[i + exponent] - '0';
    else
      digit = 0;
    if (digit == ('_' - '0'))
      continue;
    // skip out on the last digit
    if (i == frac_digits)
      break;
    // no overflow possible b/c frac_digits was limited
    frac_part = frac_part * 10 + digit;
  }

  // round fraction part if required
  if (digit >= 5) {
    if (frac_part == exp10val[frac_digits] - 1) {
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
    frac_part = exp10val[frac_digits] - 1;
  }

  //click_chatter("%d: %u %u", frac_digits, int_part, frac_part);
  *return_int_part = int_part;
  *return_frac_part = frac_part;
  return true;
}

static bool
unsigned_real10_2to1(uint32_t int_part, uint32_t frac_part, int frac_digits,
		     uint32_t *return_value)
{
  uint32_t one = exp10val[frac_digits];
  uint32_t int_max = 0xFFFFFFFFU / one;
  uint32_t frac_max = 0xFFFFFFFFU - int_max * one;
  if (int_part > int_max || (int_part == int_max && frac_part > frac_max)) {
    cp_errno = CPE_OVERFLOW;
    *return_value = 0xFFFFFFFFU;
  } else
    *return_value = int_part * one + frac_part;
  return true;
}

bool
cp_unsigned_real10(const String &str, int frac_digits,
		   uint32_t *return_int_part, uint32_t *return_frac_part)
{
  return cp_unsigned_real10(str, frac_digits, 0,
			    return_int_part, return_frac_part);
}

bool
cp_unsigned_real10(const String &str, int frac_digits, int exponent_delta,
		   uint32_t *return_value)
{
  uint32_t int_part, frac_part;
  if (!cp_unsigned_real10(str, frac_digits, exponent_delta, &int_part, &frac_part))
    return false;
  else
    return unsigned_real10_2to1(int_part, frac_part, frac_digits, return_value);
}

bool
cp_unsigned_real10(const String &str, int frac_digits,
		   uint32_t *return_value)
{
  uint32_t int_part, frac_part;
  if (!cp_unsigned_real10(str, frac_digits, 0, &int_part, &frac_part))
    return false;
  else
    return unsigned_real10_2to1(int_part, frac_part, frac_digits, return_value);
}

static uint32_t ureal2_digit_fractions[] = {
  0x00000000, 0x19999999, 0x33333333, 0x4CCCCCCC, 0x66666666,
  0x80000000, 0x99999999, 0xB3333333, 0xCCCCCCCC, 0xE6666666
};

bool
cp_unsigned_real2(const String &str, int frac_bits, uint32_t *return_value)
{
  if (frac_bits < 0 || frac_bits > CP_REAL2_MAX_FRAC_BITS) {
    cp_errno = CPE_INVALID;
    return false;
  }
  
  uint32_t int_part, frac_part;
  if (!cp_unsigned_real10(str, 9, 0, &int_part, &frac_part)) {
    cp_errno = CPE_FORMAT;
    return false;
  }

  // method from Knuth's TeX, round_decimals. Works well with
  // cp_unparse_real2 below
  uint32_t fraction = 0;
  for (int i = 0; i < 9; i++) {
    uint32_t digit = frac_part % 10;
    fraction = (fraction / 10) + ureal2_digit_fractions[digit];
    frac_part /= 10;
  }
  fraction = ((fraction >> (31 - frac_bits)) + 1) / 2;

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


// Parsing signed reals

static bool
cp_real_base(const String &in_str, int frac_digits, int32_t *return_value,
	     bool (*func)(const String &, int, uint32_t *))
{
  String str = in_str;
  bool negative = false;
  if (str.length() > 1 && str[0] == '-' && str[1] != '+') {
    negative = true;
    str = str.substring(1);
  }

  uint32_t value;
  if (!func(str, frac_digits, &value))
    return false;

  // check for overflow
  uint32_t umax = (negative ? 0x80000000 : 0x7FFFFFFF);
  if (value > umax) {
    cp_errno = CPE_OVERFLOW;
    value = umax;
  }

  *return_value = (negative ? -value : value);
  return true;
}

bool
cp_real10(const String &str, int frac_digits, int32_t *return_value)
{
  return cp_real_base(str, frac_digits, return_value, cp_unsigned_real10);
}

bool
cp_real2(const String &str, int frac_bits, int32_t *return_value)
{
  return cp_real_base(str, frac_bits, return_value, cp_unsigned_real2);
}

#ifdef HAVE_FLOAT_TYPES
bool
cp_double(const String &in_str, double *result)
{
  cp_errno = CPE_FORMAT;
  if (in_str.length() == 0 || isspace((unsigned char) in_str[0]))
    // check for space because strtod() accepts leading whitespace
    return false;

  errno = 0;
  String str = in_str;
  char *endptr;
  double val = strtod(str.c_str(), &endptr);
  if (*endptr)			// bad format; garbage after number
    return false;

  cp_errno = (errno == ERANGE ? CPE_OVERFLOW : 0);
  *result = val;
  return true;
}
#endif

// PARSING TIME

static const char *
read_unit(const char *s, const char *end, 
	  const char *unit_begin_in, int unit_len, const char *prefix,
	  int *power, int *factor)
{
  const char *work = end;
  const unsigned char *unit_begin = reinterpret_cast<const unsigned char *>(unit_begin_in);
  const unsigned char *unit = unit_begin + unit_len;
  if (unit > unit_begin && unit[-1] == 0)
    unit--;
  while (unit > unit_begin) {
    if (unit[-1] < 4) {
	int type = unit[-1];
	assert(unit - 3 - (type >= 2) >= unit_begin);
	*factor = unit[-2];
	if (type >= 2)
	    unit--, *factor += 256 * unit[-2];
	*power = (type & 1 ? -(int) unit[-3] : unit[-3]);

	// check for SI prefix
	if (prefix && work > s) {
	    for (; *prefix; prefix += 2)
		if (*prefix == work[-1]) {
		    *power += (int) prefix[1] - 64;
		    work--;
		    break;
		}
	}
      
	while (work > s && isspace((unsigned char) work[-1]))
	    work--;
	return work;
    } else if (unit[-1] != (unsigned char) work[-1]) {
      while (unit > unit_begin && unit[-1] >= 4)
	unit--;
      unit -= 3 + (unit[-1] >= 2);
      work = end;
    } else {
      unit--;
      work--;
    }
  }
  return end;
}

static const char seconds_units[] = "\
\0\1\0s\
\0\1\0sec\
\1\6\0m\
\1\6\0min\
\2\044\0h\
\2\044\0hr\
\2\003\140\2d\
\2\003\140\2day";
static const char seconds_prefixes[] = "m\075u\072n\067";

bool cp_seconds_as(int want_power, const String &str, uint32_t *return_value)
{
  int power = 0, factor = 1;
  const char *after_unit = read_unit(str.begin(), str.end(), seconds_units, sizeof(seconds_units), seconds_prefixes, &power, &factor);
  if (!cp_unsigned_real10(str.substring(str.begin(), after_unit), want_power, power, return_value))
    return false;
  if (*return_value > 0xFFFFFFFFU / factor) {
    cp_errno = CPE_OVERFLOW;
    *return_value = 0xFFFFFFFFU;
  } else
    *return_value *= factor;
  return true;
}

bool cp_seconds_as_milli(const String &str_in, uint32_t *return_value)
{
  return cp_seconds_as(3, str_in, return_value);
}

bool cp_seconds_as_micro(const String &str_in, uint32_t *return_value)
{
  return cp_seconds_as(6, str_in, return_value);
}

bool cp_time(const String &str, Timestamp* return_value)
{
    int power = 0, factor = 1;
    const char *after_unit = read_unit(str.begin(), str.end(), seconds_units, sizeof(seconds_units), seconds_prefixes, &power, &factor);
    uint32_t sec, nsec;
    if (!cp_unsigned_real10(str.substring(str.begin(), after_unit), 9, power, &sec, &nsec))
	return false;
    if (factor != 1) {
	nsec *= factor;
	int delta = nsec / 1000000000;
	nsec -= delta * 1000000000;
	sec = (sec * factor) + delta;
    }
    *return_value = Timestamp::make_nsec(sec, nsec);
    return true;
}

bool cp_time(const String &str, timeval *return_value)
{
    return cp_time(str, (Timestamp*) return_value);
}


static const char byte_bandwidth_units[] = "\
\3\175\1baud\
\3\175\1bps\
\3\175\1b/s\
\0\1\0Bps\
\0\1\0B/s\
";
static const char byte_bandwidth_prefixes[] = "k\103K\103M\106G\111";

bool
cp_bandwidth(const String &str, uint32_t *return_value)
{
  int power = 0, factor = 1;
  const char *after_unit = read_unit(str.begin(), str.end(), byte_bandwidth_units, sizeof(byte_bandwidth_units), byte_bandwidth_prefixes, &power, &factor);
  if (!cp_unsigned_real10(str.substring(str.begin(), after_unit), 0, power, return_value))
    return false;
  if (*return_value > 0xFFFFFFFFU / factor) {
    cp_errno = CPE_OVERFLOW;
    *return_value = 0xFFFFFFFFU;
  } else {
    if (after_unit == str.end())
      cp_errno = CPE_NOUNITS;
    *return_value *= factor;
  }
  return true;
}



// PARSING IPv4 ADDRESSES

static int
ip_address_portion(const String &str, unsigned char *value)
{
  const unsigned char *s = reinterpret_cast<const unsigned char*>(str.data());
  int len = str.length();
  int pos = 0, part;

  for (int d = 0; d < 4; d++) {
    if (d && pos < len && s[pos] == '.')
      pos++;
    if (pos >= len) {
      memset(value + d, 0, 4 - d);
      return d;
    } else if (!isdigit(s[pos]))
      return 0;
    for (part = 0; pos < len && isdigit(s[pos]) && part <= 255; pos++)
      part = part*10 + s[pos] - '0';
    if (part > 255)
      return 0;
    value[d] = part;
  }

  return (pos == len ? 4 : 0);
}

bool
cp_ip_address(const String &str, unsigned char *return_value
	      CP_CONTEXT_ARG)
{
  unsigned char value[4];
  if (ip_address_portion(str, value) == 4) {
    memcpy(return_value, value, 4);
    return true;
  }
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
  do {
    unsigned char value[4], mask[4];

    int slash = str.find_right('/');
    String ip_part, mask_part;
    if (slash < 0 && allow_bare_address)
      ip_part = str;
    else if (slash >= 0 && slash < str.length() - 1) {
      ip_part = str.substring(0, slash);
      mask_part = str.substring(slash + 1);
    } else
      goto failure;

    // read IP address part
    int good_ip_bytes = ip_address_portion(ip_part, value);
    if (good_ip_bytes == 0) {
      if (!cp_ip_address(ip_part, value  CP_PASS_CONTEXT))
	goto failure;
      good_ip_bytes = 4;
    }

    // check mask
    if (allow_bare_address && !mask_part.length() && good_ip_bytes == 4) {
      memcpy(return_value, value, 4);
      return_mask[0] = return_mask[1] = return_mask[2] = return_mask[3] = 255;
      return true;
    }

    // check for complete IP address
    int relevant_bits;
    if (good_ip_bytes == 4 && cp_ip_address(mask_part, mask  CP_PASS_CONTEXT))
      /* OK */;
  
    else if (cp_integer(mask_part, &relevant_bits)
	     && relevant_bits >= 0 && relevant_bits <= 32) {
      // set bits
      unsigned umask = 0;
      if (relevant_bits > 0)
	umask = 0xFFFFFFFFU << (32 - relevant_bits);
      for (int i = 0; i < 4; i++, umask <<= 8)
	mask[i] = (umask >> 24) & 255;
      if (good_ip_bytes < (relevant_bits + 7)/8)
	goto failure;
    
    } else
      goto failure;

    memcpy(return_value, value, 4);
    memcpy(return_mask, mask, 4);
    return true;
    
  } while (0);

 failure:
  return bad_ip_prefix(str, return_value, return_mask, allow_bare_address CP_PASS_CONTEXT);
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
cp_ip_address_list(const String &str, IPAddressList *l
		   CP_CONTEXT_ARG)
{
  Vector<String> words;
  cp_spacevec(str, words);
  StringAccum sa;
  IPAddress ip;
  for (int i = 0; i < words.size(); i++) {
    if (!cp_ip_address(words[i], &ip  CP_PASS_CONTEXT))
      return false;
    if (char *x = sa.extend(4))
      *reinterpret_cast<uint32_t *>(x) = ip.addr();
    else {
      cp_errno = CPE_MEMORY;
      return false;
    }
  }
  l->assign(words.size(), reinterpret_cast<uint32_t *>(sa.take_bytes()));
  return true;
}


// PARSING IPv6 ADDRESSES

#ifdef HAVE_IP6

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
    } else if (d && pos < len - 1 && s[pos] == ':' && isxdigit((unsigned char) s[pos+1]))
      pos++;
    if (pos >= len || !isxdigit((unsigned char) s[pos]))
      break;
    unsigned part = 0;
    last_part_pos = pos;
    for (; pos < len && isxdigit((unsigned char) s[pos]) && part <= 0xFFFF; pos++)
      part = (part<<4) + xvalue((unsigned char) s[pos]);
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
    relevant_bits = IP6Address(mask).mask_to_prefix_len();
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

#endif /* HAVE_IP6 */


bool
cp_ethernet_address(const String &str, unsigned char *return_value
		    CP_CONTEXT_ARG)
{
  int i = 0;
  const unsigned char* s = reinterpret_cast<const unsigned char*>(str.data());
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


bool
cp_tcpudp_port(const String &str, int ip_p, uint16_t *return_value
	       CP_CONTEXT_ARG)
{
    uint32_t value;
    assert(ip_p > 0 && ip_p < 256);
#ifndef CLICK_TOOL
    if (!NameInfo::query_int(NameInfo::T_IP_PORT + ip_p, context, str, &value))
	return false;
#else
    (void) ip_p;
    if (!cp_unsigned(str, &value))
	return false;
#endif
    if (value <= 0xFFFF) {
	*return_value = value;
	return true;
    } else {
	cp_errno = CPE_OVERFLOW;
	return false;
    }
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
cp_handler_name(const String& str,
		Element** result_element, String* result_hname,
		Element* context, ErrorHandler* errh)
{
  if (!errh)
    errh = ErrorHandler::silent_handler();
  
  String text;
  if (!cp_string(str, &text) || !text) {
    errh->error("bad handler name format");
    return false;
  }

  const char *leftmost_dot = find(text, '.');
  if (leftmost_dot == text.end() || leftmost_dot == text.begin()) {
    *result_element = context->router()->root_element();
    *result_hname = (leftmost_dot == text.begin() ? text.substring(text.begin() + 1, text.end()) : text);
    return true;
  } else if (leftmost_dot == text.end() - 1) {
    errh->error("empty handler name");
    return false;
  }

  Element *e = context->router()->find(text.substring(text.begin(), leftmost_dot), context, errh);
  if (!e)
    return false;

  *result_element = e;
  *result_hname = text.substring(leftmost_dot + 1, text.end());
  return true;
}

bool
cp_handler(const String &str, int flags,
	   Element** result_element, const Handler** result_h,
	   Element* context, ErrorHandler* errh)
{
  HandlerCall hc(str);
  if (hc.initialize(flags, context, errh) < 0)
    return false;
  else {
    *result_element = hc.element();
    *result_h = hc.handler();
    return true;
  }
}
#endif

#ifdef HAVE_IPSEC
bool
cp_des_cblock(const String &str, unsigned char *return_value)
{
  int i = 0;
  const unsigned char *s = reinterpret_cast<const unsigned char*>(str.data());
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

#ifdef CLICK_USERLEVEL
bool
cp_filename(const String &str, String *return_value)
{
  String fn;
  if (!cp_string(str, &fn) || !fn)
    return false;

  // expand home directory substitutions
  if (fn[0] == '~') {
    if (fn.length() == 1 || fn[1] == '/') {
      const char *home = getenv("HOME");
      if (home)
	fn = String(home) + fn.substring(1);
    } else {
      int off = 1;
      while (off < fn.length() && fn[off] != '/')
	off++;
      String username = fn.substring(1, off - 1);
      struct passwd *pwd = getpwnam(username.c_str());
      if (pwd && pwd->pw_dir)
	fn = String(pwd->pw_dir) + fn.substring(off);
    }
  }

  // replace double slashes with single slashes
  int len = fn.length();
  for (int i = 0; i < len - 1; i++)
    if (fn[i] == '/' && fn[i+1] == '/') {
      fn = fn.substring(0, i) + fn.substring(i + 1);
      i--;
      len--;
    }

  // return
  *return_value = fn;
  return true;
}
#endif


//
// CP_VA_PARSE AND FRIENDS
//

// parse commands; those which must be recognized inside a keyword section
// must begin with "\377"

const CpVaParseCmd
  cpEnd			= 0,
  cpOptional		= "OPTIONAL",
  cpKeywords		= "\377KEYWORDS",
  cpConfirmKeywords	= "\377CONFIRM_KEYWORDS",
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
  cpNamedInteger	= "named_int",
  cpInteger64		= "long_long",
  cpUnsigned64		= "u_long_long",
  cpFileOffset		= "off_t",
  cpReal2		= "real2",
  cpUnsignedReal2	= "u_real2",
  cpReal10		= "real10",
  cpUnsignedReal10	= "u_real10",
  cpDouble		= "double",
  cpSeconds		= "sec",
  cpSecondsAsMilli	= "msec",
  cpSecondsAsMicro	= "usec",
  cpTimeval		= "timeval",
  cpTimestamp		= "timestamp",
  cpInterval		= "interval",
  cpBandwidth		= "bandwidth_Bps",
  cpIPAddress		= "ip_addr",
  cpIPPrefix		= "ip_prefix",
  cpIPAddressOrPrefix	= "ip_addr_or_prefix",
  cpIPAddressList	= "ip_addr_list",
  cpEthernetAddress	= "ether_addr",
  cpEtherAddress	= "ether_addr", // synonym
  cpTCPPort		= "tcp_port",
  cpUDPPort		= "udp_port",
  cpElement		= "element",
  cpHandlerName		= "handler_name",
  cpHandler		= "handler",
  cpReadHandlerCall	= "read_handler_call",
  cpWriteHandlerCall	= "write_handler_call",
  cpIP6Address		= "ip6_addr",
  cpIP6Prefix		= "ip6_prefix",
  cpIP6AddressOrPrefix	= "ip6_addr_or_prefix",
  cpDesCblock		= "des_cblock",
  cpFilename		= "filename";

enum {
  cpiEnd = 0,
  cpiOptional,
  cpiKeywords,
  cpiConfirmKeywords,
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
  cpiNamedInteger,
  cpiInteger64,
  cpiUnsigned64,
  cpiFileOffset,
  cpiReal2,
  cpiUnsignedReal2,
  cpiReal10,
  cpiUnsignedReal10,
  cpiDouble,
  cpiSeconds,
  cpiSecondsAsMilli,
  cpiSecondsAsMicro,
  cpiTimeval,
  cpiInterval,
  cpiBandwidth,
  cpiIPAddress,
  cpiIPPrefix,
  cpiIPAddressOrPrefix,
  cpiIPAddressList,
  cpiEthernetAddress,
  cpiTCPPort,
  cpiUDPPort,
  cpiElement,
  cpiHandlerName,
  cpiReadHandlerCall,
  cpiWriteHandlerCall,
  cpiIP6Address,
  cpiIP6Prefix,
  cpiIP6AddressOrPrefix,
  cpiDesCblock,
  cpiFilename
};

#define NARGTYPE_HASH 128
static cp_argtype *argtype_hash[NARGTYPE_HASH];

static inline int
argtype_bucket(const char *command)
{
  const unsigned char *s = (const unsigned char *)command;
  return (s[0] ? (s[0]%32 + strlen(command)*32) % NARGTYPE_HASH : 0);
}

static const cp_argtype *
cp_find_argtype(const char *command)
{
  cp_argtype *t = argtype_hash[argtype_bucket(command)];
  while (t && strcmp(t->name, command) != 0)
    t = t->next;
  return t;
}

static int
cp_register_argtype(const char *name, const char *desc, int flags,
		    cp_parsefunc parse, cp_storefunc store, int internal,
		    void *user_data = 0)
{
    if (cp_argtype *t = const_cast<cp_argtype *>(cp_find_argtype(name))) {
	t->use_count++;
	if (strcmp(desc, t->description) != 0
	    || flags != t->flags
	    || parse != t->parse
	    || store != t->store
	    || internal != t->internal)
	    return -EEXIST;
	else
	    return t->use_count - 1;
    }
  
    if (cp_argtype *t = new cp_argtype) {
	t->name = name;
	t->parse = parse;
	t->store = store;
	t->user_data = user_data;
	t->flags = flags;
	t->description = desc;
	t->internal = internal;
	t->use_count = 1;
	int bucket = argtype_bucket(name);
	t->next = argtype_hash[bucket];
	argtype_hash[bucket] = t;
	return 0;
    } else
	return -ENOMEM;
}

int
cp_register_argtype(const char *name, const char *desc, int flags,
		    cp_parsefunc parse, cp_storefunc store, void *user_data)
{
    return cp_register_argtype(name, desc, flags, parse, store, -1, user_data);
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
   handle_int32_t:
    underflower = -0x80000000;
    overflower = 0x7FFFFFFF;
    goto handle_signed;

   case cpiUnsigned:
    overflower = 0xFFFFFFFFU;
    goto handle_unsigned;

    case cpiNamedInteger:
#ifndef CLICK_TOOL
      if (NameInfo::query(v->extra, context, arg, &v->v.i, 4))
	  break;
#endif
      goto handle_int32_t;

   handle_signed:
    if (!cp_integer(arg, &v->v.i))
      errh->error("%s takes %s (%s)", argname, argtype->description, desc);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("%s (%s) too large; max %d", argname, desc, v->v.i);
    else if (v->v.i < underflower)
      errh->error("%s (%s) must be >= %d", argname, desc, underflower);
    else if (v->v.i > (int)overflower)
      errh->error("%s (%s) must be <= %u", argname, desc, overflower);
    break;

   handle_unsigned:
    if (!cp_unsigned(arg, &v->v.u))
      errh->error("%s takes %s (%s)", argname, argtype->description, desc);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("%s (%s) too large; max %u", argname, desc, v->v.u);
    else if (v->v.u > overflower)
      errh->error("%s (%s) must be <= %u", argname, desc, overflower);
    break;

#ifdef HAVE_INT64_TYPES
   case cpiInteger64:
    if (!cp_integer(arg, &v->v.i64))
      errh->error("%s takes %s (%s)", argname, argtype->description, desc);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("%s (%s) too large; max %^64d", argname, desc, v->v.i64);
    break;

   case cpiUnsigned64:
    if (!cp_unsigned(arg, &v->v.u64))
      errh->error("%s takes %s (%s)", argname, argtype->description, desc);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("%s (%s) too large; max %^64u", argname, desc, v->v.u64);
    break;
#endif

#ifdef CLICK_USERLEVEL
   case cpiFileOffset:
    if (!cp_file_offset(arg, (off_t *) &v->v))
      errh->error("%s takes %s (%s)", argname, argtype->description, desc);
    break;
#endif

   case cpiReal10:
    if (!cp_real10(arg, v->extra, &v->v.i))
      errh->error("%s takes real (%s)", argname, desc);
    else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_real10(v->v.i, v->extra);
      errh->error("%s (%s) too large; max %s", argname, desc, m.c_str());
    }
    break;

   case cpiUnsignedReal10:
    if (!cp_unsigned_real10(arg, v->extra, &v->v.u))
      errh->error("%s takes unsigned real (%s)", argname, desc);
    else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_real10(v->v.u, v->extra);
      errh->error("%s (%s) too large; max %s", argname, desc, m.c_str());
    }
    break;

#ifdef HAVE_FLOAT_TYPES
   case cpiDouble:
    if (!cp_double(arg, &v->v.d))
      errh->error("%s takes %s (%s)", argname, argtype->description, desc);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("%s (%s) out of range; limit %g", argname, desc, v->v.d);
    break;
#endif

   case cpiSeconds:
    if (!cp_seconds_as(0, arg, &v->v.u))
      errh->error("%s takes time in seconds (%s)", argname, desc);
    else if (cp_errno == CPE_OVERFLOW)
      errh->error("%s (%s) too large; max %u", argname, desc, v->v.u);
    break;

   case cpiSecondsAsMilli:
    if (!cp_seconds_as(3, arg, &v->v.u))
      errh->error("%s takes time in seconds (%s)", argname, desc);
    else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_milliseconds(v->v.u);
      errh->error("%s (%s) too large; max %s", argname, desc, m.c_str());
    }
    break;

   case cpiSecondsAsMicro:
    if (!cp_seconds_as(6, arg, &v->v.u))
      errh->error("%s takes time in seconds (%s)", argname, desc);
    else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_microseconds(v->v.u);
      errh->error("%s (%s) too large; max %s", argname, desc, m.c_str());
    }
    break;

   case cpiTimeval:
   case cpiInterval: {
     struct timeval tv;
     if (!cp_time(arg, &tv)) {
       if (cp_errno == CPE_NEGATIVE)
	 errh->error("%s (%s) must be >= 0", argname, desc);
       else
	 errh->error("%s takes %s (%s)", argname, argtype->description, desc);
     } else if (cp_errno == CPE_OVERFLOW)
       errh->error("%s (%s) too large", argname, desc);
     else {
       v->v.i = tv.tv_sec;
       v->v2.i = tv.tv_usec;
     }
     break;
   }

   case cpiBandwidth:
    if (!cp_bandwidth(arg, &v->v.u))
      errh->error("%s takes bandwidth (%s)", argname, desc);
    else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_bandwidth(v->v.u);
      errh->error("%s (%s) too large; max %s", argname, desc, m.c_str());
    } else if (cp_errno == CPE_NOUNITS)
      errh->warning("no units on bandwidth %s (%s), assuming Bps", argname, desc);
    break;

   case cpiReal2:
    if (!cp_real2(arg, v->extra, &v->v.i)) {
      // CPE_INVALID would indicate a bad 'v->extra'
      errh->error("%s takes real (%s)", argname, desc);
    } else if (cp_errno == CPE_OVERFLOW) {
      String m = cp_unparse_real2(v->v.i, v->extra);
      errh->error("%s (%s) too large; max %s", argname, desc, m.c_str());
    }
    break;

   case cpiUnsignedReal2:
    if (!cp_unsigned_real2(arg, v->extra, &v->v.u)) {
      // CPE_INVALID would indicate a bad 'v->extra'
      errh->error("%s takes unsigned real (%s)", argname, desc);
    } else if (cp_errno == CPE_OVERFLOW) {
      String m  = cp_unparse_real2(v->v.u, v->extra);
      errh->error("%s (%s) too large; max %s", argname, desc, m.c_str());
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

   case cpiIPAddressList: {
     IPAddressList l;
     if (!cp_ip_address_list(arg, &l CP_PASS_CONTEXT))
       errh->error("%s takes set of IP addresses (%s)", argname, desc);
     break;
   }

#ifdef HAVE_IP6
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
#endif

   case cpiEthernetAddress:
    if (!cp_ethernet_address(arg, v->v.address CP_PASS_CONTEXT))
      errh->error("%s takes Ethernet address (%s)", argname, desc);
    break;

   case cpiTCPPort:
    if (!cp_tcpudp_port(arg, IP_PROTO_TCP, (uint16_t *) v->v.address CP_PASS_CONTEXT))
      errh->error("%s takes TCP port (%s)", argname, desc);
    break;

   case cpiUDPPort:
    if (!cp_tcpudp_port(arg, IP_PROTO_UDP, (uint16_t *) v->v.address CP_PASS_CONTEXT))
      errh->error("%s takes UDP port (%s)", argname, desc);
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
     cp_handler_name(arg, &v->v.element, &v->v2_string, context, &cerrh);
     break;
   }

   case cpiReadHandlerCall:
    underflower = HandlerCall::CHECK_READ | HandlerCall::ALLOW_PREINITIALIZE;
    goto handler_call;

   case cpiWriteHandlerCall:
    underflower = HandlerCall::CHECK_WRITE | HandlerCall::ALLOW_PREINITIALIZE;
    goto handler_call;

   handler_call: {
     ContextErrorHandler cerrh(errh, String(argname) + " (" + desc + "):");
     HandlerCall garbage(arg);
     garbage.initialize(underflower, context, &cerrh);
     break;
   }
#endif

#ifdef CLICK_USERLEVEL
   case cpiFilename:
    if (!cp_filename(arg, &v->v_string))
      errh->error("%s takes filename (%s)", argname, desc);
    break;
#endif
    
  }
}

static void
default_storefunc(cp_value *v  CP_CONTEXT_ARG)
{
  int helper;
  const cp_argtype *argtype = v->argtype;
  
  if (v->store_confirm)
    *v->store_confirm = true;
  
  switch (argtype->internal) {
    
   case cpiBool: {
     bool *bstore = (bool *)v->store;
     *bstore = v->v.b;
     break;
   }
   
   case cpiByte: {
     uint8_t *ucstore = (uint8_t *)v->store;
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
    
   case cpiTCPPort:
   case cpiUDPPort: {
     uint16_t *u16store = (uint16_t *)v->store;
     *u16store = *((uint16_t *)v->v.address);
     break;
   }
   
   case cpiInteger:
   case cpiNamedInteger:
   case cpiReal2:
   case cpiReal10:
   case cpiSeconds:
   case cpiSecondsAsMilli:
   case cpiSecondsAsMicro:
   case cpiBandwidth: {
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

#ifdef HAVE_INT64_TYPES
   case cpiInteger64: {
     int64_t *llstore = (int64_t *)v->store;
     *llstore = v->v.i64;
     break;
   }

   case cpiUnsigned64: {
     uint64_t *ullstore = (uint64_t *)v->store;
     *ullstore = v->v.u64;
     break;
   }
#endif

#ifdef CLICK_USERLEVEL
   case cpiFileOffset: {
     off_t *offstore = (off_t *)v->store;
     *offstore = *((off_t *)&v->v);
     break;
   }
#endif

#ifdef HAVE_FLOAT_TYPES
   case cpiDouble: {
     double *dstore = (double *)v->store;
     *dstore = v->v.d;
     break;
   }
#endif

   case cpiTimeval:
   case cpiInterval: {
     struct timeval *tvstore = (struct timeval *)v->store;
     tvstore->tv_sec = v->v.i;
     tvstore->tv_usec = v->v2.i;
     break;
   }

   case cpiArgument:
   case cpiString:
   case cpiWord:
   case cpiKeyword:
   case cpiFilename: {
     String *sstore = (String *)v->store;
     *sstore = v->v_string;
     break;
   }

   case cpiArguments: {
     Vector<String> *vstore = (Vector<String> *)v->store;
     uint32_t pos = 0;
     const char *len_str = v->v2_string.data();
     for (int len_pos = 0; len_pos < v->v2_string.length(); len_pos += 4) {
       uint32_t pos2 = *((const uint32_t *)(len_str + len_pos));
       vstore->push_back(v->v_string.substring(pos, pos2 - pos));
       pos = pos2;
     }
     vstore->push_back(v->v_string.substring(pos));
     break;
   }

   case cpiIPAddress:
    helper = 4;
    goto address;

#ifdef HAVE_IP6
   case cpiIP6Address:
    helper = 16;
    goto address;
#endif

   case cpiEthernetAddress:
    helper = 6;
    goto address;

#ifdef HAVE_IPSEC
   case cpiDesCblock:
    helper = 8;
    goto address;
#endif
   
   address: {
     unsigned char *addrstore = (unsigned char *)v->store;
     memcpy(addrstore, v->v.address, helper);
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

#ifdef HAVE_IP6
   case cpiIP6Prefix:
   case cpiIP6AddressOrPrefix: {
     unsigned char *addrstore = (unsigned char *)v->store;
     memcpy(addrstore, v->v.address, 16);
     unsigned char *maskstore = (unsigned char *)v->store2;
     memcpy(maskstore, v->v2.address, 16);
     break;
   }
#endif

   case cpiIPAddressList: {
     // oog... parse set into stored set only when we know there are no errors
     IPAddressList *liststore = (IPAddressList *)v->store;
     cp_ip_address_list(v->v_string, liststore  CP_PASS_CONTEXT);
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

   case cpiReadHandlerCall:
    helper = HandlerCall::CHECK_READ | HandlerCall::ALLOW_PREINITIALIZE;
    goto handler_call;

   case cpiWriteHandlerCall:
    helper = HandlerCall::CHECK_WRITE | HandlerCall::ALLOW_PREINITIALIZE;
    goto handler_call;

   handler_call:
    HandlerCall::reset(*(HandlerCall**)v->store, v->v_string, helper, context, (ErrorHandler*)0);
    break;
#endif

   default:
    // no argument provided
    break;
    
  }
}


static void
stringlist_parsefunc(cp_value *v, const String &arg,
		     ErrorHandler *errh, const char *argname  CP_CONTEXT_ARG)
{
    const char *desc = v->description;
    const cp_argtype *argtype = v->argtype;
#ifndef CLICK_TOOL
    (void) context;
#endif

    if (HashMap<String, int> *m = reinterpret_cast<HashMap<String, int> *>(argtype->user_data)) {
	String word;
	if (cp_word(arg, &word))
	    if (int *valp = m->findp(word)) {
		v->v.i = *valp;
		return;
	    }
    }

    if (argtype->flags & cpArgAllowNumbers) {
	if (!cp_integer(arg, &v->v.i))
	    errh->error("%s takes %s (%s)", argname, argtype->description, desc);
	else if (cp_errno == CPE_OVERFLOW)
	    errh->error("%s (%s) too large; max %d", argname, desc, v->v.i);
    } else
	errh->error("%s takes %s (%s)", argname, argtype->description, desc);
}

int
cp_register_stringlist_argtype(const char *name, const char *desc, int flags)
{
    return cp_register_argtype(name, desc, flags, stringlist_parsefunc, default_storefunc, cpiInteger);
}

int
cp_extend_stringlist_argtype(const char *name, ...)
{
    cp_argtype *t = const_cast<cp_argtype *>(cp_find_argtype(name));
    if (!t || t->parse != stringlist_parsefunc)
	return -ENOENT;
    HashMap<String, int> *m = reinterpret_cast<HashMap<String, int> *>(t->user_data);
    if (!m)
	t->user_data = m = new HashMap<String, int>();
    if (!m)
	return -ENOMEM;
  
    va_list val;
    va_start(val, name);
    const char *s;
    int retval = 0;
    while ((s = va_arg(val, const char *))) {
	int value = va_arg(val, int);
	if (cp_is_word(s))
	    m->insert(String(s), value);
	else
	    retval = -1;
    }
    va_end(val);
    return retval;
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
	    if (trav->parse == stringlist_parsefunc)
		delete reinterpret_cast<HashMap<String, int> *>(trav->user_data);
	    *prev = trav->next;
	    delete trav;
	}
    }
}


#define CP_VALUES_SIZE 80
static cp_value *cp_values;
static Vector<int> *cp_parameter_used;

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
    return t && t->internal == cpiArguments;
}

static int
handle_special_argtype_for_keyword(cp_value *val, const String &rest)
{
  if (val->argtype->internal == cpiArguments) {
    if (val->v.i > 0) {
      uint32_t l = val->v_string.length();
      val->v2_string += String((const char *)&l, 4);
      val->v_string += rest;
    } else {
      val->v.i = 1;
      val->v_string = rest;
      val->v2_string = String();
    }
    return kwSuccess;
  } else {
    assert(0);
    return kwUnkKeyword;
  }
}


namespace {

struct CpVaHelper {

  CpVaHelper(struct cp_value *, int, bool keywords_only);

  int develop_values(va_list val, ErrorHandler *errh);

  int assign_keyword_argument(const String &);
  void add_keyword_error(StringAccum &, int err, const String &arg, const char *argname, int argno);
  int finish_keyword_error(const char *format, const char *bad_keywords, ErrorHandler *errh);

  int assign_arguments(const Vector<String> &args, const char *argname, ErrorHandler *errh);
  
  int parse_arguments(const char *argname  CP_CONTEXT_ARG, ErrorHandler *errh);

  bool keywords_only;
  int nvalues;
  int nrequired;
  int npositional;
  bool ignore_rest;

  struct cp_value *cp_values;
  int cp_values_size;
  
};

CpVaHelper::CpVaHelper(struct cp_value *cp_values_, int cp_values_size_,
		       bool keywords_only_)
  : keywords_only(keywords_only_), nvalues(0),
    nrequired(keywords_only ? 0 : -1), npositional(keywords_only ? 0 : -1),
    ignore_rest(keywords_only),
    cp_values(cp_values_), cp_values_size(cp_values_size_)
{
}

int
CpVaHelper::develop_values(va_list val, ErrorHandler *errh)
{
  if (!cp_values || !cp_parameter_used)
    return errh->error("out of memory in cp_va_parse");

  bool confirm_keywords = false;
  bool mandatory_keywords = false;

  while (1) {
    
    if (nvalues == cp_values_size - 1)
      // no more space to store information about the arguments; break
      return errh->error("too many arguments to cp_va_parse!");

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
    const cp_argtype *argtype = cp_find_argtype(command_name);
    if (!argtype)
      return errh->error("unknown argument type '%s'!", command_name);
    
    // check for special commands
    if (argtype->internal == cpiOptional) {
      if (nrequired < 0)
	nrequired = nvalues;
      continue;
    } else if (argtype->internal == cpiKeywords
	       || argtype->internal == cpiConfirmKeywords
	       || argtype->internal == cpiMandatoryKeywords) {
      if (nrequired < 0)
	nrequired = nvalues;
      if (npositional < 0)
	npositional = nvalues;
      confirm_keywords = (argtype->internal == cpiConfirmKeywords);
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
    if (argtype->flags & cpArgExtraInt)
      v->extra = va_arg(val, int);
    v->store_confirm = (confirm_keywords ? va_arg(val, bool *) : 0);
    v->store = va_arg(val, void *);
    if (argtype->flags & cpArgStore2)
      v->store2 = va_arg(val, void *);
    v->v.i = (mandatory_keywords ? -1 : 0);
    nvalues++;
  }

 done:
  
  if (nrequired < 0)
    nrequired = nvalues;
  if (npositional < 0)
    npositional = nvalues;
  
  return 0;
}

  
int
CpVaHelper::assign_keyword_argument(const String &arg)
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

void
CpVaHelper::add_keyword_error(StringAccum &sa, int err, const String &arg,
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

int
CpVaHelper::finish_keyword_error(const char *format, const char *bad_keywords, ErrorHandler *errh)
{
    StringAccum keywords_sa;
    for (int i = npositional; i < nvalues; i++) {
	if (i > npositional)
	    keywords_sa << ", ";
	keywords_sa << cp_values[i].keyword;
    }
    errh->error(format, bad_keywords, keywords_sa.c_str());
    return -EINVAL;
}

int
CpVaHelper::assign_arguments(const Vector<String> &args, const char *argname, ErrorHandler *errh)
{
  if (!cp_values || !cp_parameter_used)
    return errh->error("out of memory in cp_va_parse");

  //
  // Assign parameters from 'conf' to argument slots.
  //
  
  int npositional_supplied = 0;
  StringAccum keyword_error_sa;
  bool any_keywords = false;
  cp_parameter_used->assign(args.size(), 0);
  
  for (int i = 0; i < args.size(); i++) {
    // check for keyword if past mandatory positional arguments
    if (npositional_supplied >= nrequired) {
      int result = assign_keyword_argument(args[i]);
      if (result == kwSuccess) {
	(*cp_parameter_used)[i] = 1;
	any_keywords = true;
      } else if (!any_keywords)
	goto positional_argument; // optional argument, or too many arguments
      else if (ignore_rest)
	/* no error */;
      else
	add_keyword_error(keyword_error_sa, result, args[i], argname, i);
      continue;
    }

    // otherwise, assign positional argument
   positional_argument:
    if (npositional_supplied < npositional) {
      (*cp_parameter_used)[i] = 1;
      cp_values[npositional_supplied].v_string = args[i];
    }
    npositional_supplied++;
  }
  
  // report keyword argument errors
  if (keyword_error_sa.length() && !keywords_only)
    return finish_keyword_error("bad keyword(s) %s\n(valid keywords are %s)", keyword_error_sa.c_str(), errh);
  
  // report missing mandatory keywords
  for (int i = npositional; i < nvalues; i++)
    if (cp_values[i].v.i < 0) {
      if (keyword_error_sa.length())
	keyword_error_sa << ", ";
      keyword_error_sa << cp_values[i].keyword;
    }
  if (keyword_error_sa.length())
    return errh->error("missing mandatory keyword(s) %s", keyword_error_sa.c_str());
  
  // if wrong number of arguments, print signature
  if (npositional_supplied < nrequired
      || (npositional_supplied > npositional && !ignore_rest)) {
    StringAccum signature;
    for (int i = 0; i < npositional; i++) {
      if (i == nrequired)
	signature << (nrequired > 0 ? " [" : "[");
      if (i)
	signature << ", ";
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
	signature << ", ";
      signature << "[keywords]";
    }

    const char *whoops = (npositional_supplied > npositional ? "too many" : "too few");
    if (signature.length())
      errh->error("%s %ss; expected '%s'", whoops, argname, signature.c_str());
    else
      errh->error("expected empty %s list", argname);
    return -EINVAL;
  }

  // clear 'argtype' on unused arguments
  for (int i = npositional_supplied; i < npositional; i++)
    cp_values[i].argtype = 0;
  for (int i = npositional; i < nvalues; i++)
    if (cp_values[i].v.i <= 0)
      cp_values[i].argtype = 0;

  return 0;
}

int
CpVaHelper::parse_arguments(const char *argname,
#ifndef CLICK_TOOL
			    Element *context,
#endif
			    ErrorHandler *errh)
{
  int nerrors_in = errh->nerrors();
  
  // parse arguments
  char argname_buf[128];
  int argname_offset;
  argname_offset = sprintf(argname_buf, "%s ", argname);

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
      v->argtype->parse(v, v->v_string, errh, sa.c_str()  CP_PASS_CONTEXT);
    }
  }

  // check for failure
  if (errh->nerrors() != nerrors_in)
    return -EINVAL;
  
  // if success, actually set the values
  int nset = 0;
  for (int i = 0; i < nvalues; i++)
    if (const cp_argtype *t = cp_values[i].argtype) {
      t->store(&cp_values[i]  CP_PASS_CONTEXT);
      nset++;
    }
  return nset;
}

}


int
cp_va_parse(const Vector<String> &argv,
#ifndef CLICK_TOOL
	    Element *context,
#endif
	    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(argv, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
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
  Vector<String> argv;
  cp_argvec(confstr, argv);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(argv, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

int
cp_va_space_parse(const String &arg,
#ifndef CLICK_TOOL
		  Element *context,
#endif
		  ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> argv;
  cp_spacevec(arg, argv);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(argv, "word", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("word"  CP_PASS_CONTEXT, errh);
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
  Vector<String> argv;
  argv.push_back(arg);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, true);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(argv, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

int
cp_va_parse_remove_keywords(Vector<String> &argv, int first,
#ifndef CLICK_TOOL
			    Element *context,
#endif
			    ErrorHandler *errh, ...)
{
  Vector<String> conf2, *confp = &argv;
  if (first > 0) {
    for (int i = first; i < argv.size(); i++)
      conf2.push_back(argv[i]);
    confp = &conf2;
  }
  
  va_list val;
  va_start(val, errh);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, true);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(*confp, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);

  // remove keywords that were used
  if (retval >= 0) {
    int delta = 0;
    for (int i = first; i < argv.size(); i++)
      if ((*cp_parameter_used)[i - first])
	delta++;
      else if (delta)
	argv[i - delta] = argv[i];
    argv.resize(argv.size() - delta);
  }
  
  return retval;
}

int
cp_assign_arguments(const Vector<String> &argv, const Vector<String> &keys, Vector<String> *values)
{
  // check common case
  if (keys.size() == 0 || !keys.back()) {
    if (argv.size() != keys.size())
      return -EINVAL;
    else {
      if (values)
	*values = argv;
      return 0;
    }
  }

  if (!cp_values || !cp_parameter_used || keys.size() > CP_VALUES_SIZE)
    return -ENOMEM; /*errh->error("out of memory in cp_va_parse");*/

  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  if (keys.size() && keys.back() == "__REST__") {
    cpva.ignore_rest = true;
    cpva.nvalues = keys.size() - 1;
  } else {
    cpva.ignore_rest = false;
    cpva.nvalues = keys.size();
  }
  
  int arg;
  for (arg = 0; arg < cpva.nvalues && keys[arg] == ""; arg++) {
    cp_values[arg].argtype = 0;
    cp_values[arg].keyword = 0;
  }
  cpva.nrequired = cpva.npositional = arg;
  for (; arg < cpva.nvalues; arg++) {
    cp_values[arg].argtype = 0;
    cp_values[arg].keyword = keys[arg].c_str();
    cp_values[arg].v.i = -1;	// mandatory keyword
  }

  int retval = cpva.assign_arguments(argv, "argument", ErrorHandler::silent_handler());
  if (retval >= 0 && values) {
    if (cpva.ignore_rest) {
      // collect '__REST__' argument
      StringAccum sa;
      bool any = false;
      for (int i = 0; i < argv.size(); i++)
	if (!(*cp_parameter_used)[i]) {
	  sa << (any ? ", " : "") << argv[i];
	  any = true;
	}
      cp_values[cpva.nvalues].v_string = sa.take_string();
    }
    values->resize(keys.size());
    for (arg = 0; arg < keys.size(); arg++)
      (*values)[arg] = cp_values[arg].v_string;
  }
  return retval;
}


// UNPARSING

String
cp_unparse_bool(bool b)
{
    if (b)
	return String::stable_string("true", 4);
    else
	return String::stable_string("false", 5);
}

#ifdef HAVE_INT64_TYPES

String
cp_unparse_unsigned64(uint64_t q, int base, bool uppercase)
{
  // Unparse a uint64_t. Linux kernel sprintf can't handle %lld, so we provide
  // our own function.
  
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
      uint64_t k = (q >> 4) + (q >> 5) + (q >> 8) + (q >> 9)
	+ (q >> 12) + (q >> 13) + (q >> 16) + (q >> 17);
      uint64_t m;
      
      // increase k until it exactly equals floor(q/10). on exit, m is the
      // remainder: m < 10 and q == 10*k + m.
      while (1) {
	// d = 10*k
	uint64_t d = (k << 3) + (k << 1);
	m = q - d;
	if (m < 10) break;
	
	// delta = Approx[m/10] -- know that delta <= m/10
	uint64_t delta = (m >> 4) + (m >> 5) + (m >> 8) + (m >> 9);
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
cp_unparse_integer64(int64_t q, int base, bool uppercase)
{
  if (q < 0)
    return "-" + cp_unparse_unsigned64(-q, base, uppercase);
  else
    return cp_unparse_unsigned64(q, base, uppercase);
}

#endif

String
cp_unparse_real2(uint32_t real, int frac_bits)
{
  // adopted from Knuth's TeX function print_scaled, hope it is correct in
  // this context but not sure.
  // Works well with cp_real2 above; as an invariant,
  // unsigned x, y;
  // cp_real2(cp_unparse_real2(x, FRAC_BITS), FRAC_BITS, &y) == true && x == y
  
  StringAccum sa;
  assert(frac_bits <= CP_REAL2_MAX_FRAC_BITS);

  uint32_t int_part = real >> frac_bits;
  sa << int_part;

  uint32_t one = 1 << frac_bits;
  real &= one - 1;
  if (!real) return sa.take_string();
  
  sa << ".";
  real = (10 * real) + 5;
  unsigned allowable_inaccuracy = 10;

  unsigned inaccuracy_rounder = 5;
  while (inaccuracy_rounder < (one >> 1))
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

#undef TEST_REAL2
#ifdef TEST_REAL2
void
test_unparse_real2()
{
#define TEST(s, frac_bits, result) { String q = (#s); uint32_t r; if (!cp_unsigned_real2(q, (frac_bits), &r)) fprintf(stderr, "FAIL: %s unparsable\n", q.c_str()); else { String qq = cp_unparse_real2(r, (frac_bits)); fprintf(stderr, "%s: %s %d/%d %s\n", (qq == (result) ? "PASS" : "FAIL"), q.c_str(), r, (frac_bits), qq.c_str()); }}
  TEST(0.418, 8, "0.418");
  TEST(0.417, 8, "0.418");
  TEST(0.416, 8, "0.414");
  TEST(0.42, 8, "0.42");
  TEST(0.3, 16, "0.3");
  TEST(0.49, 16, "0.49");
  TEST(0.499, 16, "0.499");
  TEST(0.4999, 16, "0.4999");
  TEST(0.49999, 16, "0.49998");
  TEST(0.499999, 16, "0.5");
  TEST(0.49998, 16, "0.49998");
  TEST(0.999999, 16, "1");
#undef TEST
}
#endif

String
cp_unparse_real2(int32_t real, int frac_bits)
{
  if (real < 0)
    return "-" + cp_unparse_real2(static_cast<uint32_t>(-real), frac_bits);
  else
    return cp_unparse_real2(static_cast<uint32_t>(real), frac_bits);
}

#ifdef HAVE_INT64_TYPES
String
cp_unparse_real2(uint64_t real, int frac_bits)
{
  assert(frac_bits <= CP_REAL2_MAX_FRAC_BITS);
  String int_part = cp_unparse_unsigned64(real >> frac_bits, 10, false);
  String frac_part = cp_unparse_real2((uint32_t)(real & ((1 << frac_bits) - 1)), frac_bits);
  return int_part + frac_part.substring(1);
}

String
cp_unparse_real2(int64_t real, int frac_bits)
{
  if (real < 0)
    return "-" + cp_unparse_real2(static_cast<uint64_t>(-real), frac_bits);
  else
    return cp_unparse_real2(static_cast<uint64_t>(real), frac_bits);
}
#endif

String
cp_unparse_real10(uint32_t real, int frac_digits)
{
  assert(frac_digits >= 0 && frac_digits <= 9);
  uint32_t one = exp10val[frac_digits];
  uint32_t int_part = real / one;
  uint32_t frac_part = real - (int_part * one);

  if (frac_part == 0)
    return String(int_part);

  StringAccum sa(30);
  sa << int_part << '.';
  if (char *x = sa.extend(frac_digits, 1))
    sprintf(x, "%0*d", frac_digits, frac_part);
  else				// out of memory
    return sa.take_string();

  // remove trailing zeros from fraction part
  while (sa.back() == '0')
    sa.pop_back();

  return sa.take_string();
}

String
cp_unparse_real10(int32_t real, int frac_digits)
{
    if (real < 0)
	return "-" + cp_unparse_real10(static_cast<uint32_t>(-real), frac_digits);
    else
	return cp_unparse_real10(static_cast<uint32_t>(real), frac_digits);
}

String
cp_unparse_milliseconds(uint32_t ms)
{
    if (ms && ms < 1000)
	return String(ms) + "ms";
    else
	return cp_unparse_real10(ms, 3) + "s";
}

String
cp_unparse_microseconds(uint32_t us)
{
    if (us && us < 1000)
	return String(us) + "us";
    else
	return cp_unparse_real10(us, 6) + "s";
}

String
cp_unparse_interval(const Timestamp& ts)
{
    if (ts.sec() == 0)
	return cp_unparse_microseconds(ts.usec());
    else {
	StringAccum sa;
	sa << ts << 's';
	return sa.take_string();
    }
}

String
cp_unparse_interval(const timeval& tv)
{
    return cp_unparse_interval(*(const Timestamp*) &tv);
}

String
cp_unparse_bandwidth(uint32_t bw)
{
    if (bw >= 0x20000000U)
	return cp_unparse_real10(bw, 6) + "MBps";
    else if (bw >= 125000000)
	return cp_unparse_real10(bw * 8, 9) + "Gbps";
    else if (bw >= 125000)
	return cp_unparse_real10(bw * 8, 6) + "Mbps";
    else
	return cp_unparse_real10(bw * 8, 3) + "kbps";
}


// initialization and cleanup

void
cp_va_static_initialize()
{
    assert(!cp_values);
  
    cp_register_argtype(cpOptional, "<optional arguments marker>", 0, default_parsefunc, default_storefunc, cpiOptional);
    cp_register_argtype(cpKeywords, "<keyword arguments marker>", 0, default_parsefunc, default_storefunc, cpiKeywords);
    cp_register_argtype(cpConfirmKeywords, "<confirmed keyword arguments marker>", 0, default_parsefunc, default_storefunc, cpiConfirmKeywords);
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
    cp_register_argtype(cpNamedInteger, "named int", cpArgExtraInt, default_parsefunc, default_storefunc, cpiNamedInteger);
#ifdef HAVE_INT64_TYPES
    cp_register_argtype(cpInteger64, "64-bit int", 0, default_parsefunc, default_storefunc, cpiInteger64);
    cp_register_argtype(cpUnsigned64, "64-bit unsigned", 0, default_parsefunc, default_storefunc, cpiUnsigned64);
#endif
#ifdef CLICK_USERLEVEL
    cp_register_argtype(cpFileOffset, "file offset", 0, default_parsefunc, default_storefunc, cpiFileOffset);
#endif
    cp_register_argtype(cpReal2, "real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiReal2);
    cp_register_argtype(cpUnsignedReal2, "unsigned real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiUnsignedReal2);
    cp_register_argtype(cpReal10, "real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiReal10);
    cp_register_argtype(cpUnsignedReal10, "unsigned real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiUnsignedReal10);
#ifdef HAVE_FLOAT_TYPES
    cp_register_argtype(cpDouble, "double", 0, default_parsefunc, default_storefunc, cpiDouble);
#endif
    cp_register_argtype(cpSeconds, "time in sec", 0, default_parsefunc, default_storefunc, cpiSeconds);
    cp_register_argtype(cpSecondsAsMilli, "time in sec (msec precision)", 0, default_parsefunc, default_storefunc, cpiSecondsAsMilli);
    cp_register_argtype(cpSecondsAsMicro, "time in sec (usec precision)", 0, default_parsefunc, default_storefunc, cpiSecondsAsMicro);
    cp_register_argtype(cpTimeval, "seconds since the epoch", 0, default_parsefunc, default_storefunc, cpiTimeval);
    cp_register_argtype(cpTimestamp, "seconds since the epoch", 0, default_parsefunc, default_storefunc, cpiTimeval);
    cp_register_argtype(cpInterval, "time in sec (usec precision)", 0, default_parsefunc, default_storefunc, cpiInterval);
    cp_register_argtype(cpBandwidth, "bandwidth", 0, default_parsefunc, default_storefunc, cpiBandwidth);
    cp_register_argtype(cpIPAddress, "IP address", 0, default_parsefunc, default_storefunc, cpiIPAddress);
    cp_register_argtype(cpIPPrefix, "IP address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIPPrefix);
    cp_register_argtype(cpIPAddressOrPrefix, "IP address or prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIPAddressOrPrefix);
    cp_register_argtype(cpIPAddressList, "list of IP addresses", 0, default_parsefunc, default_storefunc, cpiIPAddressList);
    cp_register_argtype(cpEthernetAddress, "Ethernet address", 0, default_parsefunc, default_storefunc, cpiEthernetAddress);
    cp_register_argtype(cpTCPPort, "TCP port", 0, default_parsefunc, default_storefunc, cpiTCPPort);
    cp_register_argtype(cpUDPPort, "UDP port", 0, default_parsefunc, default_storefunc, cpiUDPPort);
#ifndef CLICK_TOOL
    cp_register_argtype(cpElement, "element name", 0, default_parsefunc, default_storefunc, cpiElement);
    cp_register_argtype(cpHandlerName, "handler name", cpArgStore2, default_parsefunc, default_storefunc, cpiHandlerName);
    cp_register_argtype(cpReadHandlerCall, "read handler name", 0, default_parsefunc, default_storefunc, cpiReadHandlerCall);
    cp_register_argtype(cpWriteHandlerCall, "write handler name and value", 0, default_parsefunc, default_storefunc, cpiWriteHandlerCall);
#endif
#ifdef HAVE_IP6
    cp_register_argtype(cpIP6Address, "IPv6 address", 0, default_parsefunc, default_storefunc, cpiIP6Address);
    cp_register_argtype(cpIP6Prefix, "IPv6 address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6Prefix);
    cp_register_argtype(cpIP6AddressOrPrefix, "IPv6 address or prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6AddressOrPrefix);
#endif
#ifdef HAVE_IPSEC
    cp_register_argtype(cpDesCblock, "DES cipher block", 0, default_parsefunc, default_storefunc, cpiDesCblock);
#endif
#ifdef CLICK_USERLEVEL
    cp_register_argtype(cpFilename, "filename", 0, default_parsefunc, default_storefunc, cpiFilename);
#endif

    cp_values = new cp_value[CP_VALUES_SIZE];
    cp_parameter_used = new Vector<int>;

#ifdef TEST_REAL2
    test_unparse_real2();
#endif

#if CLICK_USERLEVEL && HAVE_IP6
    // force all IP6 library objects to be included
    extern int IP6FlowID_linker_trick;
    IP6FlowID_linker_trick++;
#endif
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
  delete cp_parameter_used;
  cp_values = 0;
  cp_parameter_used = 0;
}

CLICK_ENDDECLS
