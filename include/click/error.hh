#ifndef ERROR_HH
#define ERROR_HH
#include <click/string.hh>
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
  int verror(Seriousness, const String &, const char *, va_list);

  void lmessage(const String &, const char *, ...);
  int lwarning(const String &, const char *, ...);
  int lerror(const String &, const char *, ...);
  int lfatal(const String &, const char *, ...);
  static String fix_landmark(const String &);
  
  void message(const char *, ...);
  int warning(const char *, ...);
  int error(const char *, ...);
  int fatal(const char *, ...);

  String make_text(Seriousness, const char *, ...);
  virtual String make_text(Seriousness, const char *, va_list);
  virtual String apply_landmark(const String &, const String &);
  virtual void handle_text(Seriousness, const String &) = 0;

  static String prepend_lines(const String &, const String &);
  
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
  
  void handle_text(Seriousness, const String &);
  
};
#endif

class ErrorVeneer : public ErrorHandler { protected:

  ErrorHandler *_errh;

 public:

  ErrorVeneer(ErrorHandler *errh)	: _errh(errh) { }

  int nwarnings() const;
  int nerrors() const;
  void reset_counts();

  String make_text(Seriousness, const char *, va_list);
  String apply_landmark(const String &, const String &);
  void handle_text(Seriousness, const String &);

};

class ContextErrorHandler : public ErrorVeneer {
  
  String _context;
  String _indent;
  
 public:
  
  ContextErrorHandler(ErrorHandler *, const String &context = "",
		      const String &indent = "  ");
  
  String make_text(Seriousness, const char *, va_list);
  
};

class PrefixErrorHandler : public ErrorVeneer {
  
  String _prefix;
  
 public:
  
  PrefixErrorHandler(ErrorHandler *, const String &prefix);
  
  void handle_text(Seriousness, const String &);
  
};

#endif
