#ifndef ERROR_HH
#define ERROR_HH
#include "string.hh"
#ifndef __KERNEL__
# include <stdio.h>
#endif
#include <stdarg.h>

class ErrorHandler {
  
 public:
  
  enum Seriousness { Message, Warning, Error, Fatal };
  
  ErrorHandler()			{ }
  virtual ~ErrorHandler()		{ }
  static void static_initialize(ErrorHandler *);
  static void static_cleanup();
  
  static ErrorHandler *default_handler();
  static ErrorHandler *silent_handler();
  
  virtual int nwarnings() const = 0;
  virtual int nerrors() const = 0;
  virtual void reset_counts() = 0;
  
  // all error functions always return -1
  virtual int verror(Seriousness, const String &, const char *, va_list);
  virtual void vmessage(Seriousness, const String &) = 0;

  int lmessage(const String &, const char *, ...);
  int lwarning(const String &, const char *, ...);
  int lerror(const String &, const char *, ...);
  int lfatal(const String &, const char *, ...);
  static String fix_landmark(const String &);
  
  void message(const String &);
  void message(const char *, ...);
  int warning(const char *, ...);
  int error(const char *, ...);
  int fatal(const char *, ...);
  
};

#ifndef __KERNEL__
class FileErrorHandler : public ErrorHandler {
  
  FILE *_f;
  String _context;
  int _nwarnings;
  int _nerrors;
  
 public:
  
  FileErrorHandler(FILE *, const String & = String());
  
  int nwarnings() const;
  int nerrors() const;
  void reset_counts();
  
  void vmessage(Seriousness, const String &);
  
};
#endif

class ContextErrorHandler : public ErrorHandler {
  
  String _context;
  ErrorHandler *_errh;
  String _indent;
  
 public:
  
  ContextErrorHandler(ErrorHandler *, const String &context = "",
		      const String &indent = "  ");
  
  int nwarnings() const			{ return _errh->nwarnings(); }
  int nerrors() const			{ return _errh->nerrors(); }
  void reset_counts();
  
  int verror(Seriousness, const String &, const char *, va_list);
  void vmessage(Seriousness, const String &);
  
};

class PrefixErrorHandler : public ErrorHandler {
  
  String _prefix;
  ErrorHandler *_errh;
  
 public:
  
  PrefixErrorHandler(ErrorHandler *, const String &prefix);
  
  int nwarnings() const			{ return _errh->nwarnings(); }
  int nerrors() const			{ return _errh->nerrors(); }
  void reset_counts();
  
  int verror(Seriousness, const String &, const char *, va_list);
  void vmessage(Seriousness, const String &);
  
};

#endif
