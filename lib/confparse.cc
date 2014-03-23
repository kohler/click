// -*- c-basic-offset: 4; related-file-name: "../include/click/confparse.hh" -*-
/*
 * confparse.{cc,hh} -- configuration string parsing
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000-2001 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2004-2008 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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

#define CLICK_COMPILING_CONFPARSE_CC 1
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/hashtable.hh>
#if HAVE_IP6
# include <click/ip6address.hh>
# include <click/ip6flowid.hh>
#endif
#if !CLICK_TOOL
# include <click/router.hh>
# include <click/handlercall.hh>
# include <click/nameinfo.hh>
# include <click/standard/addressinfo.hh>
# include <click/packet_anno.hh>
# define CP_CONTEXT , const Element *context
# define CP_PASS_CONTEXT , context
#else
# include <click/timestamp.hh>
# if HAVE_NETDB_H
#  include <netdb.h>
# endif
# define CP_CONTEXT
# define CP_PASS_CONTEXT
#endif
#if CLICK_USERLEVEL || CLICK_TOOL
# include <pwd.h>
#endif
#if CLICK_BSDMODULE
# include <machine/stdarg.h>
#else
# include <stdarg.h>
#endif
CLICK_DECLS

int cp_errno;

/// @file confparse.hh
/// @brief Support for parsing configuration strings.
///
/// Defines functions and helpers for parsing configuration strings into
/// numbers, IP addresses, and other useful types.
///
/// <h3>cp_va_kparse Introduction</h3>
///
/// Most elements that take configuration strings parse them using the
/// cp_va_kparse() function and friends.  These functions take a variable
/// argument list describing the desired arguments and result slots.  They
/// parse the configuration, store the results in the slots, report any
/// errors, and return the number of arguments successfully assigned on
/// success or a negative value on failure.
///
/// @note Previous versions of Click used cp_va_parse() and friends instead of
/// cp_va_kparse().  A guide for transitioning from cp_va_parse() to
/// cp_va_kparse() is given in the documentation for cp_va_parse().
///
/// Here are some cp_va_kparse() examples.
///
/// @code
/// int MyElement::configure(Vector<String> &conf, ErrorHandler *errh) {
///     String data; uint32_t limit = 0; bool stop = false;
///     if (cp_va_kparse(conf, this, errh,
///                      "DATA", cpkP+cpkM, cpString, &data,
///                      "LIMIT", cpkP, cpUnsigned, &limit,
///                      "STOP", 0, cpBool, &stop,
///                      cpEnd) < 0)   // argument list always terminated by cpEnd
///         return -1;
///     ... }
/// @endcode
///
/// This element supports three arguments, <tt>DATA</tt> (a string),
/// <tt>LIMIT</tt> (an unsigned integer), and <tt>STOP</tt> (a boolean).  Here
/// are some example element definitions:
///
/// @code
/// MyElement(DATA "blah blah blah", LIMIT 10);
///             /* OK, sets data = "blah blah blah", limit = 10; leaves stop unchanged */
/// MyElement(LIMIT 10, DATA "blah blah blah");
///             /* OK, has the same effect */
/// MyElement(LIMIT 10);
///             /* error "missing mandatory DATA argument" */
///             /* (the cpkM flag marks an argument as mandatory) */
/// MyElement(DATA "blah blah blah");
///             /* OK, sets data = "blah blah blah" and leaves limit unchanged */
///             /* (LIMIT lacks the cpkM flag, so it can be left off) */
/// MyElement(DATA "blah blah blah", STOP true);
///             /* OK, sets data = "blah blah blah" and stop = true */
///             /* (LIMIT lacks the cpkM flag, so it can be left off) */
/// MyElement(DATA "blah blah blah", LIMIT 10, DATA "blah");
///             /* OK, sets data = "blah" (later arguments take precedence) */
/// MyElement(DATA "blah blah blah", LIMIT 10, BOGUS "bogus");
///             /* error "too many arguments" */
/// MyElement("blah blah blah", 10);
///             /* OK, same as MyElement(DATA "blah blah blah", LIMIT 10) */
///             /* (the cpkP flag allows positional arguments) */
/// MyElement("blah blah blah", 10, true);
///             /* error "too many arguments" */
///             /* (STOP lacks the cpkP flag and must be given by name) */
/// @endcode
///
/// <h3>cp_va_kparse Items</h3>
///
/// An item in a cp_va_kparse() argument list consists of:
///
/// <ol>
/// <li><strong>Argument name</strong> (type: const char *).  Example: <tt>"DATA"</tt>.</li>
/// <li><strong>Parse flags</strong> (type: int).  Zero or more of
/// #cpkP, #cpkM, and #cpkC.</li>
/// <li>If the parse flags contain #cpkC, then a <strong>confirmation
/// flag</strong> comes next (type: bool *).  This flag is set to true if an
/// argument successfully matched the item and false if not.</li>
/// <li><strong>Argument type</strong> (type: @ref CpVaParseCmd).  Defines the type
/// of argument read from the configuration string.  Example: ::cpString.</li>
/// <li>Optional <strong>parse parameters</strong> (determined by the
/// argument type).  For example, ::cpUnsignedReal2 takes a parse parameter
/// that defines how many bits of fraction are needed.</li>
/// <li><strong>Result storage</strong> (determined by the argument type).</li>
/// </ol>
///
/// This example uses more of these features.
///
/// @code
/// int MyElement2::configure(Vector<String> &conf, ErrorHandler *errh) {
///     bool p_given; uint32_t p = 0x10000; IPAddress addr, mask;
///     if (cp_va_kparse(conf, this, errh,
///                      "P", cpkC, &p_given, cpUnsignedReal2, 16, &p,
///                      "NETWORK", 0, cpIPPrefix, &addr, &mask,
///                      cpEnd) < 0)
///         return -1;
///     ... }
/// @endcode
///
/// This element supports two arguments, <tt>P</tt> (a fixed-point number with
/// 16 bits of fraction) and <tt>NETWORK</tt> (an IP prefix, defined by
/// address and mask).  Here are some example element definitions:
///
/// @code
/// MyElement2();
///             /* OK, since neither argument is mandatory; sets p_given = false */
/// MyElement2(P 0.5, PREFIX 10/8);
///             /* OK, sets p_given = true, p = 0x8000, addr = 10.0.0.0, and mask = 255.0.0.0 */
/// @endcode
///
/// <h3>cp_va_kparse Argument Types</h3>
///
/// cp_va_kparse() argument types are defined by @ref CpVaParseCmd constants.
/// For example, the ::cpInteger argument type parses a 32-bit signed integer.
/// See @ref CpVaParseCmd for more.  Elements may also define their own
/// argument types with cp_register_argtype().
///
/// <h3>Direct Parsing Functions</h3>
///
/// You may also call parsing functions directly if cp_va_kparse() doesn't
/// match your needs.  These functions have names like cp_bool(), cp_string(),
/// cp_integer(), cp_ip_address(), and so forth, and share a basic interface:
///
/// @li The first argument, const String &@a str, contains the string to be
/// parsed.
/// @li The last argument(s) specify locations where the parsed results should
/// be stored. These @a result arguments have pointer type.
/// @li The return type is bool.  True is returned if and only if parsing
/// succeeds, and the @a result slots are modified if and only if parsing
/// succeeds.
/// @li Most parsing functions expect to parse the entire supplied string. Any
/// extraneous characters, such as trailing whitespace, cause parsing to
/// fail.
/// @li Most parsing functions never report errors to any source; they simply
/// return false when parsing fails.
///
/// <h3>Argument Manipulation</h3>
///
/// Finally, functions like cp_uncomment(), cp_unquote(), cp_quote(),
/// cp_argvec(), and cp_is_space() manipulate arguments as strings.
/// cp_uncomment() removes comments and simplifies white space; cp_unquote()
/// removes quotation marks and expands backslash escapes; cp_argvec() splits
/// a configuration string at commas; and so forth.


/** @brief  Find the first nonspace character in the string [@a begin, @a end).
 *  @param  begin  beginning of string
 *  @param  end    one past end of string
 *  @return Pointer to first non-space character in [@a begin, @a end), or
 *          @a end if the string is all spaces.
 *
 *  Space characters are defined as by isspace() in the "C" locale, and
 *  consist of the characters in <tt>[ \\f\\n\\r\\t\\v]</tt>. */
const char *
cp_skip_space(const char *begin, const char *end)
{
    while (begin < end && isspace((unsigned char) *begin))
	begin++;
    return begin;
}

/** @brief  Remove spaces from the beginning of @a str.
 *  @param[in,out]  str  string
 *  @return  True if the resulting string is nonempty, false otherwise. */
bool
cp_eat_space(String &str)
{
    const char *begin = str.begin(), *end = str.end();
    const char *space = cp_skip_space(begin, end);
    str = str.substring(space, end);
    return space != begin;
}

/** @brief  Test whether @a str is a valid "word".
 *
 *  A "word" in Click consists of one or more characters in the ASCII range
 *  '!' through '~', inclusive, except for the quote characters '"' and ''',
 *  the backslash '\', and the comma ','. */
bool
cp_is_word(const String &str)
{
    for (const char *s = str.begin(); s != str.end(); s++)
	if (*s == '\"' || *s == '\'' || *s == '\\' || *s == ','
	    || *s <= 32 || *s >= 127)
	    return false;
    return str.length() > 0;
}

/** @brief  Test if @a str is a valid Click identifier.
 *
 *  A Click identifier consists of one or more characters in the set
 *  <tt>[A-Za-z0-9/_@]</tt>, with restrictions on where <tt>/</tt> may appear
 *  (it cannot be the first character or the last character, and two adjacent
 *  slashes aren't allowed either). */
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

/** @brief  Return the first character after a double-quoted string starting at @a begin.
 *  @param  begin  beginning of double-quoted string
 *  @param  end    one past end of string
 *  @return Pointer to first character in [@a begin, @a end) after the
 *          double-quoted string, or @a end if the double-quoted portion is not
 *          correctly terminated.
 *  @pre    @a begin < @a end and *@a begin == '\"'
 *
 *  cp_skip_double_quote() understands all the backslash escapes processed
 *  by cp_process_backslash(). */
const char *
cp_skip_double_quote(const char *begin, const char *end)
{
  assert(begin < end && *begin == '\"');

  for (begin++; begin < end; )
    if (*begin == '\\')
      begin = skip_backslash(begin, end);
    else if (*begin == '\"')
      return begin + 1;
    else
      begin++;

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

/// @brief  Find the first nonspace, noncomment character in the string [@a begin, @a end).
/// @param  begin  beginning of string
/// @param  end    one past end of string
/// @return Pointer to first nonspace and noncomment character in [@a begin,
///	    @a end), or @a end if the string is all spaces and comments.
///
/// This function recognizes C-style and C++-style comments:
/// @code
/// /* C style */  // C++ style (runs until newline)
/// @endcode
/// In C++-style comments, the character
/// sequences <tt>"\n"</tt>, <tt>"\r"</tt>, and <tt>"\r\n"</tt> are
/// recognized as newlines.  The newline is considered part of the comment.
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
	s = cp_skip_double_quote(s, end);
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

/// @brief  Simplify @a str's whitespace and replace comments by spaces,
///	    returning the result.
/// @return  A version of @a str with all initial space removed, all final
///	     space removed, and all comments and space-comment sequences
///	     replaced by a single space character.
///
/// Adjacent space characters are preserved in the output @em unless they
/// appear next to a comment.  For example:
/// @code
/// cp_uncomment("  a   b  ") == "a   b", but:
/// cp_uncomment("  a /* Comment */       b  ") == "a b"
/// @endcode
/// Comment characters inside double or single quotes are ignored:
/// @code
/// cp_uncomment("  \" /*???  */ \"  ") == "\" /*???  */ \""
/// @endcode
String
cp_uncomment(const String &str)
{
  return partial_uncomment(str, 0, 0);
}

/// @brief  Process a backslash escape, appending results to @a sa.
/// @param  begin  beginning of string
/// @param  end    end of string
/// @param  sa     string accumulator
/// @pre  @a begin < @a end, and @a begin points to a backslash character.
/// @return A pointer to the first character in [@a begin, @a end) following
///	    the backslash escape.
///
/// This function understands the following backslash escapes.
/// <ul>
/// <li><tt>"\[newline]"</tt> is ignored (it adds no characters to @a sa),
/// where <tt>[newline]</tt> is one of the sequences <tt>"\n"</tt>,
/// <tt>"\r"</tt>, or <tt>"\r\n"</tt>.</li>
/// <li><tt>"\[C escape]"</tt> is processed as in C, where <tt>[C escape]</tt>
/// is one of the characters in <tt>[abfnrtv]</tt>.</li>
/// <li><tt>"\\"</tt> expands to a single backslash.  Similarly,
/// <tt>"\$"</tt>, <tt>"\'"</tt>, <tt>"\\""</tt>, and <tt>"\,"</tt>
/// expand to the escaped character.</li>
/// <li><tt>"\[1-3 octal digits]"</tt> expands to the given character.</li>
/// <li><tt>"\x[hex digits]"</tt> expands to the given character.</li>
/// <li><tt>"\<[hex digits, spaces, and comments]>"</tt> expands to the
/// binary string indicated by the <tt>hex digits</tt>.  Spaces and comments
/// are removed.  For example,
/// @code
/// "\<48656c6C 6f 2 /* And finally */ 1>" expands to "Hello!"
/// @endcode
/// (This example should begin with <tt>"\<"</tt>; it may not because of Doxygen problems.)</li>
/// <li>A backslash at the end of the string expands to a backslash.</li>
/// </ul>
const char *
cp_process_backslash(const char *begin, const char *end, StringAccum &sa)
{
  assert(begin < end && *begin == '\\');

  if (begin == end - 1) {
    sa << '\\';
    return end;
  }

  switch (begin[1]) {

   case '\r':
    return (begin + 2 < end && begin[2] == '\n' ? begin + 3 : begin + 2);

   case '\n':
    return begin + 2;

   case 'a': sa << '\a'; return begin + 2;
   case 'b': sa << '\b'; return begin + 2;
   case 'f': sa << '\f'; return begin + 2;
   case 'n': sa << '\n'; return begin + 2;
   case 'r': sa << '\r'; return begin + 2;
   case 't': sa << '\t'; return begin + 2;
   case 'v': sa << '\v'; return begin + 2;

   case '0': case '1': case '2': case '3':
   case '4': case '5': case '6': case '7': {
     int c = 0, d = 0;
     for (begin++; begin < end && *begin >= '0' && *begin <= '7' && d < 3;
	  begin++, d++)
       c = c*8 + *begin - '0';
     sa << (char)c;
     return begin;
   }

   case 'x': {
     int c = 0;
     for (begin += 2; begin < end; begin++)
       if (*begin >= '0' && *begin <= '9')
	 c = c*16 + *begin - '0';
       else if (*begin >= 'A' && *begin <= 'F')
	 c = c*16 + *begin - 'A' + 10;
       else if (*begin >= 'a' && *begin <= 'f')
	 c = c*16 + *begin - 'a' + 10;
       else
	 break;
     sa << (char)c;
     return begin;
   }

   case '<': {
     int c = 0, d = 0;
     for (begin += 2; begin < end; begin++) {
       if (*begin == '>')
	 return begin + 1;
       else if (*begin >= '0' && *begin <= '9')
	 c = c*16 + *begin - '0';
       else if (*begin >= 'A' && *begin <= 'F')
	 c = c*16 + *begin - 'A' + 10;
       else if (*begin >= 'a' && *begin <= 'f')
	 c = c*16 + *begin - 'a' + 10;
       else if (*begin == '/' && begin + 1 < end && (begin[1] == '/' || begin[1] == '*')) {
	 begin = skip_comment(begin, end) - 1;
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

   case '\\': case '\'': case '\"': case '$': case ',':
   default:
    sa << begin[1];
    return begin + 2;

  }
}

/// @brief  Remove one level of quoting from @a str, returning the result.
///
/// This function acts as cp_uncomment, plus removing one level of quoting.
/// <tt>"..."</tt> and <tt>'...'</tt> sequences are replaced by their contents.
/// Backslash escapes are expanded inside double quotes (see
/// cp_process_backslash).  Additionally, <tt>"\<...>"</tt> sequences are
/// expanded outside of any quotes.  For example:
/// @code
/// cp_unquote("\"\\n\" abc /* 123 */ '/* def */'") == "\n abc /* def */"
/// @endcode
String
cp_unquote(const String &str)
{
  String xtr = partial_uncomment(str, 0, 0);
  const char *s = xtr.data();
  const char *end = xtr.end();

  // accumulate a word
  StringAccum sa;
  const char *start = s;
  int quote_state = 0;

  for (; s < end; s++)
    switch (*s) {

     case '\"':
     case '\'':
      if (quote_state == 0) {
	sa << xtr.substring(start, s); // null string if start >= s
	start = s + 1;
	quote_state = *s;
      } else if (quote_state == *s) {
	sa << xtr.substring(start, s);
	start = s + 1;
	quote_state = 0;
      }
      break;

     case '\\':
      if (s + 1 < end && (quote_state == '\"'
			  || (quote_state == 0 && s[1] == '<'))) {
	sa << xtr.substring(start, s);
	start = cp_process_backslash(s, end, sa);
	s = start - 1;
      }
      break;

    }

  if (start == xtr.begin())
    return xtr;
  else {
    sa << xtr.substring(start, s);
    return sa.take_string();
  }
}

/// @brief  Return a quoted version of @a str.
/// @param  str  string
/// @param  allow_newlines  If true, then newline sequences are allowed in
///	    in the result.  If false, then newline sequences should be
///	    translated to their backslash escape equivalents.  Default is false.
///
/// Returns a double-quoted string that, when unquoted by cp_unquote(), will
/// equal @a str.  The returned string consists of a single double-quoted
/// string, and in particular is never empty.
///
/// @invariant cp_quote(@a str) != "" && cp_unquote(cp_quote(@a str)) == @a str
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

/// @brief  Separate a configuration string into arguments at commas.
/// @param       str   configuration string
/// @param[out]  conf  arguments
///
/// The configuration string is broken into arguments at unquoted commas.
/// Each argument is passed through cp_uncomment(), then appended to @a conf.
/// If the final argument is empty, it is ignored.  For example:
/// @code
/// cp_argvec("a, b, c", v)            appends  "a", "b", "c"
/// cp_argvec("  a /*?*/ b,  c, ", v)  appends  "a b", "c"
/// cp_argvec("\"x, y\" // ?", v)      appends  "\"x, y\""
/// @endcode
void
cp_argvec(const String &str, Vector<String> &conf)
{
  // common case: no configuration
  int len = str.length();
  if (len == 0)
    return;

  for (int pos = 0; pos < len; pos++) {
    String arg = partial_uncomment(str, pos, &pos);
    // add the argument if it is nonempty or not the last argument
    if (arg || pos < len)
      conf.push_back(arg);
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
      s = cp_skip_double_quote(s, end);
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

/// @brief  Separate a configuration string into arguments at unquoted spaces.
/// @param       str   configuration string
/// @param[out]  conf  arguments
///
/// The configuration string is broken into arguments at unquoted spaces.
/// Each argument is passed through cp_uncomment(), then appended to @a conf.
/// If the final argument is empty, it is ignored.  For example:
/// @code
/// cp_spacevec("a  b, c", v)            appends  "a", "b,", "c"
/// cp_spacevec("  'a /*?*/ b'c", v)     appends  "'a /*?*/ b'c"
/// @endcode
void
cp_spacevec(const String &str, Vector<String> &conf)
{
  // common case: no configuration
  if (str.length() == 0)
    return;

  // collect arguments like cp_shift_spacevec
  const char *s = str.data();
  const char *end = str.end();
  while ((s = cp_skip_comment_space(s, end)) < end) {
    const char *t = skip_spacevec_item(s, end);
    conf.push_back(str.substring(s, t));
    s = t;
  }
}

String
cp_shift_spacevec(String &str)
{
  const char *item = cp_skip_comment_space(str.begin(), str.end());
  const char *item_end = skip_spacevec_item(item, str.end());
  String answer = str.substring(item, item_end);
  item_end = cp_skip_comment_space(item_end, str.end());
  str = str.substring(item_end, str.end());
  return answer;
}

/// @brief  Join the strings of @a conf with commas and return the result.
///
/// This function does not quote or otherwise protect the strings in @a conf.
/// The caller should do that if necessary.
String
cp_unargvec(const Vector<String> &conf)
{
  if (conf.size() == 0)
    return String();
  else if (conf.size() == 1)
    return conf[0];
  else {
    StringAccum sa;
    sa << conf[0];
    for (int i = 1; i < conf.size(); i++)
      sa << ", " << conf[i];
    return sa.take_string();
  }
}

/// @brief  Join the strings in [@a begin, @a end) with spaces and return the result.
/// @param  begin  first string in range
/// @param  end    one past last string in range
///
/// This function does not quote or otherwise protect the strings in [@a
/// begin, @a end).  The caller should do that if necessary.
/// @sa cp_unspacevec(const Vector<String> &)
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

/** @brief Parse a string from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @param[out]  rest  (optional) stores unparsed portion of @a str
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a string from @a str.  The first unquoted space encountered ends the
 * string, but spaces are allowed within single or double quotes.  Unquoted
 * empty strings are not accepted.  If the string fully parses, then the
 * result is unquoted by cp_unquote() and stored in *@a result and the function
 * returns true.  Otherwise, *@a result remains unchanged and the function
 * returns false.
 *
 * If @a rest is nonnull, then the string doesn't need to fully parse; the
 * part of the string starting with the first unquoted space is stored in *@a
 * rest and the function returns true.
 */
bool
cp_string(const String &str, String *result, String *rest)
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
      s = cp_skip_double_quote(s, end);
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
    *result = cp_unquote(str.substring(str.begin(), s));
    return true;
  }
}

/** @brief Parse a word from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @param[out]  rest  (optional) stores unparsed portion of @a str
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a word from @a str.  The first unquoted space encountered ends the
 * word.  Single and double quotes are removed as by cp_unquote, but the
 * unquoted string must satisfy cp_is_word.  If the string fully parses, then
 * the resulting value is stored in *@a result and the function returns true.
 * Otherwise, *@a result remains unchanged and the function returns false.
 *
 * If @a rest is nonnull, then the string doesn't need to fully parse; the
 * part of the string starting with the first unquoted space is stored in *@a
 * rest and the function returns true (assuming cp_is_word succeeds on the
 * initial portion).
 */
bool
cp_word(const String &str, String *result, String *rest)
{
  String word;
  if (!cp_string(str, &word, rest))
    return false;
  else if (!cp_is_word(word))
    return false;
  else {
    *result = word;
    return true;
  }
}

/** @brief Parse a keyword from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @param[out]  rest  (optional) stores unparsed portion of @a str
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a keyword from @a str.  Keywords consist of characters in
 * <tt>[A-Za-z0-9_.:?!]</tt>.  Quotes and spaces are not allowed; neither is the
 * empty string.  If the string fully parses as a keyword, then the resulting
 * value is stored in *@a result and the function returns true.  Otherwise,
 * *@a result remains unchanged and the function returns false.
 *
 * If @a rest is nonnull, then the string doesn't need to fully parse; the
 * part of the string starting with the first unquoted space is stored in *@a
 * rest and the function returns true (assuming the initial portion is a valid
 * keyword).
 */
bool
cp_keyword(const String &str, String *result, String *rest)
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
     case '?':
     case '!':
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
    *result = str.substring(str.begin(), s);
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

/** @brief Parse a boolean from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a boolean from @a str.  The following strings are the valid
 * representations for booleans:
 *
 * <dl>
 * <dt>0, false, no, f, n<dt><dd>Means false</dd>
 * <dt>1, true, yes, t, y</dt><dd>Means true</dd>
 * </dl>
 *
 * If the string fully parses, then the resulting value is stored in *@a
 * result and the function returns true.  Otherwise, *@a result remains
 * unchanged and the function returns false.
 */
bool
cp_bool(const String &str, bool *result)
{
    return BoolArg::parse(str, *result);
}



/* Integer parsing helper functions */

#define LARGEBITS		(sizeof(String::uintmax_t) * 8)
#define LARGEMSVBITS		6
#define LARGETHRESHSHIFT	(LARGEBITS - LARGEMSVBITS)
#define LARGETHRESH		((String::uintmax_t) 1 << LARGETHRESHSHIFT)

const char *
cp_basic_integer(const char *begin, const char *end, int flags, int size,
		 void *result)
{
    IntArg ia(flags & 63);
    IntArg::limb_type limbs[4];
    int usize = size < 0 ? -size : size;
    const char *x = ia.parse(begin, end,
			     size < 0, usize,
			     limbs, usize / sizeof(IntArg::limb_type));
    if ((ia.status && ia.status != IntArg::status_range)
	|| ((flags & cp_basic_integer_whole) && x != end)) {
	cp_errno = CPE_FORMAT;
	return begin;
    } else if (ia.status == IntArg::status_ok)
	cp_errno = CPE_OK;
    else
	cp_errno = CPE_OVERFLOW;

    // assign
    if (usize == 1)
	extract_integer(limbs, *static_cast<unsigned char *>(result));
    else if (usize == sizeof(short))
	extract_integer(limbs, *static_cast<unsigned short *>(result));
    else if (usize == sizeof(int))
	extract_integer(limbs, *static_cast<unsigned *>(result));
    else if (usize == sizeof(long))
	extract_integer(limbs, *static_cast<unsigned long *>(result));
#if HAVE_LONG_LONG
    else if (usize == sizeof(long long))
	extract_integer(limbs, *static_cast<unsigned long long *>(result));
#endif
#if HAVE_INT64_TYPES && (!HAVE_LONG_LONG || (!HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG))
    else if (usize == sizeof(int64_t))
	extract_integer(limbs, *static_cast<uint64_t *>(result));
#endif
    else
	assert(0);

    return x;
}


#if CLICK_USERLEVEL || CLICK_TOOL

/** @brief  Parse a file offset from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an unsigned integer from @a str, similarly to cp_integer(str,
 * result). */
bool
cp_file_offset(const String &str, off_t *result)
{
# if SIZEOF_OFF_T == 4 || (SIZEOF_OFF_T == 8 && HAVE_INT64_TYPES)
    return IntArg().parse(str, *result);
# elif SIZEOF_OFF_T == 8
#  warning "--disable-int64 means I can handle files up to only 4GB"
    uint32_t x;
    if (IntArg().parse(str, x)) {
	*result = x;
	return true;
    } else
	return false;
# else
#  error "unexpected sizeof(off_t)"
# endif
}

#endif


// PARSING REAL NUMBERS

static uint32_t exp10val[] = { 1, 10, 100, 1000, 10000, 100000, 1000000,
			       10000000, 100000000, 1000000000 };

bool
cp_real10(const String &str, int frac_digits, int exponent_delta,
	  uint32_t *return_int_part, uint32_t *return_frac_part)
{
    DecimalFixedPointArg dfpa(frac_digits, exponent_delta);
    if (!dfpa.parse_saturating(str, *return_int_part, *return_frac_part)) {
	cp_errno = CPE_FORMAT;
	return false;
    } else if (dfpa.status == NumArg::status_range)
	cp_errno = CPE_OVERFLOW;
    else
	cp_errno = CPE_OK;
    return true;
}

/** @brief Parse a real number from @a str, representing the result as an
 * integer with @a frac_digits decimal digits of fraction.
 * @param  str  string
 * @param  frac_digits  number of decimal digits of fraction, 0-9
 * @param[out]  result_int_part  stores integer portion of parsed result
 * @param[out]  result_frac_part  stores fractional portion of parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an unsigned real number from an input string.  The result is
 * represented with @a frac_digits decimal digits of fraction.  The integer
 * and fraction parts of the result are stored in two separate integers, @a
 * result_int_part and @a result_frac_part.  For example, the number 10.5
 * would be represented as 10 and 5 if @a frac_digits == 1, or 10 and 5000 if
 * @a frac_digits == 4.  If the string fully parses, then the resulting value
 * is stored in the result variables and the function returns true.
 * Otherwise, the result variables remains unchanged and the function returns
 * false.
 *
 * The real number format and error conditions are the same as for cp_real2().
 * (Negative numbers are not allowed.)
 */
bool
cp_real10(const String &str, int frac_digits,
	  uint32_t *result_int_part, uint32_t *result_frac_part)
{
    return cp_real10(str, frac_digits, 0, result_int_part, result_frac_part);
}

bool
cp_real10(const String &str, int frac_digits, int exponent_delta,
	  uint32_t *result)
{
    DecimalFixedPointArg dfpa(frac_digits, exponent_delta);
    if (!dfpa.parse_saturating(str, *result)) {
	cp_errno = CPE_FORMAT;
	return false;
    } else if (dfpa.status == dfpa.status_range)
	cp_errno = CPE_OVERFLOW;
    else
	cp_errno = CPE_OK;
    return true;
}

bool
cp_real10(const String &str, int frac_digits, uint32_t *result)
{
    return cp_real10(str, frac_digits, 0, result);
}

bool
cp_real2(const String &str, int frac_bits, uint32_t *result)
{
    if (frac_bits < 0 || frac_bits > 32) {
	cp_errno = CPE_INVALID;
	return false;
    }

    FixedPointArg fpa(frac_bits);
    if (!fpa.parse_saturating(str, *result)) {
	cp_errno = CPE_FORMAT;
	return false;
    } else if (fpa.status == NumArg::status_range)
	cp_errno = CPE_OVERFLOW;
    else
	cp_errno = CPE_OK;
    return true;
}


// Parsing signed reals

/** @brief Parse a real number from @a str, representing the result as an
 * integer with @a frac_digits decimal digits of fraction.
 * @param  str  string
 * @param  frac_digits  number of decimal digits of fraction, 0-9
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a real number from an input string.  The result is represented as an
 * integer with @a frac_digits decimal digits of fraction.  For example, the
 * number 0.5 would be represented as 5 if @a frac_digits == 1, or 5000 if @a
 * frac_digits == 4.  If the string fully parses, then the resulting value is
 * stored in *@a result and the function returns true.  Otherwise, *@a result
 * remains unchanged and the function returns false.
 *
 * The real number format and error conditions are the same as for cp_real2().
 *
 * An overloaded version of this function is available for uint32_t
 * @a result values; it doesn't accept negative numbers.
 */
bool
cp_real10(const String &str, int frac_digits, int32_t *result)
{
    DecimalFixedPointArg dfpa(frac_digits);
    if (!dfpa.parse_saturating(str, *result)) {
	cp_errno = CPE_FORMAT;
	return false;
    } else if (dfpa.status == dfpa.status_range)
	cp_errno = CPE_OVERFLOW;
    else
	cp_errno = CPE_OK;
    return true;
}

/** @brief  Parse a fixed-point number from @a str.
 * @param  str  string
 * @param  frac_bits  number of bits of fraction, 0-CP_REAL2_MAX_FRAC_BITS
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a real number from an input string.  The result is represented as a
 * fixed-point number with @a frac_bits bits of fraction.  For example, the
 * number 0.5 would be represented as 0x1 if @a frac_bits == 1, or 0x8000 if
 * @a frac_bits == 16.  If the string fully parses, then the resulting value
 * is stored in *@a result and the function returns true.  Otherwise, *@a
 * result remains unchanged and the function returns false.
 *
 * The real number format is the familiar decimal format parsed by, for
 * example, C's strtod() function.  It consists of, in order:
 *
 * @li An optional <tt>+</tt> or <tt>-</tt> sign.
 * @li An optional sequence of decimal digits representing the integer part.
 * @li An optional fraction point, followed by an optional sequence of decimal
 * digits representing the fraction part.
 * @li An optional exponent (either <tt>E&lt;digits&gt;</tt>,
 * <tt>E+&lt;digits&gt;</tt>, or <tt>E-&lt;digits&gt;</tt>).
 *
 * There must be at least one digit in either the integer part or the fraction
 * part.  As with cp_integer, digits can be separated by underscores to make
 * large numbers easier to read.  Some examples:
 *
 * @code
 * 0
 * -100_000_000
 * 1e8
 * +10.
 * .1
 * 0.000_000_01e10
 * @endcode
 *
 * This function checks for overflow.  If a number is too large for @a result,
 * then the maximum possible value is stored in @a result and the cp_errno
 * variable is set to CPE_OVERFLOW.  Otherwise, cp_errno is set to CPE_FORMAT
 * (unparsable input) or CPE_OK (if all was well).  Underflow is handled by
 * rounding the result to the nearest representable number.
 *
 * The following invariant always holds for all values @e x and fraction bits
 * @e frac_bits:
 * @code
 * check_invariant(int32_t x, int frac_bits) {
 *     int32_t y;
 *     assert(cp_real2(cp_unparse_real2(x, frac_bits), frac_bits, &y) == true
 *            && y == x);
 * }
 * @endcode
 *
 * An overloaded version of this function is available for uint32_t
 * @a result values; it doesn't accept negative numbers.
 *
 * @sa The cp_real10() functions behave like cp_real2(), but the fractional
 * part is expressed in decimal digits rather than bits.
 */
bool
cp_real2(const String &str, int frac_bits, int32_t *result)
{
    FixedPointArg fpa(frac_bits);
    if (!fpa.parse_saturating(str, *result)) {
	cp_errno = CPE_FORMAT;
	return false;
    } else if (fpa.status == NumArg::status_range)
	cp_errno = CPE_OVERFLOW;
    else
	cp_errno = CPE_OK;
    return true;
}

#ifdef HAVE_FLOAT_TYPES
/** @brief  Parse a real number from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a real number from an input string in double format.  It is
 * basically equivalent to C's strtod(), but follows Click's configuration
 * parsing conventions: If the string fully parses, then the resulting value
 * is stored in *@a result and the function returns true.  Otherwise, *@a
 * result remains unchanged and the function returns false.  If a number is
 * too large for @a result, then the maximum possible value is stored in @a
 * result and the cp_errno variable is set to CPE_OVERFLOW; otherwise,
 * cp_errno is set to CPE_FORMAT (unparsable) or CPE_OK (if all was well).
 *
 * This function is not available in the kernel, since double objects cannot
 * be used there.
 */
bool
cp_double(const String &str, double *result)
{
    DoubleArg da;
    double d;
    (void) da.parse(str, d);
    if (da.status == DoubleArg::status_ok || da.status == DoubleArg::status_range) {
	*result = d;
	cp_errno = (da.status == DoubleArg::status_ok ? 0 : CPE_OVERFLOW);
	return true;
    } else {
	cp_errno = CPE_FORMAT;
	return false;
    }
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

/** @brief Parse an amount of time from @a str.
 * @param  str  string
 * @param  frac_digits  number of decimal digits of fraction, 0-9
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an amount of time, measured in seconds, from @a str.  @a frac_digits
 * is the number of decimal digits of fraction returned in the result.  For
 * example, to measure the result in milliseconds, set @a frac_digits == 3;
 * for microseconds, set @a frac_digits == 6.  Does not handle negative
 * amounts of time.
 *
 * The input string is a real number (as in cp_real2) followed by an optional
 * unit suffix.  Units are:
 *
 * <dl>
 * <dt>ns, nsec</dt><dd>nanoseconds</dd>
 * <dt>us, usec</dt><dd>microseconds</dd>
 * <dt>ms, msec</dt><dd>milliseconds</dd>
 * <dt>s, sec</dt><dd>seconds</dd>
 * <dt>m, min</dt><dd>minutes</dd>
 * <dt>h, hr</dt><dd>hours</dd>
 * <dt>d, day</dt><dd>days</dd>
 * </dl>
 *
 * The default unit suffix is seconds.  Thus, "3600", "3600s", "3.6e6 msec",
 * "60m", and "1 hr" all parse to the same result, 3600 seconds.
 *
 * If the string fully parses, then the resulting value is stored in *@a
 * result and the function returns true.  Otherwise, *@a result remains
 * unchanged and the function returns false.
 *
 * If a number is too large for @a result, then the maximum possible value is
 * stored in @a result and the cp_errno variable is set to CPE_OVERFLOW;
 * otherwise, cp_errno is set to CPE_FORMAT (unparsable) or CPE_OK (if all was
 * well).
 */
bool cp_seconds_as(const String &str, int frac_digits, uint32_t *result)
{
    SecondsArg sa(frac_digits);
    if (!sa.parse_saturating(str, *result))
	return false;
    else if (sa.status == sa.status_range)
	cp_errno = CPE_OVERFLOW;
    else
	cp_errno = CPE_OK;
    return true;
}

/** @brief Parse an amount of time in milliseconds from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an amount of time, measured in milliseconds, from @a str.
 * Equivalent to cp_seconds_as(@a str, 3, @a result).
 */
bool cp_seconds_as_milli(const String &str, uint32_t *result)
{
    return cp_seconds_as(str, 3, result);
}

/** @brief Parse an amount of time in microseconds from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an amount of time, measured in microseconds, from @a str.
 * Equivalent to cp_seconds_as(@a str, 6, @a result).
 */
bool cp_seconds_as_micro(const String &str, uint32_t *result)
{
    return cp_seconds_as(str, 6, result);
}

#if HAVE_FLOAT_TYPES
/** @brief Parse an amount of time from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an amount of time, measured in seconds, from @a str.  As in
 * cp_seconds_as(), the input string is a real number followed by an optional
 * unit suffix which defaults to seconds.
 *
 * If the string fully parses, then the resulting value is stored in *@a
 * result and the function returns true.  Otherwise, *@a result remains
 * unchanged and the function returns false.
 */
bool cp_seconds(const String &str, double *result)
{
    return SecondsArg().parse(str, *result);
}
#endif

/** @brief Parse a timestamp from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @param  allow_negative  allow negative timestamps if true
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a timestamp from @a str.  Timestamps are expressed as fractional
 * amounts of seconds, usually measured in Unix time, such as
 * <tt>"1189383079.180265331"</tt>.  The input format accepts the unit
 * suffixes described at cp_seconds_as.  If the string fully parses, then the
 * resulting value is stored in *@a result and the function returns true.
 * Otherwise, *@a result remains unchanged and the function returns false.
 *
 * If a number is too large for @a result, then the maximum possible value is
 * stored in @a result and the cp_errno variable is set to CPE_OVERFLOW;
 * otherwise, cp_errno is set to CPE_FORMAT (unparsable) or CPE_OK (if all was
 * well).
 *
 * An overloaded version of this function is available for struct timeval @a
 * result values.
 */
bool cp_time(const String &str, Timestamp *result, bool allow_negative)
{
    int power = 0, factor = 1;
    const char *begin = str.begin(), *end = str.end();
    const char *after_unit = read_unit(begin, end, seconds_units, sizeof(seconds_units), seconds_prefixes, &power, &factor);

    bool negative = false;
    if (allow_negative && after_unit - begin > 1
	&& begin[0] == '-' && begin[1] != '+') {
	negative = true;
	++begin;
    }

    uint32_t sec, nsec;
    if (!cp_real10(str.substring(begin, after_unit), 9, power, &sec, &nsec))
	return false;
    if (factor != 1) {
	nsec *= factor;
	int delta = nsec / 1000000000;
	nsec -= delta * 1000000000;
	sec = (sec * factor) + delta;
    }
    if (!negative)
	*result = Timestamp::make_nsec(sec, nsec);
    else
	*result = Timestamp::make_nsec(-(Timestamp::seconds_type) sec, nsec);
    return true;
}

bool cp_time(const String &str, timeval *result)
{
#if TIMESTAMP_PUNS_TIMEVAL
    return cp_time(str, reinterpret_cast<Timestamp*>(result));
#else
    Timestamp t;
    if (cp_time(str, &t)) {
	*result = t.timeval();
	return true;
    } else
	return false;
#endif
}


/** @brief Parse a bandwidth value from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a bandwidth value from @a str.  The input format is a real number
 * followed by an optional unit suffix.  Units are:
 *
 * <dl>
 * <dt>baud, bps, b/s</dt><dd>bits per second</dd>
 * <dt>Bps, B/s</dt><dd>bytes per second</dd>
 * <dt>e.g. kbaud, Mbps, GBps</dt><dd>kilo, mega, giga are supported
 * (they mean 10^3, 10^6, and 10^9)</dd>
 * </dl>
 *
 * The default unit suffix is bytes per second.
 *
 * If a number is too large for @a result, then the maximum possible value is
 * stored in @a result and the cp_errno variable is set to CPE_OVERFLOW;
 * otherwise, cp_errno is set to CPE_FORMAT (unparsable) or CPE_OK (if all was
 * well).
 */
bool cp_bandwidth(const String &str, uint32_t *result)
{
    BandwidthArg ba;
    uint32_t x;
    if (!ba.parse(str, x)) {
	cp_errno = CPE_FORMAT;
	return false;
    }
    if (ba.status == NumArg::status_range)
	cp_errno = CPE_OVERFLOW;
    else if (ba.status == NumArg::status_unitless)
	cp_errno = CPE_NOUNITS;
    else
	cp_errno = CPE_OK;
    *result = x;
    return true;
}



// PARSING IPv4 ADDRESSES

bool
cp_ip_address(const String &str, unsigned char *result
	      CP_CONTEXT)
{
#if CLICK_TOOL
    return IPAddressArg::parse(str, reinterpret_cast<IPAddress &>(*result));
#else
    return IPAddressArg::parse(str, reinterpret_cast<IPAddress &>(*result), Args(context));
#endif
}

bool
cp_ip_prefix(const String &str,
	     unsigned char *result_addr, unsigned char *result_mask,
	     bool allow_bare_address  CP_CONTEXT)
{
#if CLICK_TOOL
    return IPPrefixArg(allow_bare_address).parse(str, reinterpret_cast<IPAddress &>(*result_addr), reinterpret_cast<IPAddress &>(*result_mask));
#else
    return IPPrefixArg(allow_bare_address).parse(str, reinterpret_cast<IPAddress &>(*result_addr), reinterpret_cast<IPAddress &>(*result_mask), Args(context));
#endif
}

/** @brief Parse an IP address from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @param  context  optional context for @e AddressInfo
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an IP address from @a str.  The input format is the usual
 * dotted-quad format, as in <tt>"18.26.4.9"</tt>, where each number is a
 * decimal number from 0-255.  The @e AddressInfo element can be used to register
 * shorthand names for other IP addresses.  If the string fully parses, then
 * the resulting value is stored in *@a result and the function returns true.
 * Otherwise, *@a result remains unchanged and the function returns false.
 *
 * Overloaded versions of this function are available for unsigned char[4] and
 * struct in_addr * result types.
 */
bool
cp_ip_address(const String &str, IPAddress *result  CP_CONTEXT)
{
  return cp_ip_address(str, result->data()  CP_PASS_CONTEXT);
}

/** @brief Parse an IP address or prefix from @a str.
 * @param  str  string
 * @param[out]  result_addr  stores parsed address result
 * @param[out]  result_mask  stores parsed address mask result
 * @param  allow_bare_address  optional: if true, allow raw IP addresses;
 * defaults to false
 * @param  context  optional context for @e AddressInfo
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an IP prefix description from @a str.  The input format is the usual
 * CIDR format with some additions.  Allowed examples:
 *
 * <ul>
 * <li><tt>"18.26.4.0/24"</tt>: the default CIDR format.  The prefix length,
 * here 24, is a number between 0 and 32.  This stores the equivalents of
 * 18.26.4.0 in *@a result_addr and 255.255.255.0 in *@a result_mask.</li>
 * <li><tt>"18.26.4/24"</tt>: it is OK to leave off irrelevant parts of the
 * address.  However, <tt>"18.26/24"</tt> will not parse.
 * <li><tt>"18.26.4.0/255.255.255.0"</tt>: the mask may be specified directly.
 * This is the only way to define a non-prefix mask.</li>
 * <li>Additionally, @e AddressInfo names may be used to specify the address part
 * or the whole prefix: given AddressInfo(a 18.26.4.9), <tt>"a/24"</tt> is
 * parseable as an IP prefix.</li>
 * </ul>
 *
 * The address part need not fit entirely within the prefix.
 * <tt>"18.26.4.9/24"</tt> will parse into address 18.26.4.9 and mask
 * 255.255.255.0.
 *
 * If @a allow_bare_address is true, then a raw IP address is also acceptable
 * input.  The resulting mask will equal 255.255.255.255.  Raw IP addresses
 * take precedence over networks, so given AddressInfo(a 18.26.4.9/24), "a"
 * will parse as 18.26.4.9/32, not 18.26.4/24.  @a allow_bare_address defaults
 * to false.
 *
 * If the string fully parses, then the resulting address is stored in *@a
 * result_addr, the resulting mask is stored in *@a result_mask, and the
 * function returns true.  Otherwise, the results remain unchanged and the
 * function returns false.
 *
 * Overloaded versions of this function are available for unsigned char[4]
 * result types.
 */
bool
cp_ip_prefix(const String &str, IPAddress *result_addr, IPAddress *result_mask,
	     bool allow_bare_address  CP_CONTEXT)
{
  return cp_ip_prefix(str, result_addr->data(), result_mask->data(),
		      allow_bare_address  CP_PASS_CONTEXT);
}

bool
cp_ip_prefix(const String &str, unsigned char *address, unsigned char *mask
	     CP_CONTEXT)
{
  return cp_ip_prefix(str, address, mask,
		      false  CP_PASS_CONTEXT);
}

bool
cp_ip_prefix(const String &str, IPAddress *address, IPAddress *mask
	     CP_CONTEXT)
{
  return cp_ip_prefix(str, address->data(), mask->data(),
		      false  CP_PASS_CONTEXT);
}

/** @brief Parse a space-separated list of IP addresses from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @param  context  optional context for @e AddressInfo
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a space-separated list of IP addresses from @a str.  Each individual
 * IP address is parsed as by cp_ip_address().  If the string fully parses,
 * then *@a result is set to the resulting values in order.  Otherwise, *@a
 * result remains unchanged and the function returns false.
 *
 * In addition to errors from cp_ip_address(), this function might run out of
 * memory for *@a result, which produces a CPE_MEMORY error.
 */
bool
cp_ip_address_list(const String &str, Vector<IPAddress> *result  CP_CONTEXT)
{
    Vector<String> words;
    cp_spacevec(str, words);
    Vector<IPAddress> build;
    build.reserve(words.size());
    for (int i = 0; i < words.size(); i++) {
	IPAddress ip;
	if (!cp_ip_address(words[i], &ip  CP_PASS_CONTEXT))
	    return false;
	build.push_back(ip);
    }
    if (build.size() != words.size()) {
	cp_errno = CPE_MEMORY;
	return false;
    }
    result->swap(build);
    return true;
}


// PARSING IPv6 ADDRESSES

#ifdef HAVE_IP6

bool
cp_ip6_address(const String &str, unsigned char *result
	       CP_CONTEXT)
{
#if CLICK_TOOL
    return IP6AddressArg::parse(str, reinterpret_cast<IP6Address &>(*result));
#else
    return IP6AddressArg::parse(str, reinterpret_cast<IP6Address &>(*result), Args(context));
#endif
}

/** @brief Parse an IPv6 address from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @param  context  optional context for @e AddressInfo
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an IPv6 address from @a str.  The input format may be any of the
 * forms allowed by <a href="ftp://ftp.ietf.org/rfc/rfc2373.txt">RFC2373</a>:
 *
 * - A nonabbreviated address consists of eight colon-separated 16-bit
 *   hexadecimal numbers, as in <tt>"1080:0:0:0:8:800:200C:417a"</tt>.
 * - Groups of zeros may be abbrivated with two colons, as in the equivalent
 *   <tt>"1080::8:800:200C:417A"</tt>.
 * - An address may end with an embedded IPv4 address, as in
 *   <tt>"::13.1.68.3"</tt>, <tt>"::FFFF:129.144.52.38"</tt>, and (assuming
 *   the appropriate @e AddressInfo information) <tt>"0::ip4_addr"</tt>.
 *
 * The @e AddressInfo element can be used to register shorthand names for
 * other IPv6 addresses.  If the string fully parses, then the resulting value
 * is stored in *@a result and the function returns true.  Otherwise, *@a
 * result remains unchanged and the function returns false.
 *
 * An overloaded version of this function is available for unsigned char[16]
 * result type.
 */
bool
cp_ip6_address(const String &str, IP6Address *result
	       CP_CONTEXT)
{
    return cp_ip6_address(str, result->data()  CP_PASS_CONTEXT);
}


bool
cp_ip6_prefix(const String &str,
	      unsigned char *result, int *return_bits,
	      bool allow_bare_address  CP_CONTEXT)
{
#if CLICK_TOOL
    return IP6PrefixArg(allow_bare_address).parse(str, reinterpret_cast<IP6Address &>(*result), *return_bits);
#else
    return IP6PrefixArg(allow_bare_address).parse(str, reinterpret_cast<IP6Address &>(*result), *return_bits, Args(context));
#endif
}

bool
cp_ip6_prefix(const String &str, unsigned char *address, unsigned char *mask,
	      bool allow_bare_address  CP_CONTEXT)
{
  int bits;
  if (cp_ip6_prefix(str, address, &bits, allow_bare_address  CP_PASS_CONTEXT)) {
    IP6Address m = IP6Address::make_prefix(bits);
    memcpy(mask, m.data(), 16);
    return true;
  } else
    return false;
}

/** @brief Parse an IPv6 address or prefix from @a str.
 * @param  str  string
 * @param[out]  result_addr  stores parsed address result
 * @param[out]  result_prefix  stores parsed prefix length result
 * @param  allow_bare_address  if true, allow raw IPv6 addresses
 * @param  context  optional context for @e AddressInfo
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an IPv6 prefix description from @a str.  The input format is the
 * usual CIDR format: an IPv6 address, followed by <tt>"/prefixlen"</tt>,
 * where <tt>prefixlen</tt> is a number between 0 and 128.  As an extension,
 * the format <tt>"addr/mask"</tt> is supported, where both @c addr and @c
 * mask are valid IPv6 addresses.  However, unlike cp_ip_prefix(), @c mask
 * must correspond to a valid prefix length -- some number of one bits,
 * followed by all zero bits.  For example, "::/::1" will not parse.  Finally,
 * @e AddressInfo names may be used to specify the address part or the whole
 * prefix.
 *
 * The address part need not fit entirely within the prefix.
 * <tt>"::1/32"</tt> will parse into address ::1 and prefix length 32.
 *
 * If @a allow_bare_address is true, then a raw IPv6 address is also
 * acceptable input.  The resulting prefix will equal 128.
 *
 * If the string fully parses, then the resulting address is stored in *@a
 * result_addr, the resulting prefix length is stored in *@a result_prefix,
 * and the function returns true.  Otherwise, the results remain unchanged and
 * the function returns false.
 *
 * Overloaded versions of this function are available for unsigned char[16]
 * result address type, and for IP6Address or unsigned char[16] result masks
 * (instead of result prefix lengths).
 */
bool
cp_ip6_prefix(const String &str, IP6Address *result_addr, int *result_prefix,
	      bool allow_bare_address  CP_CONTEXT)
{
    return cp_ip6_prefix(str, result_addr->data(), result_prefix, allow_bare_address  CP_PASS_CONTEXT);
}

bool
cp_ip6_prefix(const String &str, IP6Address *address, IP6Address *prefix,
	      bool allow_bare_address  CP_CONTEXT)
{
  return cp_ip6_prefix(str, address->data(), prefix->data(), allow_bare_address  CP_PASS_CONTEXT);
}

#endif /* HAVE_IP6 */


bool
cp_ethernet_address(const String &str, unsigned char *result
		    CP_CONTEXT)
{
#if !CLICK_TOOL
    return EtherAddressArg().parse(str, *reinterpret_cast<EtherAddress*>(result), Args(context));
#else
    return EtherAddressArg().parse(str, *reinterpret_cast<EtherAddress*>(result));
#endif
}

/** @brief Parse an Ethernet address from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @param  context  optional context for AddressInfo
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses an Ethernet address from @a str.  The input format is the IEEE
 * standard format, as in <tt>"00-15-58-2D-FB-8F"</tt>: six dash-separated
 * 8-bit hexadecimal numbers in transmission order.  Colons are also accepted
 * as separators.  The AddressInfo element can be used to register shorthand
 * names for other Ethernet addresses.  If the string fully parses, then the
 * resulting value is stored in *@a result and the function returns true.
 * Otherwise, *@a result remains unchanged and the function returns false.
 *
 * An overloaded version of this function is available for unsigned char[6]
 * result type.
 */
bool
cp_ethernet_address(const String &str, EtherAddress *result  CP_CONTEXT)
{
  return cp_ethernet_address(str, result->data()  CP_PASS_CONTEXT);
}


/** @brief Parse a TCP, UDP, etc. port number from @a str.
 * @param  str  string
 * @param  proto  protocol number, e.g. IP_PROTO_TCP == 6
 * @param[out]  result  stores parsed result
 * @param  context  optional context for IPNameInfo
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a port number for IP protocol @a proto from @a str.  The input may
 * be a 16-bit number parsable by cp_integer(), as in <tt>"80"</tt>.  It may
 * also be a port name, such as <tt>"www"</tt>.  Several port names are
 * defined by default, including @c auth, @c chargen, @c echo, @c finger, @c
 * ftp, @c https, @c ntp, and @c www.  The @e PortInfo element can be used to
 * define additional names, and at user level, cp_tcpudp_port() will consult
 * the /etc/services database using getservbyname() as a last resort.  If the
 * string fully parses, then the resulting value is stored in *@a result and
 * the function returns true.  Otherwise, *@a result remains unchanged and the
 * function returns false.
 */
bool
cp_tcpudp_port(const String &str, int proto, uint16_t *result
	       CP_CONTEXT)
{
#if !CLICK_TOOL
    return IPPortArg(proto).parse(str, *result, Args(context));
#else
    return IPPortArg(proto).parse(str, *result, 0);
#endif
}


#if !CLICK_TOOL
/** @brief Parse an element reference from @a str.
 * @param str string
 * @param context element context
 * @param errh optional error handler
 * @param argname optional argument name (used for error messages)
 * @return Element pointer, or null if no such element is found.
 *
 * Parses an element reference from @a str.  The input must be a single
 * (possibly quoted) string acceptable to cp_string().  The unquoted value
 * should be an element name.  The name may be relative to a compound element;
 * for instance, if @a context is an element named <tt>a/b/c/xxx</tt>, and @a
 * str was <tt>"yyy"</tt>, then cp_element() would search for elements named
 * <tt>a/b/c/yyy</tt>, <tt>a/b/yyy</tt>, <tt>a/yyy</tt>, and finally
 * <tt>yyy</tt>, returning the first one found.  (See Router::find().)  If no
 * element is found, reports an error to @a errh and returns null.  If @a errh
 * is null, no error is reported.
 *
 * @sa This function differs from Router::find() in that it unquotes its
 * argument.
 */
Element *
cp_element(const String &str, const Element *context, ErrorHandler *errh,
	   const char *argname)
{
    String name;
    if (!cp_string(str, &name)) {
	if (errh && argname)
	    errh->error("type mismatch: %s requires element name", argname);
	else if (errh)
	    errh->error("type mismatch: requires element name");
	return 0;
    }

    Element *e = context->router()->find(name, context);
    if (!e && errh && argname)
	errh->error("%s does not name an element", argname);
    else if (!e && errh)
	errh->error("%<%s%> does not name an element", name.c_str());
    return e;
}

/** @brief Parse an element reference from @a str.
 * @param str string
 * @param router router
 * @param errh optional error handler
 * @param argname optional argument name (used for error messages)
 * @return Element pointer, or null if no such element is found.
 *
 * Parses an element reference from @a str.  The input must be a single
 * (possibly quoted) string acceptable to cp_string().  The unquoted value
 * should be a fully qualified element name corresponding to an element in @a
 * router.  If no element is found, reports an error to @a errh and returns
 * null.  If @a errh is null, no error is reported.
 *
 * @sa This function differs from Router::find() in that it unquotes its
 * argument.
 */
Element *
cp_element(const String &str, Router *router, ErrorHandler *errh,
	   const char *argname)
{
    return cp_element(str, router->root_element(), errh, argname);
}

/** @brief Parse a handler name from @a str.
 * @param  str  string
 * @param[out]  result_element  stores parsed element result
 * @param[out]  result_hname  stores parsed handler name result
 * @param  context  element context
 * @param  errh  optional error handler
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a handler name from @a str.  Three formats are supported:
 *
 * - <tt>"elementname.handlername"</tt>, for a handler on a named element.
 *   The named element must exist; it is looked up as by cp_element() in the
 *   compound element context specified by @a context.
 * - <tt>".handlername"</tt>, for a global handler on @a context's router.
 * - <tt>"handlername"</tt>, for a handler on the @a context element (if
 *   such a handler exists), or a global handler on @a context's router.
 *
 * The handler name must contain at least one character.  Although the named
 * element must exist, this function does not check whether the named handler
 * exists.  The input string may contain quotes; it is unquoted by
 * cp_string().
 *
 * If the string fully parses, then the resulting element is stored in *@a
 * result_element, the resulting handler name is stored in *@a result_hname,
 * and the function returns true.  For global handlers, *@a result_element is
 * set to Router::root_element().  If the string does not fully parse, the
 * results remain unchanged and the function returns false.
 */
bool
cp_handler_name(const String& str,
		Element** result_element, String* result_hname,
		const Element* context, ErrorHandler* errh)
{
    LocalErrorHandler lerrh(errh);

    String text;
    if (!cp_string(str, &text) || !text) {
    syntax_error:
	lerrh.error("syntax error");
	return false;
    }

    Router *r = context->router();
    const char *dot = find(text, '.'), *hstart = text.begin();
    Element *e = 0;

    if (dot == text.begin()) {
	hstart++;
	e = r->root_element();
    } else if (dot == text.end() - 1)
	goto syntax_error;
    else if (dot != text.end()) {
	String ename(text.substring(text.begin(), dot));
	if ((e = r->find(ename, context)))
	    hstart = dot + 1;
    }

    if (!e) {
	if (context->eindex() >= 0 && r->handler(context, text))
	    e = const_cast<Element *>(context);
	else if (dot == text.end() || r->handler(r->root_element(), text))
	    e = r->root_element();
	else {
	    lerrh.error("no element named %<%.*s%>", dot - text.begin(), text.begin());
	    return false;
	}
    }

    *result_element = e;
    *result_hname = text.substring(hstart, text.end());
    return true;
}

/** @brief Parse a handler reference from @a str.
 * @param  str  string
 * @param  flags  zero or more of Handler::h_read, Handler::h_write, and HandlerCall::h_preinitialize
 * @param[out]  result_element  stores parsed element result
 * @param[out]  result_handler  stores parsed handler result
 * @param  context  element context
 * @param  errh  optional error handler
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a handler reference from @a str.  The input format is as in
 * cp_handler_name(), but the named handler must actually exist.  The @a flags
 * argument lets the caller check for read and/or write handlers; its values
 * are as for HandlerCall::initialize().  If the string fully parses, then the
 * resulting element is stored in *@a result_element, the resulting handler is
 * stored in *@a result_handler, and the function returns true.  For global
 * handlers, *@a result_element is set to Router::root_element().  If the
 * string does not fully parse, the results remain unchanged and the function
 * returns false.
 */
bool
cp_handler(const String &str, int flags,
	   Element** result_element, const Handler** result_handler,
	   const Element* context, ErrorHandler* errh)
{
  HandlerCall hc(str);
  if (hc.initialize(flags, context, errh) < 0)
    return false;
  else {
    *result_element = hc.element();
    *result_handler = hc.handler();
    return true;
  }
}
#endif

#if CLICK_USERLEVEL || CLICK_TOOL

/** @brief Parse a filename string from @a str.
 * @param  str  string
 * @param[out]  result  stores parsed result
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a filename from @a str.  This behaves like cp_string() plus
 * shell-style tilde expansion.  Thus, <tt>~/</tt> at the beginning of a
 * string is replaced with the value of the <tt>HOME</tt> environment variable
 * (if it exists), and <tt>~username/</tt> is replaced with the given user's
 * home directory as returned by getpwnam() (if the given user exists).
 * Additionally, double slashes are replaced by single slashes.  Thus,
 * <tt>"~//myfile.txt~"</tt> might parse to
 * <tt>"/home/kohler/myfile.txt~"</tt>.  Empty strings are not accepted.  If
 * the string fully parses, then the result is stored in *@a result and the
 * function returns true.  Otherwise, *@a result remains unchanged and the
 * function returns false.
 *
 * This function is only available at user level.
 */
bool
cp_filename(const String &str, String *result)
{
    return FilenameArg::parse(str, *result);
}
#endif


#if !CLICK_TOOL
/** @brief Parse a packet annotation value from @a str.
 * @param  str  string
 * @param  size  annotation size, or <= 0 to not check size
 * @param[out]  result  stores parsed result
 * @param  context  element context
 * @return  True if @a str parsed correctly, false otherwise.
 *
 * Parses a packet annotation value from @a str.  This is either a predefined
 * annotation name, such as PAINT or ICMP_PARAM_PROB (all names from
 * <click/packet_anno.hh> are defined by default); a user-defined name (see
 * the AnnotationInfo element); or an integer indicating the byte offset into
 * the annotation area.
 *
 * If @a size <= 0, then the annotation value is returned as is.  Use the
 * #ANNOTATIONINFO_OFFSET and #ANNOTATIONINFO_SIZE macros to extract the
 * offset and size portions.  If @a size > 0, then the annotation value is
 * checked to ensure that the sizes are compatible.  For instance, if @a size
 * == 1, then the "DST_IP" annotation is rejected since it has the wrong size.
 * In this case, the value stored in *@a result equals the annotation offset
 * -- the size is masked off.
 *
 * Values that extend past the end of the annotation area are rejected.
 *
 * If the string fully parses, then the result is stored in *@a result and the
 * function returns true.  Otherwise, *@a result remains unchanged and the
 * function returns false.
 */
bool
cp_anno(const String &str, int size, int *result  CP_CONTEXT)
{
    return AnnoArg(size).parse(str, *result, Args(context));
}
#endif


//
// CP_VA_PARSE AND FRIENDS
//

// parse commands; those which must be recognized inside a keyword section
// must begin with "\377"
#undef cpEnd

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
  cpSize		= "size_t",
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
  cpTimestamp		= "timestamp",
  cpTimestampSigned	= "timestamp_signed",
  cpTimeval		= "timeval",
  cpBandwidth		= "bandwidth_Bps",
  cpIPAddress		= "ip_addr",
  cpIPPrefix		= "ip_prefix",
  cpIPAddressOrPrefix	= "ip_addr_or_prefix",
  cpIPAddressList	= "ip_addr_list",
  cpEthernetAddress	= "ether_addr",
  cpEtherAddress	= cpEthernetAddress, // synonym
  cpTCPPort		= "tcp_port",
  cpUDPPort		= "udp_port",
  cpElement		= "element",
  cpElementCast		= "element_cast",
  cpHandlerName		= "handler_name",
  cpHandlerCallRead	= "handler_call_read",
  cpHandlerCallWrite	= "handler_call_write",
  cpHandlerCallPtrRead	= "handler_call_ptr_read",
  cpHandlerCallPtrWrite	= "handler_call_ptr_write",
  cpIP6Address		= "ip6_addr",
  cpIP6Prefix		= "ip6_prefix",
  cpIP6PrefixLen	= "ip6_prefix_len",
  cpIP6AddressOrPrefix	= "ip6_addr_or_prefix",
  cpFilename		= "filename",
#if !CLICK_TOOL
  cpAnno		= "anno",
#endif
  cpInterval		= cpTimeval,
  cpReadHandlerCall	= cpHandlerCallPtrRead,
  cpWriteHandlerCall	= cpHandlerCallPtrWrite;

enum {
  cpiEnd = 0,
  cpiOptional,
  cpiKeywords,
  cpiConfirmKeywords,
  cpiMandatoryKeywords,
  cpiIgnore,
  cpiIgnoreRest,
  cpiLastMagic = cpiIgnoreRest, // everything before here is a magic command
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
  cpiSize,
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
  cpiTimestamp,
  cpiTimestampSigned,
  cpiTimeval,
  cpiBandwidth,
  cpiIPAddress,
  cpiIPPrefix,
  cpiIPAddressOrPrefix,
  cpiIPAddressList,
  cpiEthernetAddress,
  cpiTCPPort,
  cpiUDPPort,
  cpiElement,
  cpiElementCast,
  cpiHandlerName,
  cpiHandlerCallRead,
  cpiHandlerCallWrite,
  cpiHandlerCallPtrRead,
  cpiHandlerCallPtrWrite,
  cpiIP6Address,
  cpiIP6Prefix,
  cpiIP6PrefixLen,
  cpiIP6AddressOrPrefix,
  cpiFilename,
  cpiAnno
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


static int type_mismatch(ErrorHandler *errh, cp_value *v, const char *argname, const String &, const char *type_description = 0)
{
    if (!type_description)
	type_description = v->argtype->description;
    return errh->error("type mismatch: %s requires %s", argname, type_description);
}

static void
default_parsefunc(cp_value *v, const String &arg,
		  ErrorHandler *errh, const char *argname  CP_CONTEXT)
{
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
      goto type_mismatch;
    break;

   case cpiWord:
    if (!cp_word(arg, &v->v_string))
      goto type_mismatch;
    break;

   case cpiKeyword:
    if (!cp_keyword(arg, &v->v_string))
      goto type_mismatch;
    break;

   case cpiBool:
    if (!BoolArg::parse(arg, v->v.b))
      goto type_mismatch;
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
      if (NameInfo::query(v->extra.i, context, arg, &v->v.s32, 4))
	  break;
#endif
      goto handle_int32_t;

  handle_signed: {
	  IntArg ia;
	  if (!ia.parse_saturating(arg, v->v.s32))
	      goto type_mismatch;
	  else if (ia.status == IntArg::status_range
		   || v->v.s32 > int32_t(overflower)
		   || v->v.s32 < underflower)
	      errh->error("%s out of range, bound %ld", argname, (long) v->v.s32);
	  break;
      }

  handle_unsigned: {
	  IntArg ia;
	  if (!ia.parse_saturating(arg, v->v.u32))
	      goto type_mismatch;
	  else if (ia.status == IntArg::status_range || v->v.u32 > overflower)
	      errh->error("%s out of range, bound %lu", argname, (unsigned long) v->v.u32);
	  break;
      }

#ifdef HAVE_INT64_TYPES
  case cpiInteger64: {
      IntArg ia;
      if (!ia.parse_saturating(arg, v->v.s64))
	  goto type_mismatch;
      else if (ia.status == IntArg::status_range)
	  errh->error("%s out of range, bound %^64d", argname, v->v.s64);
      break;
  }

  case cpiUnsigned64: {
      IntArg ia;
      if (!ia.parse_saturating(arg, v->v.u64))
	  goto type_mismatch;
      else if (ia.status == IntArg::status_range)
	  errh->error("%s out of range, bound %^64u", argname, v->v.u64);
      break;
  }
#endif

  case cpiSize: {
      IntArg ia;
      if (!ia.parse_saturating(arg, v->v.size))
	  goto type_mismatch;
      else if (ia.status == IntArg::status_range)
	  errh->error("%s out of range, bound %zu", argname, v->v.size);
      break;
  }

  case cpiReal10: {
      DecimalFixedPointArg dfpa(v->extra.i);
      if (!dfpa.parse_saturating(arg, v->v.s32))
	  goto type_mismatch;
      else if (dfpa.status == dfpa.status_range)
	  errh->error("%s out of range", argname);
      break;
  }

  case cpiUnsignedReal10: {
      DecimalFixedPointArg dfpa(v->extra.i);
      if (!dfpa.parse_saturating(arg, v->v.u32))
	  goto type_mismatch;
      else if (dfpa.status == dfpa.status_range)
	  errh->error("%s out of range", argname);
      break;
  }

#ifdef HAVE_FLOAT_TYPES
  case cpiDouble: {
      DoubleArg da;
      if (!da.parse(arg, v->v.d))
	  goto type_mismatch;
      else if (da.status == DoubleArg::status_range)
	  errh->error("%s out of range, bound %g", argname, v->v.d);
      break;
  }
#endif

  case cpiSeconds: {
      SecondsArg sa;
      if (!sa.parse_saturating(arg, v->v.u32))
	  goto type_mismatch;
      else if (sa.status == sa.status_range)
	  errh->error("%s out of range, bound %u", argname, v->v.u32);
      break;
  }

  case cpiSecondsAsMilli: {
      SecondsArg sa(3);
      if (!sa.parse_saturating(arg, v->v.u32))
	  goto type_mismatch;
      else if (sa.status == sa.status_range) {
	  String m = cp_unparse_milliseconds(v->v.u32);
	  errh->error("%s out of range, bound %s", argname, m.c_str());
      }
      break;
  }

  case cpiSecondsAsMicro: {
      SecondsArg sa(6);
      if (!sa.parse_saturating(arg, v->v.u32))
	  goto type_mismatch;
      else if (sa.status == sa.status_range) {
	  String m = cp_unparse_microseconds(v->v.u32);
	  errh->error("%s out of range, bound %s", argname, m.c_str());
      }
      break;
  }

  case cpiTimestamp:
  case cpiTimestampSigned: {
     Timestamp t;
     if (!cp_time(arg, &t, argtype->internal == cpiTimestampSigned)) {
       if (cp_errno == CPE_NEGATIVE)
	 errh->error("%s must be >= 0", argname);
       else
	 goto type_mismatch;
     } else if (cp_errno == CPE_OVERFLOW)
       errh->error("%s out of range", argname);
     else {
       v->v.s32 = t.sec();
       v->v2.s32 = t.subsec();
     }
     break;
   }

   case cpiTimeval: {
     struct timeval tv;
     if (!cp_time(arg, &tv)) {
       if (cp_errno == CPE_NEGATIVE)
	 errh->error("%s must be >= 0", argname);
       else
	 goto type_mismatch;
     } else if (cp_errno == CPE_OVERFLOW)
       errh->error("%s out of range", argname);
     else {
       v->v.s32 = tv.tv_sec;
       v->v2.s32 = tv.tv_usec;
     }
     break;
   }

  case cpiBandwidth: {
      BandwidthArg ba;
      if (!ba.parse(arg, v->v.u32))
	  goto type_mismatch;
      else if (ba.status == NumArg::status_range) {
	  String m = cp_unparse_bandwidth(v->v.u32);
	  errh->error("%s out of range, bound %s", argname, m.c_str());
      } else if (ba.status == NumArg::status_unitless)
	  errh->warning("no units on bandwidth %s, assuming Bps", argname);
      break;
  }

  case cpiReal2: {
      FixedPointArg fpa(v->extra.i);
      if (!fpa.parse_saturating(arg, v->v.s32))
	  goto type_mismatch;
      else if (fpa.status == NumArg::status_range) {
	  String m = cp_unparse_real2(v->v.s32, v->extra.i);
	  errh->error("%s out of range, bound %s", argname, m.c_str());
      }
      break;
  }

  case cpiUnsignedReal2: {
      FixedPointArg fpa(v->extra.i);
      if (!fpa.parse_saturating(arg, v->v.u32))
	  goto type_mismatch;
      else if (fpa.status == NumArg::status_range) {
	  String m = cp_unparse_real2(v->v.u32, v->extra.i);
	  errh->error("%s out of range, bound %s", argname, m.c_str());
      }
      break;
  }

   case cpiIPAddress:
    if (!cp_ip_address(arg, v->v.address CP_PASS_CONTEXT))
      goto type_mismatch;
    break;

   case cpiIPPrefix:
   case cpiIPAddressOrPrefix: {
     bool mask_optional = (argtype->internal == cpiIPAddressOrPrefix);
     if (!cp_ip_prefix(arg, v->v.address, v->v2.address, mask_optional CP_PASS_CONTEXT))
       goto type_mismatch;
     break;
   }

   case cpiIPAddressList: {
     Vector<IPAddress> l;
     if (!cp_ip_address_list(arg, &l CP_PASS_CONTEXT))
       goto type_mismatch;
     break;
   }

#ifdef HAVE_IP6
   case cpiIP6Address:
    if (!cp_ip6_address(arg, (unsigned char *)v->v.address CP_PASS_CONTEXT))
      goto type_mismatch;
    break;

   case cpiIP6Prefix:
   case cpiIP6AddressOrPrefix: {
     bool mask_optional = (argtype->internal == cpiIP6AddressOrPrefix);
     if (!cp_ip6_prefix(arg, v->v.address, v->v2.address, mask_optional CP_PASS_CONTEXT))
       goto type_mismatch;
     break;
   }

  case cpiIP6PrefixLen:
      if (!cp_ip6_prefix(arg, v->v.address, &v->v2.i, false CP_PASS_CONTEXT))
	  goto type_mismatch;
      break;
#endif

   case cpiEthernetAddress:
    if (!cp_ethernet_address(arg, v->v.address CP_PASS_CONTEXT))
      goto type_mismatch;
    break;

   case cpiTCPPort:
    if (!cp_tcpudp_port(arg, IP_PROTO_TCP, &v->v.u16 CP_PASS_CONTEXT))
      goto type_mismatch;
    break;

   case cpiUDPPort:
    if (!cp_tcpudp_port(arg, IP_PROTO_UDP, &v->v.u16 CP_PASS_CONTEXT))
      goto type_mismatch;
    break;

#ifndef CLICK_TOOL
  case cpiElement:
      v->v.element = cp_element(arg, context, errh, argname);
      break;

  case cpiElementCast: {
      Element *e = cp_element(arg, context, errh, argname);
      v->v.p = 0;
      if (e && !(v->v.p = e->cast(v->extra.c_str)))
	  errh->error("%s type mismatch, expected %s", argname, v->extra.c_str);
      break;
  }

   case cpiHandlerName: {
     ContextErrorHandler cerrh(errh, "%s:", argname);
     cp_handler_name(arg, &v->v.element, &v->v2_string, context, &cerrh);
     break;
   }

   case cpiHandlerCallRead:
   case cpiHandlerCallPtrRead:
    underflower = HandlerCall::h_read | HandlerCall::h_preinitialize;
    goto handler_call;

   case cpiHandlerCallWrite:
   case cpiHandlerCallPtrWrite:
    underflower = HandlerCall::h_write | HandlerCall::h_preinitialize;
    goto handler_call;

   handler_call: {
     ContextErrorHandler cerrh(errh, "%s:", argname);
     HandlerCall garbage(arg);
     garbage.initialize(underflower, context, &cerrh);
     break;
   }
#endif

#if CLICK_USERLEVEL || CLICK_TOOL
  case cpiFilename:
      if (!FilenameArg::parse(arg, v->v_string))
	  goto type_mismatch;
      break;

   case cpiFileOffset:
    if (!cp_file_offset(arg, (off_t *) &v->v))
      goto type_mismatch;
    break;
#endif

#if !CLICK_TOOL
    case cpiAnno:
      if (!cp_anno(arg, v->extra.i, &v->v.s32  CP_PASS_CONTEXT))
	  goto type_mismatch;
      break;
#endif

   type_mismatch:
    type_mismatch(errh, v, argname, arg);
    break;

  }
}

static void
default_storefunc(cp_value *v  CP_CONTEXT)
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
     *ucstore = v->v.s32;
     break;
   }

   case cpiShort: {
     short *sstore = (short *)v->store;
     *sstore = v->v.s32;
     break;
   }

   case cpiUnsignedShort: {
     unsigned short *usstore = (unsigned short *)v->store;
     *usstore = v->v.u32;
     break;
   }

   case cpiTCPPort:
   case cpiUDPPort: {
     uint16_t *u16store = (uint16_t *)v->store;
     *u16store = v->v.u16;
     break;
   }

   case cpiInteger:
   case cpiNamedInteger:
   case cpiReal2:
   case cpiReal10:
   case cpiSeconds:
   case cpiSecondsAsMilli:
   case cpiSecondsAsMicro:
   case cpiBandwidth:
   case cpiAnno: {
     int *istore = (int *)v->store;
     *istore = v->v.s32;
     break;
   }

   case cpiUnsigned:
   case cpiUnsignedReal2:
   case cpiUnsignedReal10: {
     unsigned *ustore = (unsigned *)v->store;
     *ustore = v->v.u32;
     break;
   }

#if HAVE_INT64_TYPES
   case cpiInteger64: {
     int64_t *llstore = (int64_t *)v->store;
     *llstore = v->v.s64;
     break;
   }

   case cpiUnsigned64: {
     uint64_t *ullstore = (uint64_t *)v->store;
     *ullstore = v->v.u64;
     break;
   }
#endif

  case cpiSize: {
      size_t *sizestore = (size_t *) v->store;
      *sizestore = v->v.size;
      break;
  }

#if CLICK_USERLEVEL || CLICK_TOOL
   case cpiFileOffset: {
     off_t *offstore = (off_t *)v->store;
     *offstore = *((off_t *)&v->v);
     break;
   }
#endif

#if HAVE_FLOAT_TYPES
   case cpiDouble: {
     double *dstore = (double *)v->store;
     *dstore = v->v.d;
     break;
   }
#endif

  case cpiTimestamp:
  case cpiTimestampSigned: {
     Timestamp *tstore = (Timestamp *)v->store;
     *tstore = Timestamp(v->v.s32, v->v2.s32);
     break;
   }

   case cpiTimeval: {
     struct timeval *tvstore = (struct timeval *)v->store;
     tvstore->tv_sec = v->v.s32;
     tvstore->tv_usec = v->v2.s32;
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

#if HAVE_IP6
   case cpiIP6Address:
    helper = 16;
    goto address;
#endif

   case cpiEthernetAddress:
    helper = 6;
    goto address;

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

#if HAVE_IP6
   case cpiIP6Prefix:
   case cpiIP6AddressOrPrefix: {
     unsigned char *addrstore = (unsigned char *)v->store;
     memcpy(addrstore, v->v.address, 16);
     unsigned char *maskstore = (unsigned char *)v->store2;
     memcpy(maskstore, v->v2.address, 16);
     break;
   }

  case cpiIP6PrefixLen: {
      unsigned char *addrstore = (unsigned char *)v->store;
      memcpy(addrstore, v->v.address, 16);
      int *prefixlenstore = (int *)v->store2;
      *prefixlenstore = v->v2.i;
      break;
  }
#endif

   case cpiIPAddressList: {
     // oog... parse set into stored set only when we know there are no errors
     Vector<IPAddress> *liststore = (Vector<IPAddress> *)v->store;
     cp_ip_address_list(v->v_string, liststore  CP_PASS_CONTEXT);
     break;
   }

#if !CLICK_TOOL
   case cpiElement: {
     Element **elementstore = (Element **)v->store;
     *elementstore = v->v.element;
     break;
   }

  case cpiElementCast: {
      void **caststore = (void **) v->store;
      *caststore = v->v.p;
      break;
  }

   case cpiHandlerName: {
     Element **elementstore = (Element **)v->store;
     String *hnamestore = (String *)v->store2;
     *elementstore = v->v.element;
     *hnamestore = v->v2_string;
     break;
   }

   case cpiHandlerCallRead:
    helper = HandlerCall::h_read | HandlerCall::h_preinitialize;
    goto handler_call;

   case cpiHandlerCallWrite:
    helper = HandlerCall::h_write | HandlerCall::h_preinitialize;
    goto handler_call;

   handler_call: {
	HandlerCall *hc = static_cast<HandlerCall *>(v->store);
	*hc = HandlerCall(v->v_string);
	hc->initialize(helper, context);
	break;
    }

   case cpiHandlerCallPtrRead:
    helper = HandlerCall::h_read | HandlerCall::h_preinitialize;
    goto handler_call_ptr;

   case cpiHandlerCallPtrWrite:
    helper = HandlerCall::h_write | HandlerCall::h_preinitialize;
    goto handler_call_ptr;

   handler_call_ptr:
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
		     ErrorHandler *errh, const char *argname  CP_CONTEXT)
{
    const cp_argtype *argtype = v->argtype;
#ifndef CLICK_TOOL
    (void) context;
#endif

    if (HashTable<String, int> *m = reinterpret_cast<HashTable<String, int> *>(argtype->user_data)) {
	String word;
	if (cp_word(arg, &word))
	    if (HashTable<String, int>::iterator it = m->find(word)) {
		v->v.s32 = it.value();
		return;
	    }
    }

    if (argtype->flags & cpArgAllowNumbers) {
	if (!cp_integer(arg, &v->v.s32))
	    errh->error("%s has type %s", argname, argtype->description);
	else if (cp_errno == CPE_OVERFLOW)
	    errh->error("%s out of range, bound %d", argname, v->v.s32);
    } else
	errh->error("%s has type %s", argname, argtype->description);
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
    HashTable<String, int> *m = reinterpret_cast<HashTable<String, int> *>(t->user_data);
    if (!m)
	t->user_data = m = new HashTable<String, int>();
    if (!m)
	return -ENOMEM;

    va_list val;
    va_start(val, name);
    const char *s;
    int retval = 0;
    while ((s = va_arg(val, const char *))) {
	int value = va_arg(val, int);
	if (cp_is_word(s))
	    m->set(String(s), value);
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
		delete reinterpret_cast<HashTable<String, int> *>(trav->user_data);
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

enum {
    cpkSupplied = cpkDeprecated * 2
};

static int
handle_special_argtype_for_keyword(cp_value *val, const String &rest)
{
  if (val->argtype->internal == cpiArguments) {
    if (val->v.s32 & cpkSupplied) {
      uint32_t l = val->v_string.length();
      val->v2_string += String((const char *)&l, 4);
      val->v_string += rest;
    } else {
      val->v.s32 |= cpkSupplied;
      val->v_string = rest;
      val->v2_string = String();
    }
    return kwSuccess;
  } else {
    assert(0);
    return kwUnkKeyword;
  }
}


/** @cond never */
namespace {

struct CpVaHelper {

  CpVaHelper(struct cp_value *, int, bool keywords_only);

  int develop_values(va_list val, ErrorHandler *errh);
  int develop_kvalues(va_list val, ErrorHandler *errh);

  int assign_keyword_argument(const String &);
  void add_keyword_error(StringAccum &, int err, const String &arg, const char *argname, int argno);
  int finish_keyword_error(const char *format, const char *bad_keywords, ErrorHandler *errh);

  int assign_arguments(const Vector<String> &args, const char *argname, ErrorHandler *errh);

  int parse_arguments(const char *argname  CP_CONTEXT, ErrorHandler *errh);

  const char *value_name(int i);

  bool keywords_only;
  int nvalues;
  int nrequired;
  int npositional;
  bool ignore_rest;

  struct cp_value *cp_values;
  int cp_values_size;

  String temp_string;

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
    v->argtype = argtype;
    v->v.s32 = (mandatory_keywords || (nrequired < 0 && npositional < 0)
		? cpkMandatory : 0);

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
      nvalues++;
      continue;
    } else if (argtype->internal == cpiIgnoreRest) {
      if (nrequired < 0)
	nrequired = nvalues;
      ignore_rest = true;
      goto done;
    }

    // store stuff in v
    v->description = va_arg(val, const char *); // NB deprecated
    if (argtype->flags & cpArgExtraInt)
	v->extra.i = va_arg(val, int);
    else if (argtype->flags & cpArgExtraCStr) {
	if (!(v->extra.c_str = va_arg(val, const char *)))
	    return errh->error("missing extra parameter");
    }
    if (confirm_keywords) {
	v->store_confirm = va_arg(val, bool *);
	*v->store_confirm = false;
    } else
	v->store_confirm = 0;
    v->store = va_arg(val, void *);
    if (argtype->flags & cpArgStore2)
      v->store2 = va_arg(val, void *);
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
CpVaHelper::develop_kvalues(va_list val, ErrorHandler *errh)
{
  if (!cp_values || !cp_parameter_used)
    return errh->error("out of memory in cp_va_kparse");

  while (1) {

    if (nvalues == cp_values_size - 1)
      // no more space to store information about the arguments; break
      return errh->error("too many arguments to cp_va_kparse!");

    cp_value *v = &cp_values[nvalues];
    v->argtype = 0;
    v->keyword = va_arg(val, const char *);
    if (v->keyword == 0)
	goto done;
    else if (v->keyword[0] == '\377' && strcmp(v->keyword, cpIgnoreRest) == 0) {
	ignore_rest = true;
	goto done;
    }
    int flags = va_arg(val, int);
    if ((flags & cpkPositional) && npositional >= 0)
	return errh->error("%s: positional arguments must be grouped at the beginning", v->keyword);
    if ((flags & (cpkPositional | cpkMandatory)) == (cpkPositional | cpkMandatory)
	&& nrequired >= 0)
	return errh->error("%s: mandatory positional arguments must precede optional ones", v->keyword);
    if (npositional < 0 && !(flags & cpkPositional))
	npositional = nvalues;
    if (nrequired < 0 && (flags & (cpkPositional | cpkMandatory)) != (cpkPositional | cpkMandatory))
	nrequired = nvalues;
    if (flags & cpkConfirm) {
	v->store_confirm = va_arg(val, bool *);
	*v->store_confirm = false;
    } else
	v->store_confirm = 0;

    // find cp_argtype
    const char *command_name = va_arg(val, const char *);
    const cp_argtype *argtype = cp_find_argtype(command_name);
    if (!argtype)
      return errh->error("unknown argument type %<%s%>!", command_name);
    v->argtype = argtype;
    v->v.s32 = (flags & (cpkMandatory | cpkDeprecated));

    // check for special commands
    if (argtype->internal == cpiIgnore) {
	nvalues++;
	continue;
    } else if (argtype->internal >= 0 && argtype->internal <= cpiLastMagic)
	return errh->error("%s: bad magic command in cp_va_kparse", command_name + 1);

    // store stuff in v
    v->description = "?"; // NB deprecated
    if (argtype->flags & cpArgExtraInt)
	v->extra.i = va_arg(val, int);
    else if (argtype->flags & cpArgExtraCStr) {
	if (!(v->extra.c_str = va_arg(val, const char *)))
	    return errh->error("missing extra parameter");
    }
    v->store = va_arg(val, void *);
    if (argtype->flags & cpArgStore2)
      v->store2 = va_arg(val, void *);
    nvalues++;
  }

 done:
  if (npositional < 0)
      npositional = nvalues;
  nrequired = 0;
  return 0;
}


int
CpVaHelper::assign_keyword_argument(const String &arg)
{
  String keyword, rest;
  // extract keyword
  if (!cp_keyword(arg, &keyword, &rest))
    return kwNoKeyword;
  // look for keyword value
  for (int i = 0; i < nvalues; i++)
    if (cp_values[i].keyword && keyword == cp_values[i].keyword) {
      cp_value *val = &cp_values[i];
      // handle special types differently
      if (special_argtype_for_keyword(val->argtype))
	return handle_special_argtype_for_keyword(val, rest);
      else {
	// do not report error if keyword used already
	// if (cp_values[i].v.s32 & cpkSupplied)
	//   return kwDupKeyword;
	val->v.s32 |= cpkSupplied;
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
    for (int i = 0; i < nvalues; i++)
	if (cp_values[i].keyword) {
	    if (keywords_sa.length())
		keywords_sa << ", ";
	    keywords_sa << cp_values[i].keyword;
	}
    errh->error(format, bad_keywords, keywords_sa.c_str());
    return -EINVAL;
}

const char *
CpVaHelper::value_name(int i)
{
    if (cp_values[i].keyword)
	return cp_values[i].keyword;
    else if (const cp_argtype *t = cp_values[i].argtype)
	return t->description;
    else {
	temp_string = String("#") + String(i);
	return temp_string.c_str();
    }
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
      cp_values[npositional_supplied].v.s32 |= cpkSupplied;
    }
    npositional_supplied++;
  }

  // report keyword argument errors
  if (keyword_error_sa.length() && !keywords_only)
    return finish_keyword_error("bad keyword(s) %s\n(valid keywords are %s)", keyword_error_sa.c_str(), errh);

  // report missing mandatory keywords and uses of deprecated arguments
  int nmissing = 0;
  for (int i = 0; i < nvalues; i++)
    if ((cp_values[i].v.s32 & (cpkMandatory | cpkSupplied)) == cpkMandatory) {
      nmissing++;
      if (keyword_error_sa.length())
	  keyword_error_sa << ", ";
      keyword_error_sa << value_name(i);
    } else if ((cp_values[i].v.s32 & (cpkDeprecated | cpkSupplied)) == (cpkDeprecated | cpkSupplied)) {
	errh->warning("%s %s is deprecated", value_name(i), argname);
    } else if (!(cp_values[i].v.s32 & cpkSupplied))
	// clear 'argtype' on unused arguments
	cp_values[i].argtype = 0;

  if (nmissing)
      return errh->error("missing mandatory %s %s%s", keyword_error_sa.c_str(), argname, (nmissing > 1 ? "s" : ""));

  // if wrong number of arguments, print signature
  if (npositional_supplied > npositional && !ignore_rest)
      return errh->error("too many %ss", argname);

  return 0;
}

int
CpVaHelper::parse_arguments(const char *argname,
#if !CLICK_TOOL
			    const Element *context,
#endif
			    ErrorHandler *errh)
{
  int nerrors_in = errh->nerrors();

  // parse arguments
  char argname_buf[128];
  int argname_offset;
  argname_offset = sprintf(argname_buf, "%s ", argname);

  for (int i = 0; i < nvalues; i++) {
      cp_value *v = &cp_values[i];
      if (!v->argtype)
	  continue;
      if (v->keyword)
	  v->argtype->parse(v, v->v_string, errh, v->keyword  CP_PASS_CONTEXT);
      else {
	  sprintf(argname_buf + argname_offset, "%d", i + 1);
	  v->argtype->parse(v, v->v_string, errh, argname_buf  CP_PASS_CONTEXT);
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
/** @endcond never */


/// @brief Legacy function for parsing a list of arguments.
/// @param conf argument list
/// @param context element context
/// @param errh error handler
/// @param ... zero or more parameter descriptions, terminated by cpEnd
/// @deprecated Use cp_va_kparse() instead.
///
/// Older versions of Click used cp_va_parse() instead of the current
/// cp_va_kparse().  This guide shows how to transition cp_va_parse()
/// calls into cp_va_kparse().
///
/// There are two major differences between the variants.  First,
/// <em>every</em> argument to cp_va_kparse() must have a keyword name,
/// including arguments that are normally specified by position.  Second, each
/// cp_va_parse() argument includes an "argument description string" used to
/// improve error messages.  cp_va_kparse() arguments do not take such a
/// string; the keyword generates better, more concise messages.
///
/// The following examples, taken from Click elements, show how to change
/// concrete cp_va_parse() calls into cp_va_kparse() calls.
///
/// @code
/// // 1. Paint: Mandatory arguments are marked with cpkP+cpkM.
/// //    Element documentation and other analogous elements are
/// //    good places to look for keywords.
/// ... cp_va_parse(conf, this, errh,
///                 cpByte, "color", &_color,    // "color" is the description string
///                 cpEnd) ...
///    /* => */
/// ... cp_va_kparse(conf, this, errh,
///                  "COLOR", cpkP+cpkM, cpByte, &_color,
///                  cpEnd) ...
///
/// // 2. Switch: Optional arguments are marked with cpkP (no cpkM).
/// ... cp_va_parse(conf, this, errh,
///                 cpOptional,
///                 cpInteger, "active output", &_output,
///                 cpEnd) ...
///    /* => */
/// ... cp_va_kparse(conf, this, errh,
///                  "OUTPUT", cpkP, cpInteger, &_output,
///                  cpEnd) ...
///
/// // 3. Counter: Keywords are marked with cpkN (or, equivalently, 0).
/// ... cp_va_parse(conf, this, errh,
///                 cpKeywords,
///                 "COUNT_CALL", cpArgument, "handler to call after a count", &count_call,
///                 "BYTE_COUNT_CALL", cpArgument, "handler to call after a byte count", &byte_count_call,
///                 cpEnd) ...
///    /* => */
/// ... cp_va_kparse(conf, this, errh,
///                  "COUNT_CALL", cpkN, cpArgument, &count_call,
///                  "BYTE_COUNT_CALL", cpkN, cpArgument, &byte_count_call,
///                  cpEnd) ...
///
/// // 4. IPFragmenter: Combining all of the above.
/// ... cp_va_parse(conf, this, errh,
///                 cpUnsigned, "MTU", &_mtu,
///                 cpOptional,
///                 cpBool, "HONOR_DF", &_honor_df,
///                 cpKeywords,
///                 "HONOR_DF", cpBool, "honor DF bit?", &_honor_df,
///                 "VERBOSE", cpBool, "be verbose?", &_verbose,
///                 cpEnd) ...
///    /* => */
/// ... cp_va_kparse(conf, this, errh,
///                  "MTU", cpkP+cpkM, cpUnsigned, &_mtu,
///                  "HONOR_DF", cpkP, cpBool, &_honor_df,   // NB only one HONOR_DF
///                  "VERBOSE", cpkN, cpBool, &_verbose,
///                  cpEnd) ...
///
/// // 5. AggregateIPFlows: Confirmed keywords are marked with cpkC.
/// ... cp_va_parse(conf, this, errh,
///                 "TCP_TIMEOUT", cpSeconds, "timeout for active TCP connections", &_tcp_timeout, ...,
///                 cpConfirmKeywords,
///                 "FRAGMENTS", cpBool, "handle fragmented packets?", &gave_fragments, &fragments,
///                 cpEnd) ...
///    /* => */
/// ... cp_va_kparse(conf, this, errh,
///                  "TCP_TIMEOUT", cpkN, cpSeconds, &_tcp_timeout, ...,
///                  "FRAGMENTS", cpkC, &gave_fragments, cpBool, &fragments,  // Note different order
///                  cpEnd) ...
/// @endcode
int
cp_va_parse(const Vector<String> &conf,
#if !CLICK_TOOL
	    const Element *context,
#endif
	    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(conf, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

/// @brief Legacy function for parsing a comma-separated argument string.
/// @param str comma-separated argument string
/// @param context element context
/// @param errh error handler
/// @param ... zero or more parameter descriptions, terminated by cpEnd
/// @deprecated Use cp_va_kparse() instead.  See
/// cp_va_parse(const Vector<String> &, Element *, ErrorHandler *, ...) for a
/// transition guide.
int
cp_va_parse(const String &str,
#if !CLICK_TOOL
	    const Element *context,
#endif
	    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> argv;
  cp_argvec(str, argv);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(argv, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

/// @brief Legacy function for parsing a space-separated argument string.
/// @param str space-separated argument string
/// @param context element context
/// @param errh error handler
/// @param ... zero or more parameter descriptions, terminated by cpEnd
/// @deprecated Use cp_va_space_kparse() instead.  See
/// cp_va_parse(const Vector<String> &, Element *, ErrorHandler *, ...) for a
/// transition guide.
int
cp_va_space_parse(const String &str,
#if !CLICK_TOOL
		  const Element *context,
#endif
		  ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> argv;
  cp_spacevec(str, argv);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(argv, "word", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("word"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

/// @brief Legacy function for parsing a single argument.
/// @param str argument
/// @param context element context
/// @param errh error handler
/// @param ... zero or more parameter descriptions, terminated by cpEnd
/// @deprecated Use cp_va_kparse_keyword() instead.  See
/// cp_va_parse(const Vector<String> &, Element *, ErrorHandler *, ...) for a
/// transition guide.
int
cp_va_parse_keyword(const String &str,
#if !CLICK_TOOL
		    const Element *context,
#endif
		    ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> argv;
  argv.push_back(str);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, true);
  int retval = cpva.develop_values(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(argv, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

/// @brief Legacy function for parsing and removing matching arguments from
///   @a conf.
/// @param conf argument list
/// @param first index of first non-mandatory argument
/// @param context element context
/// @param errh error handler
/// @param ... zero or more parameter descriptions, terminated by cpEnd
/// @deprecated Use cp_va_kparse_remove_keywords() instead.  See
/// cp_va_parse(const Vector<String> &, Element *, ErrorHandler *, ...) for a
/// transition guide.  Note that cp_va_kparse_remove_keywords() does not take
/// the @a first argument; simply leave it off.
int
cp_va_parse_remove_keywords(Vector<String> &conf, int first,
#if !CLICK_TOOL
			    const Element *context,
#endif
			    ErrorHandler *errh, ...)
{
  Vector<String> conf2, *confp = &conf;
  if (first > 0) {
    for (int i = first; i < conf.size(); i++)
      conf2.push_back(conf[i]);
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
    for (int i = first; i < conf.size(); i++)
      if ((*cp_parameter_used)[i - first])
	delta++;
      else if (delta)
	conf[i - delta] = conf[i];
    conf.resize(conf.size() - delta);
  }

  return retval;
}


/** @brief Parse a list of arguments.
 * @param  conf  argument list
 * @param  context  element context
 * @param  errh  error handler
 * @param  ...  zero or more parameter items, terminated by ::cpEnd
 * @return  The number of parameters successfully assigned, or negative on error.
 *
 * The arguments in @a conf are parsed according to the items.  Each supplied
 * argument must match one of the items, and at least one argument must match
 * each mandatory item.  Any errors are reported to @a errh.  If no error
 * occurs, then the item results are assigned appropriately, and the function
 * returns the number of assigned items, which might be 0.  If any error
 * occurs, then the item results are left unchanged and the function returns a
 * negative error code.
 *
 * The @a context argument is passed to any parsing functions that require
 * element context.  See above for more information on cp_va_kparse() items.
 *
 * The item list must be terminated with cpEnd.  An error message such as
 * "warning: missing sentinel in function call" indicates that you terminated
 * the list with 0 instead.  Fix it by replacing the 0 with cpEnd.
 */
int
cp_va_kparse(const Vector<String> &conf,
#if !CLICK_TOOL
	     const Element *context,
#endif
	     ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_kvalues(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(conf, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

/** @brief Parse a comma-separated argument string.
 * @param  str  comma-separated argument string
 * @param  context  element context
 * @param  errh  error handler
 * @param  ...  zero or more parameter items, terminated by ::cpEnd
 * @return  The number of parameters successfully assigned, or negative on error.
 *
 * The argument string is separated into an argument list by cp_argvec(),
 * after which the function behaves like cp_va_kparse(const Vector<String>&,
 * Element *, ErrorHandler *, ...).
 */
int
cp_va_kparse(const String &str,
#if !CLICK_TOOL
	     const Element *context,
#endif
	     ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> argv;
  cp_argvec(str, argv);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_kvalues(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(argv, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

/** @brief Parse a space-separated argument string.
 * @param  str  space-separated argument string
 * @param  context  element context
 * @param  errh  error handler
 * @param  ...  zero or more parameter items, terminated by ::cpEnd
 * @return  The number of parameters successfully assigned, or negative on error.
 *
 * The argument string is separated into an argument list by cp_spacevec(),
 * after which the function behaves like cp_va_kparse(const Vector<String>&,
 * Element *, ErrorHandler *, ...).
 */
int
cp_va_space_kparse(const String &str,
#if !CLICK_TOOL
		   const Element *context,
#endif
		   ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> conf;
  cp_spacevec(str, conf);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  int retval = cpva.develop_kvalues(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(conf, "word", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("word"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

/** @brief Parse a single argument.
 * @param  str  argument
 * @param  context  element context
 * @param  errh  error handler
 * @param  ...  zero or more parameter items, terminated by ::cpEnd
 * @return  The number of parameters successfully assigned (0 or 1), or negative on error.
 *
 * An argument list consisting of the single argument @a str is formed,
 * after which this function behaves like cp_va_kparse(const Vector<String>&,
 * Element *, ErrorHandler *, ...).
 */
int
cp_va_kparse_keyword(const String &str,
#if !CLICK_TOOL
		     const Element *context,
#endif
		     ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  Vector<String> conf;
  conf.push_back(str);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, true);
  int retval = cpva.develop_kvalues(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(conf, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);
  return retval;
}

/** @brief Parse and remove matching arguments from @a conf.
 * @param  conf  argument list
 * @param  context  element context
 * @param  errh  error handler
 * @param  ...  zero or more parameter items, terminated by ::cpEnd
 * @return  The number of parameters successfully assigned, or negative on error.
 *
 * The arguments in @a conf are parsed according to the items.  At least one
 * argument must correspond to each mandatory item, but extra arguments are
 * not errors.  If no error occurs, then the item results are assigned
 * appropriately; any arguments that successfully matched are removed from @a
 * conf; and the function returns the number of assigned items, which might be
 * 0.  If any error occurs, then @a conf and the item results are left
 * unchanged and the function returns a negative error code.  Errors are
 * reported to @a errh.
 *
 * The @a context argument is passed to any parsing functions that require
 * element context.
 */
int
cp_va_kparse_remove_keywords(Vector<String> &conf,
#if !CLICK_TOOL
			     const Element *context,
#endif
			     ErrorHandler *errh, ...)
{
  va_list val;
  va_start(val, errh);
  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, true);
  int retval = cpva.develop_kvalues(val, errh);
  if (retval >= 0)
    retval = cpva.assign_arguments(conf, "argument", errh);
  if (retval >= 0)
    retval = cpva.parse_arguments("argument"  CP_PASS_CONTEXT, errh);
  va_end(val);

  // remove keywords that were used
  if (retval >= 0) {
    int delta = 0;
    for (int i = 0; i < conf.size(); i++)
      if ((*cp_parameter_used)[i])
	delta++;
      else if (delta)
	conf[i - delta] = conf[i];
    conf.resize(conf.size() - delta);
  }

  return retval;
}


/** @brief Assign arguments from @a argv to @a values.
 * @param argv argument array
 * @param param_begin start iterator for parameter list
 * @param param_end end iterator for parameter list
 * @param values value storage (may equal null or &@a argv)
 * @return >= 0 on success, negative on failure
 *
 * This function is used to resolve an argument array.  The range [@a
 * param_begin, @a param_end) defines the parameter names.  This range begins
 * with zero or more empty strings, which define mandatory positional
 * arguments.  It continues with zero or more nonempty strings, which define
 * optional keyword arguments with the corresponding names.  It may optionally
 * conclude with "__REST__", which says that the last value should collect all
 * unassigned arguments.
 *
 * cp_assign_arguments attempts to assign the arguments in @a argv to the
 * corresponding parameters.  This succeeds if all mandatory positional
 * arguments are present and all other arguments are accounted for, either by
 * keywords or by "__REST__".  On success, returns >= 0.  On failure because
 * the argument list is invalid, returns -EINVAL.  On other failure cases,
 * such as out of memory, returns another negative error code.
 *
 * On success, cp_assign_arguments also optionally assigns *@a values to the
 * resulting value list.  *@a values is resized to size (@a param_end - @a
 * param_begin), and *@a values[@em i] is set to the argument corresponding to
 * @a param_begin[@em i].  If @a values is null this step is skipped and the
 * function has no side effects.  It is safe to set @a values to &@a argv.
 */
int
cp_assign_arguments(const Vector<String> &argv, const String *param_begin, const String *param_end, Vector<String> *values)
{
  // check common case
  if (param_begin == param_end || !param_end[-1]) {
    if (argv.size() != param_end - param_begin)
      return -EINVAL;
    else {
      if (values)
	*values = argv;
      return 0;
    }
  }

  if (!cp_values || !cp_parameter_used || param_end - param_begin > CP_VALUES_SIZE)
    return -ENOMEM; /*errh->error("out of memory in cp_va_parse");*/

  CpVaHelper cpva(cp_values, CP_VALUES_SIZE, false);
  if (param_begin != param_end && param_end[-1] == "__REST__") {
    cpva.ignore_rest = true;
    cpva.nvalues = (param_end - param_begin) - 1;
  } else {
    cpva.ignore_rest = false;
    cpva.nvalues = param_end - param_begin;
  }

  int arg;
  for (arg = 0; arg < cpva.nvalues && param_begin[arg] == ""; arg++) {
    cp_values[arg].argtype = 0;
    cp_values[arg].keyword = 0;
    cp_values[arg].v.s32 = cpkMandatory;
  }
  cpva.nrequired = cpva.npositional = arg;
  for (; arg < cpva.nvalues; arg++) {
    cp_values[arg].argtype = 0;
    cp_values[arg].keyword = param_begin[arg].c_str();
    cp_values[arg].v.s32 = cpkMandatory; // mandatory keyword
  }

  SilentErrorHandler serrh;
  int retval = cpva.assign_arguments(argv, "argument", &serrh);
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
    values->resize(param_end - param_begin);
    for (arg = 0; arg < param_end - param_begin; arg++)
      (*values)[arg] = cp_values[arg].v_string;
  }
  return retval;
}


// UNPARSING

String
cp_unparse_bool(bool b)
{
    return BoolArg::unparse(b);
}

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
  String int_part(real >> frac_bits);
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
    return ts.unparse_interval();
}

String
cp_unparse_interval(const timeval& tv)
{
    return Timestamp(tv).unparse_interval();
}

String
cp_unparse_bandwidth(uint32_t bw)
{
    return BandwidthArg::unparse(bw);
}


// initialization and cleanup

/** @brief Initialize the cp_va_kparse() implementation.
 *
 * This function must be called before any cp_va function is called.  It is
 * safe to call it multiple times.
 *
 * @note Elements don't need to worry about cp_va_static_initialize(); Click
 * drivers have already called it for you. */
void
cp_va_static_initialize()
{
    if (cp_values)
	return;

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
    cp_register_argtype(cpSize, "size_t", 0, default_parsefunc, default_storefunc, cpiSize);
    cp_register_argtype(cpNamedInteger, "named int", cpArgExtraInt, default_parsefunc, default_storefunc, cpiNamedInteger);
#if HAVE_INT64_TYPES
    cp_register_argtype(cpInteger64, "64-bit int", 0, default_parsefunc, default_storefunc, cpiInteger64);
    cp_register_argtype(cpUnsigned64, "64-bit unsigned", 0, default_parsefunc, default_storefunc, cpiUnsigned64);
#endif
    cp_register_argtype(cpReal2, "real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiReal2);
    cp_register_argtype(cpUnsignedReal2, "unsigned real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiUnsignedReal2);
    cp_register_argtype(cpReal10, "real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiReal10);
    cp_register_argtype(cpUnsignedReal10, "unsigned real", cpArgExtraInt, default_parsefunc, default_storefunc, cpiUnsignedReal10);
#if HAVE_FLOAT_TYPES
    cp_register_argtype(cpDouble, "double", 0, default_parsefunc, default_storefunc, cpiDouble);
#endif
    cp_register_argtype(cpSeconds, "time in sec", 0, default_parsefunc, default_storefunc, cpiSeconds);
    cp_register_argtype(cpSecondsAsMilli, "time in sec (msec precision)", 0, default_parsefunc, default_storefunc, cpiSecondsAsMilli);
    cp_register_argtype(cpSecondsAsMicro, "time in sec (usec precision)", 0, default_parsefunc, default_storefunc, cpiSecondsAsMicro);
    cp_register_argtype(cpTimestamp, "seconds since the epoch", 0, default_parsefunc, default_storefunc, cpiTimestamp);
    cp_register_argtype(cpTimestampSigned, "seconds since the epoch", 0, default_parsefunc, default_storefunc, cpiTimestampSigned);
    cp_register_argtype(cpTimeval, "seconds since the epoch", 0, default_parsefunc, default_storefunc, cpiTimeval);
    cp_register_argtype(cpBandwidth, "bandwidth", 0, default_parsefunc, default_storefunc, cpiBandwidth);
    cp_register_argtype(cpIPAddress, "IP address", 0, default_parsefunc, default_storefunc, cpiIPAddress);
    cp_register_argtype(cpIPPrefix, "IP address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIPPrefix);
    cp_register_argtype(cpIPAddressOrPrefix, "IP address or prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIPAddressOrPrefix);
    cp_register_argtype(cpIPAddressList, "list of IP addresses", 0, default_parsefunc, default_storefunc, cpiIPAddressList);
    cp_register_argtype(cpEthernetAddress, "Ethernet address", 0, default_parsefunc, default_storefunc, cpiEthernetAddress);
    cp_register_argtype(cpTCPPort, "TCP port", 0, default_parsefunc, default_storefunc, cpiTCPPort);
    cp_register_argtype(cpUDPPort, "UDP port", 0, default_parsefunc, default_storefunc, cpiUDPPort);
#if !CLICK_TOOL
    cp_register_argtype(cpElement, "element name", 0, default_parsefunc, default_storefunc, cpiElement);
    cp_register_argtype(cpElementCast, "cast element name", cpArgExtraCStr, default_parsefunc, default_storefunc, cpiElementCast);
    cp_register_argtype(cpHandlerName, "handler name", cpArgStore2, default_parsefunc, default_storefunc, cpiHandlerName);
    cp_register_argtype(cpHandlerCallRead, "read handler name", 0, default_parsefunc, default_storefunc, cpiHandlerCallRead);
    cp_register_argtype(cpHandlerCallWrite, "write handler name and value", 0, default_parsefunc, default_storefunc, cpiHandlerCallWrite);
    cp_register_argtype(cpHandlerCallPtrRead, "read handler name", 0, default_parsefunc, default_storefunc, cpiHandlerCallPtrRead);
    cp_register_argtype(cpHandlerCallPtrWrite, "write handler name and value", 0, default_parsefunc, default_storefunc, cpiHandlerCallPtrWrite);
#endif
#if HAVE_IP6
    cp_register_argtype(cpIP6Address, "IPv6 address", 0, default_parsefunc, default_storefunc, cpiIP6Address);
    cp_register_argtype(cpIP6Prefix, "IPv6 address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6Prefix);
    cp_register_argtype(cpIP6PrefixLen, "IPv6 address prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6PrefixLen);
    cp_register_argtype(cpIP6AddressOrPrefix, "IPv6 address or prefix", cpArgStore2, default_parsefunc, default_storefunc, cpiIP6AddressOrPrefix);
#endif
#if CLICK_USERLEVEL || CLICK_TOOL
    cp_register_argtype(cpFileOffset, "file offset", 0, default_parsefunc, default_storefunc, cpiFileOffset);
    cp_register_argtype(cpFilename, "filename", 0, default_parsefunc, default_storefunc, cpiFilename);
#endif
#if !CLICK_TOOL
    cp_register_argtype(cpAnno, "packet annotation", cpArgExtraInt, default_parsefunc, default_storefunc, cpiAnno);
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

/** @brief Clean up the cp_va_kparse() implementation.
 *
 * Call this function to release any memory allocated by the cp_va
 * implementation.  As a side effect, this function unregisters all argument
 * types registered by cp_register_argtype(). */
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
