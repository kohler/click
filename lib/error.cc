/*
 * error.{cc,hh} -- flexible classes for error reporting
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#include <click/error.hh>
#include <click/straccum.hh>
#include <click/string.hh>
#ifndef CLICK_TOOL
# include <click/element.hh>
#endif
#include <click/confparse.hh>

void
ErrorHandler::debug(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_DEBUG, String(), format, val);
  va_end(val);
}

void
ErrorHandler::message(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_MESSAGE, String(), format, val);
  va_end(val);
}

int
ErrorHandler::warning(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_WARNING, String(), format, val);
  va_end(val);
  return -EINVAL;
}

int
ErrorHandler::error(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_ERROR, String(), format, val);
  va_end(val);
  return -EINVAL;
}

int
ErrorHandler::fatal(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_FATAL, String(), format, val);
  va_end(val);
  return -EINVAL;
}

void
ErrorHandler::ldebug(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_DEBUG, where, format, val);
  va_end(val);
}

void
ErrorHandler::lmessage(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_MESSAGE, where, format, val);
  va_end(val);
}

int
ErrorHandler::lwarning(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_WARNING, where, format, val);
  va_end(val);
  return -EINVAL;
}

int
ErrorHandler::lerror(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_ERROR, where, format, val);
  va_end(val);
  return -EINVAL;
}

int
ErrorHandler::lfatal(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_FATAL, where, format, val);
  va_end(val);
  return -EINVAL;
}

String
ErrorHandler::make_text(Seriousness seriousness, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  String s = make_text(seriousness, format, val);
  va_end(val);
  return s;
}

#define ZERO_PAD 1
#define PLUS_POSITIVE 2
#define SPACE_POSITIVE 4
#define LEFT_JUST 8
#define ALTERNATE_FORM 16
#define UPPERCASE 32
#define SIGNED 64
#define NEGATIVE 128

#define NUMBUF_SIZE 128

static char *
do_number(unsigned long num, char *after_last, int base, int flags)
{
  const char *digits =
    ((flags & UPPERCASE) ? "0123456789ABCDEF" : "0123456789abcdef");
  char *pos = after_last;
  while (num) {
    *--pos = digits[num % base];
    num /= base;
  }
  if (pos == after_last)
    *--pos = '0';
  return pos;
}

static char *
do_number_flags(char *pos, char *after_last, int base, int flags,
		int precision, int field_width)
{
  // remove ALTERNATE_FORM for zero results in base 16
  if ((flags & ALTERNATE_FORM) && base == 16 && *pos == '0')
    flags &= ~ALTERNATE_FORM;
  
  // account for zero padding
  if (precision >= 0)
    while (after_last - pos < precision)
      *--pos = '0';
  else if (flags & ZERO_PAD) {
    if ((flags & ALTERNATE_FORM) && base == 16)
      field_width -= 2;
    if ((flags & NEGATIVE) || (flags & (PLUS_POSITIVE | SPACE_POSITIVE)))
      field_width--;
    while (after_last - pos < field_width)
      *--pos = '0';
  }
  
  // alternate forms
  if ((flags & ALTERNATE_FORM) && base == 8 && pos[1] != '0')
    *--pos = '0';
  else if ((flags & ALTERNATE_FORM) && base == 16) {
    *--pos = ((flags & UPPERCASE) ? 'X' : 'x');
    *--pos = '0';
  }
  
  // sign
  if (flags & NEGATIVE)
    *--pos = '-';
  else if (flags & PLUS_POSITIVE)
    *--pos = '+';
  else if (flags & SPACE_POSITIVE)
    *--pos = ' ';
  
  return pos;
}

String
ErrorHandler::make_text(Seriousness seriousness, const char *s, va_list val)
{
  StringAccum msg;
  char numbuf[NUMBUF_SIZE];	// for numerics
  String placeholder;		// to ensure temporaries aren't destroyed
  String s_placeholder;		// ditto, in case we change `s'
  numbuf[NUMBUF_SIZE-1] = 0;
  
  if (seriousness == ERR_WARNING) {
    // prepend `warning: ' to every line
    s_placeholder = prepend_lines("warning: ", s);
    s = s_placeholder.cc();
  }
  
  // declare and initialize these here to make gcc shut up about possible 
  // use before initialization
  int flags = 0;
  int field_width = -1;
  int precision = -1;
  int width_flag = 0;
  int base = 10;
  while (1) {
    
    const char *pct = strchr(s, '%');
    if (!pct) {
      if (*s) msg << s;
      break;
    }
    if (pct != s) {
      memcpy(msg.extend(pct - s), s, pct - s);
      s = pct;
    }
    
    // parse flags
    flags = 0;
   flags:
    switch (*++s) {
     case '#': flags |= ALTERNATE_FORM; goto flags;
     case '0': flags |= ZERO_PAD; goto flags;
     case '-': flags |= LEFT_JUST; goto flags;
     case ' ': flags |= SPACE_POSITIVE; goto flags;
     case '+': flags |= PLUS_POSITIVE; goto flags;
    }
    
    // parse field width
    field_width = -1;
    if (*s == '*') {
      field_width = va_arg(val, int);
      if (field_width < 0) {
	field_width = -field_width;
	flags |= LEFT_JUST;
      }
      s++;
    } else if (*s >= '0' && *s <= '9')
      for (field_width = 0; *s >= '0' && *s <= '9'; s++)
	field_width = 10*field_width + *s - '0';
    
    // parse precision
    precision = -1;
    if (*s == '.') {
      s++;
      precision = 0;
      if (*s == '*') {
	precision = va_arg(val, int);
	s++;
      } else if (*s >= '0' && *s <= '9')
	for (; *s >= '0' && *s <= '9'; s++)
	  precision = 10*precision + *s - '0';
    }
    
    // parse width flags
    width_flag = 0;
   width_flags:
    switch (*s) {
     case 'h':
      width_flag = 'h'; s++; goto width_flags;
     case 'l':
      width_flag = (width_flag == 'l' ? 'q' : 'l'); s++; goto width_flags;
     case 'L': case 'q':
      width_flag = 'q'; s++; goto width_flags;
    }
    
    // conversion character
    // after switch, data lies between `s1' and `s2'
    const char *s1 = 0, *s2 = 0;
    base = 10;
    switch (*s++) {
      
     case 's': {
       s1 = va_arg(val, const char *);
       if (!s1)
	 s1 = "(null)";
       if (flags & ALTERNATE_FORM) {
	 placeholder = String(s1).printable();
	 s1 = placeholder.cc();
       }
       for (s2 = s1; *s2 && precision != 0; s2++)
	 if (precision > 0)
	   precision--;
       break;
     }

     case 'c': {
       int c = va_arg(val, int);
       // check for extension of 'signed char' to 'int'
       if (c < 0)
	 c += 256;
       // assume ASCII
       if (c == '\n')
	 strcpy(numbuf, "\\n");
       else if (c == '\t')
	 strcpy(numbuf, "\\t");
       else if (c == '\r')
	 strcpy(numbuf, "\\r");
       else if (c == '\0')
	 strcpy(numbuf, "\\0");
       else if (c < 0 || c >= 256)
	 strcpy(numbuf, "(bad char)");
       else if (c < 32 || c >= 0177)
	 sprintf(numbuf, "\\%03o", c);
       else
	 sprintf(numbuf, "%c", c);
       s1 = numbuf;
       s2 = strchr(numbuf, 0);
       break;
     }
     
     case '%': {
       numbuf[0] = '%';
       s1 = numbuf;
       s2 = s1 + 1;
       break;
     }

#ifndef CLICK_TOOL
     case 'e': {
       Element *e = va_arg(val, Element *);
       if (e)
	 placeholder = e->declaration();
       else
	 placeholder = "(null)";
       s1 = placeholder.data();
       s2 = s1 + placeholder.length();
       break;
     }
#endif
     
     case 'd':
     case 'i':
      flags |= SIGNED;
     case 'u':
     number: {
       // protect numbuf from overflow
       if (field_width > NUMBUF_SIZE)
	 field_width = NUMBUF_SIZE;
       if (precision > NUMBUF_SIZE-4)
	 precision = NUMBUF_SIZE-4;
       
       s2 = numbuf + NUMBUF_SIZE;
       
       unsigned long num;
#ifdef HAVE_INT64_TYPES
       if (width_flag == 'q') {
	 unsigned long long qnum = va_arg(val, unsigned long long);
	 if ((flags & SIGNED) && (long long)qnum < 0)
	   qnum = -(long long)qnum, flags |= NEGATIVE;
	 String q = cp_unparse_unsigned64(qnum, base, flags & UPPERCASE);
	 s1 = s2 - q.length();
	 memcpy((char *)s1, q.data(), q.length());
	 goto got_number;
       }
#endif
       if (width_flag == 'h') {
	 num = (unsigned short)va_arg(val, int);
	 if ((flags & SIGNED) && (short)num < 0)
	   num = -(short)num, flags |= NEGATIVE;
       } else if (width_flag == 'l') {
	 num = va_arg(val, unsigned long);
	 if ((flags & SIGNED) && (long)num < 0)
	   num = -(long)num, flags |= NEGATIVE;
       } else {
	 num = va_arg(val, unsigned int);
	 if ((flags & SIGNED) && (int)num < 0)
	   num = -(int)num, flags |= NEGATIVE;
       }
       s1 = do_number(num, (char *)s2, base, flags);

#ifdef HAVE_INT64_TYPES
      got_number:
#endif
       s1 = do_number_flags((char *)s1, (char *)s2, base, flags,
			    precision, field_width);
       break;
     }
     
     case 'o':
      base = 8;
      goto number;
      
     case 'X':
      flags |= UPPERCASE;
     case 'x':
      base = 16;
      goto number;
      
     case 'p': {
       void *v = va_arg(val, void *);
       s2 = numbuf + NUMBUF_SIZE;
       s1 = do_number((unsigned long)v, (char *)s2, 16, flags);
       s1 = do_number_flags((char *)s1, (char *)s2, 16, flags | ALTERNATE_FORM,
			    precision, field_width);
       break;
     }
     
     default:
      assert(0 && "Bad % in error");
      break;
      
    }

    // add result of conversion
    int slen = s2 - s1;
    if (slen > field_width) field_width = slen;
    char *dest = msg.extend(field_width);
    if (flags & LEFT_JUST) {
      memcpy(dest, s1, slen);
      memset(dest + slen, ' ', field_width - slen);
    } else {
      memcpy(dest + field_width - slen, s1, slen);
      memset(dest, (flags & ZERO_PAD ? '0' : ' '), field_width - slen);
    }
  }

  return msg.take_string();
}

String
ErrorHandler::decorate_text(Seriousness, const String &prefix, const String &landmark, const String &text)
{
  String new_text;
  
  if (!landmark)
    new_text = text;

  else if (landmark.length() > 2 && landmark[0] == '\\'
	   && landmark.back() == '\\')
    // ignore special-purpose landmarks (begin and end with a backslash `\')
    new_text = text;

  else {
    // fix landmark: skip trailing spaces and trailing colon
    int i, len = landmark.length();
    for (i = len - 1; i >= 0; i--)
      if (!isspace(landmark[i]))
	break;
    if (i >= 0 && landmark[i] == ':')
      i--;

    // prepend landmark
    new_text = prepend_lines(landmark.substring(0, i+1) + ": ", text);
  }

  // prepend prefix, if any
  if (prefix)
    return prepend_lines(prefix, new_text);
  else
    return new_text;
}

int
ErrorHandler::verror(Seriousness seriousness, const String &where,
		     const char *s, va_list val)
{
  String text = make_text(seriousness, s, val);
  text = decorate_text(seriousness, String(), where, text);
  handle_text(seriousness, text);
  return -EINVAL;
}

int
ErrorHandler::verror_text(Seriousness seriousness, const String &where,
			  const String &text)
{
  // text is already made
  String dec_text = decorate_text(seriousness, String(), where, text);
  handle_text(seriousness, dec_text);
  return -EINVAL;
}

void
ErrorHandler::set_error_code(int)
{
}

String
ErrorHandler::prepend_lines(const String &prepend, const String &text)
{
  if (!prepend)
    return text;
  
  StringAccum sa;
  int pos = 0, nl;
  while ((nl = text.find_left('\n', pos)) >= 0) {
    sa << prepend << text.substring(pos, nl - pos + 1);
    pos = nl + 1;
  }
  if (pos < text.length())
    sa << prepend << text.substring(pos);
  
  return sa.take_string();
}

#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
//
// FILE ERROR HANDLER
//

FileErrorHandler::FileErrorHandler(FILE *f, const String &context)
  : _f(f), _context(context), _nwarnings(0), _nerrors(0)
{
}

int
FileErrorHandler::nwarnings() const
{
  return _nwarnings;
}

int
FileErrorHandler::nerrors() const
{
  return _nerrors;
}

void
FileErrorHandler::reset_counts()
{
  _nwarnings = _nerrors = 0;
}

void
FileErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  if (seriousness <= ERR_MESSAGE)
    /* do nothing */;
  else if (seriousness == ERR_WARNING)
    _nwarnings++;
  else
    _nerrors++;

  int pos = 0, nl;
  while ((nl = message.find_left('\n', pos)) >= 0) {
    String s = _context + message.substring(pos, nl - pos) + "\n";
    fwrite(s.data(), 1, s.length(), _f);
    pos = nl + 1;
  }
  if (pos < message.length()) {
    String s = _context + message.substring(pos) + "\n";
    fwrite(s.data(), 1, s.length(), _f);
  }
  
  if (seriousness >= ERR_FATAL)
    exit(1);
}
#endif


//
// SILENT ERROR HANDLER
//

class SilentErrorHandler : public ErrorHandler { public:

  SilentErrorHandler()			: _nwarnings(0), _nerrors(0) { }
  
  int nwarnings() const			{ return _nwarnings; }
  int nerrors() const			{ return _nerrors; }
  void reset_counts()			{ _nwarnings = _nerrors = 0; }

  void handle_text(Seriousness, const String &);  

 private:
  
  int _nwarnings;
  int _nerrors;
  
};

void
SilentErrorHandler::handle_text(Seriousness seriousness, const String &)
{
  if (seriousness == ERR_WARNING)
    _nwarnings++;
  if (seriousness == ERR_ERROR || seriousness == ERR_FATAL)
    _nerrors++;
}


//
// STATIC ERROR HANDLERS
//

static ErrorHandler *the_default_handler = 0;
static ErrorHandler *the_silent_handler = 0;

void
ErrorHandler::static_initialize(ErrorHandler *default_handler)
{
  the_default_handler = default_handler;
  the_silent_handler = new SilentErrorHandler;
}

void
ErrorHandler::static_cleanup()
{
  delete the_default_handler;
  delete the_silent_handler;
  the_default_handler = the_silent_handler = 0;
}

bool
ErrorHandler::has_default_handler()
{
  return the_default_handler ? true : false;
}

ErrorHandler *
ErrorHandler::default_handler()
{
  assert(the_default_handler);
  return the_default_handler;
}

ErrorHandler *
ErrorHandler::silent_handler()
{
  assert(the_silent_handler);
  return the_silent_handler;
}

void
ErrorHandler::set_default_handler(ErrorHandler *errh)
{
  the_default_handler = errh;
}


//
// ERROR VENEER
//

int
ErrorVeneer::nwarnings() const
{
  return _errh->nwarnings();
}

int
ErrorVeneer::nerrors() const
{
  return _errh->nerrors();
}

void
ErrorVeneer::reset_counts()
{
  _errh->reset_counts();
}

String
ErrorVeneer::make_text(Seriousness seriousness, const char *s, va_list val)
{
  return _errh->make_text(seriousness, s, val);
}

String
ErrorVeneer::decorate_text(Seriousness seriousness, const String &prefix, const String &landmark, const String &text)
{
  return _errh->decorate_text(seriousness, prefix, landmark, text);
}

void
ErrorVeneer::handle_text(Seriousness seriousness, const String &text)
{
  _errh->handle_text(seriousness, text);
}


//
// CONTEXT ERROR HANDLER
//

ContextErrorHandler::ContextErrorHandler(ErrorHandler *errh,
					 const String &context,
					 const String &indent)
  : ErrorVeneer(errh), _context(context), _indent(indent)
{
}

String
ContextErrorHandler::decorate_text(Seriousness seriousness, const String &prefix, const String &landmark, const String &text)
{
  String context_lines;
  if (_context) {
    context_lines = _errh->decorate_text(ERR_MESSAGE, String(), landmark, _context);
    if (context_lines && context_lines.back() != '\n')
      context_lines += '\n';
    _context = String();
  }
  return context_lines + _errh->decorate_text(seriousness, String(), landmark, prepend_lines(_indent + prefix, text));
}


//
// PREFIX ERROR HANDLER
//

PrefixErrorHandler::PrefixErrorHandler(ErrorHandler *errh,
				       const String &prefix)
  : ErrorVeneer(errh), _prefix(prefix)
{
}

String
PrefixErrorHandler::decorate_text(Seriousness seriousness, const String &prefix, const String &landmark, const String &text)
{
  return _errh->decorate_text(seriousness, _prefix + prefix, landmark, text);
}


//
// INDENT ERROR HANDLER
//

IndentErrorHandler::IndentErrorHandler(ErrorHandler *errh,
				       const String &indent)
  : ErrorVeneer(errh), _indent(indent)
{
}

String
IndentErrorHandler::decorate_text(Seriousness seriousness, const String &prefix, const String &landmark, const String &text)
{
  return _errh->decorate_text(seriousness, String(), landmark, prepend_lines(_indent + prefix, text));
}


//
// LANDMARK ERROR HANDLER
//

LandmarkErrorHandler::LandmarkErrorHandler(ErrorHandler *errh, const String &landmark)
  : ErrorVeneer(errh), _landmark(landmark)
{
}

String
LandmarkErrorHandler::decorate_text(Seriousness seriousness, const String &prefix, const String &lm, const String &text)
{
  if (lm)
    return _errh->decorate_text(seriousness, prefix, lm, text);
  else
    return _errh->decorate_text(seriousness, prefix, _landmark, text);
}


//
// BAIL ERROR HANDLER
//

#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)

BailErrorHandler::BailErrorHandler(ErrorHandler *errh, Seriousness s)
  : ErrorVeneer(errh), _exit_seriousness(s)
{
}

void
BailErrorHandler::handle_text(Seriousness s, const String &text)
{
  _errh->handle_text(s, text);
  if (s >= _exit_seriousness)
    exit(1);
}

#endif
