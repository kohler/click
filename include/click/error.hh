#ifndef ERROR_HH
#define ERROR_HH
#include <click/string.hh>
#ifndef __KERNEL__
# include <stdio.h>
#endif
#include <stdarg.h>

class ErrorHandler { public:
  
  enum Seriousness {
    ERR_DEBUG, ERR_MESSAGE, ERR_WARNING, ERR_ERROR, ERR_FATAL
  };
  
  ErrorHandler()			{ }
  virtual ~ErrorHandler()		{ }
  
  static void static_initialize(ErrorHandler *);
  static void static_cleanup();
  
  static ErrorHandler *default_handler();
  static ErrorHandler *silent_handler();
  
  static void set_default_handler(ErrorHandler *);
  
  virtual int nwarnings() const = 0;
  virtual int nerrors() const = 0;
  virtual void reset_counts() = 0;

  // all error functions always return -EINVAL

  void debug(const char *, ...);
  void message(const char *, ...);
  int warning(const char *, ...);
  int error(const char *, ...);
  int fatal(const char *, ...);

  void ldebug(const String &, const char *, ...);
  void lmessage(const String &, const char *, ...);
  int lwarning(const String &, const char *, ...);
  int lerror(const String &, const char *, ...);
  int lfatal(const String &, const char *, ...);

  int verror(Seriousness, const String &, const char *, va_list);
  
  String make_text(Seriousness, const char *, ...);
  virtual String make_text(Seriousness, const char *, va_list);
  virtual String decorate_text(Seriousness, const String &, const String &, const String &);
  virtual void handle_text(Seriousness, const String &) = 0;

  static String prepend_lines(const String &, const String &);
  
};

#ifndef __KERNEL__
class FileErrorHandler : public ErrorHandler { public:
  
  FileErrorHandler(FILE *, const String & = String());
  
  int nwarnings() const;
  int nerrors() const;
  void reset_counts();
  
  void handle_text(Seriousness, const String &);
  
 private:
  
  FILE *_f;
  String _context;
  int _nwarnings;
  int _nerrors;
  
};
#endif

class ErrorVeneer : public ErrorHandler { public:

  ErrorVeneer(ErrorHandler *errh)	: _errh(errh) { }

  int nwarnings() const;
  int nerrors() const;
  void reset_counts();

  String make_text(Seriousness, const char *, va_list);
  String decorate_text(Seriousness, const String &, const String &, const String &);
  void handle_text(Seriousness, const String &);

 protected:

  ErrorHandler *_errh;
 
};

class ContextErrorHandler : public ErrorVeneer { public:

  ContextErrorHandler(ErrorHandler *, const String &context,
		      const String &indent = "  ");
  
  String decorate_text(Seriousness, const String &, const String &, const String &);
  
 private:
  
  String _context;
  String _indent;
  
};

class PrefixErrorHandler : public ErrorVeneer { public:

  PrefixErrorHandler(ErrorHandler *, const String &prefix);
  
  String decorate_text(Seriousness, const String &, const String &, const String &);
  
 private:
  
  String _prefix;
  
};

class IndentErrorHandler : public ErrorVeneer { public:

  IndentErrorHandler(ErrorHandler *, const String &indent);
  
  String decorate_text(Seriousness, const String &, const String &, const String &);
  
 private:
  
  String _indent;
  
};

class LandmarkErrorHandler : public ErrorVeneer { public:

  LandmarkErrorHandler(ErrorHandler *, const String &);
  
  void set_landmark(const String &s)	{ _landmark = s; }
  
  String decorate_text(Seriousness, const String &, const String &, const String &);
  
 private:
  
  String _landmark;
  
};

#endif
