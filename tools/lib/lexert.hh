#ifndef LEXERT_HH
#define LEXERT_HH
#include "error.hh"
#include <stdio.h>
class LexerTSource;
class RouterT;

enum {
  lexEOF = 0,
  lexIdent = 256,
  lexArrow,
  lex2Colon,
  lexElementclass,
  lexPseudoports,
  lexWithprefix,
};

class Lexeme {
  
  int _kind;
  String _s;
  
 public:
  
  Lexeme()				: _kind(lexEOF) { }
  Lexeme(int k, const String &s)	: _kind(k), _s(s) { }
  
  int kind() const			{ return _kind; }
  bool is(int k) const			{ return _kind == k; }
  
  const String &string() const		{ return _s; }
  String &string()			{ return _s; }
  
};

class LexerT { protected:
  
  // lexer
  String _big_string;
  
  const char *_data;
  unsigned _len;
  unsigned _pos;
  
  LexerTSource *_source;
  unsigned _lineno;
  
  bool get_data();
  unsigned skip_line(unsigned);
  Lexeme next_lexeme();
  static String lexeme_string(int);
  
  // parser
  static const int TCIRCLE_SIZE = 8;
  Lexeme _tcircle[TCIRCLE_SIZE];
  int _tpos;
  int _tfull;
  
  // router
  RouterT *_router;

  String _element_prefix;
  int _anonymous_prefixes;
  
  // errors
  ErrorHandler *_errh;
  
  int lerror(const char *, ...);
  String anon_element_name(const String &) const;
  
 public:
  
  LexerT(ErrorHandler * = 0);
  virtual ~LexerT();
  
  void reset(LexerTSource *);
  void clear();
  
  const Lexeme &lex();
  void unlex(const Lexeme &);
  String lex_config();
  String landmark() const;
  
  bool expect(int, bool report_error = true);
  
  int element_type(const String &) const;
  int force_element_type(String);
  
  int make_element(String, int, const String &);
  int make_anon_element(const String &, int, const String &);
  void connect(int f1, int p1, int p2, int f2);
  
  bool yport(int &port);
  bool yelement(int &element, bool comma_ok);
  void ydeclaration(const String &first_element = "");
  bool yconnection();
  void yelementclass();
  void ypseudoports();
  void ywithprefix();
  bool ystatement(bool nested = false);
    
  RouterT *take_router();
  
};

class LexerTSource { public:
  
  LexerTSource()			{ }
  virtual ~LexerTSource()		{ }
  
  virtual unsigned more_data(char *, unsigned) = 0;
  virtual String landmark(unsigned) const;

};
  
class FileLexerTSource : public LexerTSource {
  
  const char *_filename;
  FILE *_f;
  bool _own_f;
  
 public:
  
  FileLexerTSource(const char *, FILE * = 0);
  ~FileLexerTSource();
  
  unsigned more_data(char *, unsigned);
  String landmark(unsigned lineno) const;
  
};

class MemoryLexerTSource : public LexerTSource {
  
  const char *_data;
  unsigned _pos;
  unsigned _len;
  
 public:
  
  MemoryLexerTSource(const char *, unsigned);
  
  unsigned more_data(char *, unsigned);
  
};

#endif
