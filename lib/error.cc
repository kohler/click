#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "error.hh"
#include "straccum.hh"
#include "string.hh"
#ifndef CLICK_TOOL
# include "element.hh"
#endif
#include "confparse.hh"

void
ErrorHandler::message(const String &message)
{
  vmessage(Message, message);
}

void
ErrorHandler::message(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(Message, String(), format, val);
  va_end(val);
}

int
ErrorHandler::warning(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(Warning, String(), format, val);
  va_end(val);
  return -1;
}

int
ErrorHandler::error(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(Error, String(), format, val);
  va_end(val);
  return -1;
}

int
ErrorHandler::fatal(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(Fatal, String(), format, val);
  va_end(val);
  return -1;
}

int
ErrorHandler::lwarning(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(Warning, where, format, val);
  va_end(val);
  return -1;
}

int
ErrorHandler::lerror(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(Error, where, format, val);
  va_end(val);
  return -1;
}

int
ErrorHandler::lfatal(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(Fatal, where, format, val);
  va_end(val);
  return -1;
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
ErrorHandler::fix_landmark(const String &landmark)
{
  if (!landmark)
    return landmark;
  // find first nonspace
  int i, len = landmark.length();
  for (i = len - 1; i >= 0; i--)
    if (!isspace(landmark[i]))
      break;
  if (i < 0 || (i < len - 1 && landmark[i] == ':'))
    return landmark;
  // change landmark
  String lm = landmark.substring(0, i);
  if (landmark[i] != ':')
    lm += ':';
  if (i < len - 1)
    lm += landmark.substring(i);
  else
    lm += ' ';
  return lm;
}

int
ErrorHandler::verror(Seriousness seriousness, const String &where,
		     const char *s, va_list val)
{
  StringAccum msg;
  char numbuf[NUMBUF_SIZE];	// for numerics
  String placeholder;		// to ensure temporaries aren't destroyed
  numbuf[NUMBUF_SIZE-1] = 0;
  
  if (where)
    msg << fix_landmark(where);
  if (seriousness == Warning)
    msg << "warning: ";
  
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
    int flags = 0;    
   flags:
    switch (*++s) {
     case '#': flags |= ALTERNATE_FORM; goto flags;
     case '0': flags |= ZERO_PAD; goto flags;
     case '-': flags |= LEFT_JUST; goto flags;
     case ' ': flags |= SPACE_POSITIVE; goto flags;
     case '+': flags |= PLUS_POSITIVE; goto flags;
    }
    
    // parse field width
    int field_width = -1;
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
    int precision = -1;
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
    int width_flag = 0;
   width_flags:
    switch (*s) {
     case 'h': width_flag = 'h'; s++; goto width_flags;
     case 'l': width_flag = 'l'; s++; goto width_flags;
     case 'L': case 'q': width_flag = 'q'; s++; goto width_flags;
    }
    
    // conversion character
    // after switch, data lies between `s1' and `s2'
    const char *s1 = 0, *s2 = 0;
    int base = 10;
    switch (*s++) {
      
     case 's': {
       s1 = va_arg(val, const char *);
       if (!s1) s1 = "(null)";
       for (s2 = s1; *s2 && precision != 0; s2++)
	 if (precision > 0) precision--;
       break;
     }
     
     case 'c': {
       int c = va_arg(val, char);
       numbuf[0] = c;
       s1 = numbuf;
       s2 = s1 + 1;
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
       Element *f = va_arg(val, Element *);
       if (f) placeholder = f->declaration();
       else placeholder = "(null)";
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
       if (field_width > NUMBUF_SIZE) field_width = NUMBUF_SIZE;
       if (precision > NUMBUF_SIZE-4) precision = NUMBUF_SIZE-4;
       
       s2 = numbuf + NUMBUF_SIZE;
       
       unsigned long num;
       if (width_flag == 'q') {
#if 1
	 unsigned long long qnum = va_arg(val, unsigned long long);
	 if ((flags & SIGNED) && (long long)qnum < 0)
	   qnum = -(long long)qnum, flags |= NEGATIVE;
	 String q = cp_unparse_ulonglong(qnum, base, flags & UPPERCASE);
	 s1 = s2 - q.length();
	 memcpy((char *)s1, q.data(), q.length());
	 goto got_number;
#else
	 assert(0 && "can't pass %q to errorhandler in a tool");
#endif
       } else if (width_flag == 'h') {
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
       
      got_number:
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
       s1 = do_number((unsigned long)v, (char *)s2, 16, 0);
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
  
  int len = msg.length();
  String msg_str = String::claim_string(msg.take(), len);
  vmessage(seriousness, msg_str);
  
  return -1;
}


#ifndef __KERNEL__
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
FileErrorHandler::vmessage(Seriousness seriousness, const String &message)
{
  if (seriousness == Message) /* do nothing */;
  else if (seriousness == Warning) _nwarnings++;
  else _nerrors++;

  String s = _context + message + "\n";
  fputs(s.cc(), _f);
  
  if (seriousness == Fatal)
    exit(1);
}
#endif


//
// SILENT ERROR HANDLER
//

class SilentErrorHandler : public ErrorHandler {
  
  int _nwarnings;
  int _nerrors;
  
 public:
  
  SilentErrorHandler()			: _nwarnings(0), _nerrors(0) { }
  
  int nwarnings() const			{ return _nwarnings; }
  int nerrors() const			{ return _nerrors; }
  void reset_counts()			{ _nwarnings = _nerrors = 0; }

  int verror(Seriousness, const String &, const char *, va_list);
  void vmessage(Seriousness, const String &);
  
};

int
SilentErrorHandler::verror(Seriousness seriousness, const String &,
			   const char *, va_list)
{
  vmessage(seriousness, String());
  return -1;
}

void
SilentErrorHandler::vmessage(Seriousness seriousness, const String &)
{
  if (seriousness == Warning)
    _nwarnings++;
  if (seriousness == Error || seriousness == Fatal)
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


//
// CONTEXT ERROR HANDLER
//

ContextErrorHandler::ContextErrorHandler(ErrorHandler *errh,
					 const String &context,
					 const String &indent)
  : _context(context), _errh(errh), _indent(indent)
{
}

void
ContextErrorHandler::reset_counts()
{
  _errh->reset_counts();
}

int
ContextErrorHandler::verror(Seriousness seriousness, const String &where,
			    const char *format, va_list val)
{
  if (_context) {
    _errh->vmessage(Message, _context);
    _context = String();
  }
  return _errh->verror(seriousness, _indent + where, format, val);
}

void
ContextErrorHandler::vmessage(Seriousness seriousness, const String &message)
{
  if (_context) {
    _errh->vmessage(Message, _context);
    _context = String();
  }
  _errh->vmessage(seriousness, _indent + message);
}
