#ifndef LEXERT_HH
#define LEXERT_HH
#include <click/error.hh>
#include <stdio.h>
class RouterT;
class CompoundElementClassT;

enum {
  lexEOF = 0,
  lexIdent = 256,
  lexVariable,
  lexArrow,
  lex2Colon,
  lex2Bar,
  lex3Dot,
  lexTunnel,
  lexElementclass,
  lexRequire,
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
  
  String _filename;
  String _original_filename;
  unsigned _lineno;
  bool _ignore_line_directives;
  
  bool get_data();
  unsigned skip_line(unsigned);
  unsigned skip_slash_star(unsigned);
  unsigned skip_quote(unsigned, char);
  unsigned process_line_directive(unsigned);
  Lexeme next_lexeme();
  static String lexeme_string(int);
  
  // parser
  static const int TCIRCLE_SIZE = 8;
  Lexeme _tcircle[TCIRCLE_SIZE];
  int _tpos;
  int _tfull;
  
  // router
  RouterT *_router;

  int _anonymous_offset;
  int _compound_depth;
  
  // errors
  ErrorHandler *_errh;
  
  int lerror(const char *, ...);
  String anon_element_name(const String &) const;
  String anon_element_class_name(String) const;
  
 public:
  
  LexerT(ErrorHandler * = 0, bool ignore_line_directives = false);
  virtual ~LexerT();
  
  void reset(const String &data, const String &filename = String());
  void clear();
  void set_router(RouterT *);
  void ignore_line_directives(bool g)	{ _ignore_line_directives = g; }
  
  const Lexeme &lex();
  void unlex(const Lexeme &);
  String lex_config();
  String landmark() const;
  
  bool expect(int, bool report_error = true);
  
  int element_type(const String &) const;
  int force_element_type(String);
  
  int make_element(String, int, const String &, const String & = String());
  int make_anon_element(const String &, int, const String &, const String & = String());
  void connect(int f1, int p1, int p2, int f2);
  
  bool yport(int &port);
  bool yelement(int &element, bool comma_ok);
  void ydeclaration(const String &first_element = "");
  bool yconnection();
  void ycompound_arguments(CompoundElementClassT *);
  void yelementclass();
  void ytunnel();
  int ycompound(String);
  void yrequire();
  bool ystatement(bool nested = false);

  RouterT *router() const		{ return _router; }
  RouterT *take_router();
  
};

#endif
