#ifndef LEXER_HH
#define LEXER_HH
#include "hashmap.hh"
#include "router.hh"
#include "glue.hh"
class LexerSource;

enum Lexemes {
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

class Lexer {
  
  class Pseudoport;
  class Compound;
  typedef Router::Hookup Hookup;
  
  // lexer
  String _big_string;
  
  const char *_data;
  unsigned _len;
  unsigned _pos;
  
  LexerSource *_source;
  unsigned _lineno;
  
  bool get_data();
  unsigned skip_line(unsigned);
  Lexeme next_lexeme();
  static String lexeme_string(int);
  
  // parser
  static const int TCircleSize = 8;
  Lexeme _tcircle[TCircleSize];
  int _tpos;
  int _tfull;
  
  // element types
  HashMap<String, int> _element_type_map;
  Vector<Element *> _element_types;
  Element *_default_element_type;
  Element *_pseudoport_element_type;
  HashMap<String, int> _reset_element_type_map;
  int _reset_element_types;
  
  // elements
  HashMap<String, int> _element_map;
  Vector<Element *> _elements;
  Vector<String> _element_names;
  Vector<String> _element_configurations;
  
  Vector<Hookup> _hookup_from;
  Vector<Hookup> _hookup_to;
  
  Pseudoport *_definputs;
  Pseudoport *_defoutputs;
  
  // compound elements
  String _element_prefix;
  int _anonymous_prefixes;
  int _anonymous_offset;
  
  // errors
  ErrorHandler *_errh;
  
  int lerror(const char *, ...);

  String anon_element_name(const String &) const;
  int add_element(String, Element *, const String &);
  int fixed_element(String, Element *);
  int make_compound_element(String, Element *, const String &);
  void add_router_connections(int, const Vector<int> &, Router *);
  
 public:
  
  Lexer(ErrorHandler * = 0);
  virtual ~Lexer();
  
  void reset(LexerSource *);
  
  const Lexeme &lex();
  void unlex(const Lexeme &);
  String lex_config();
  String lex_compound_body();
  String landmark() const;
  
  bool expect(int, bool report_error = true);
  
  int add_element_type(Element *);
  int add_element_type(const String &, Element *);
  Element *element_type(const String &) const;
  Element *force_element_type(String);
  Element *default_element_type() const;
  void element_types_permanent();

  int permanent_element_types() const	{ return _reset_element_types; }
  Element *element_type(int i) const	{ return _element_types[i]; }
  
  int make_element(String, Element *, const String &);
  int make_anon_element(const String &, Element *, const String &);
  void connect(int f1, int p1, int f2, int p2);
  String element_name(int) const;
  
  void add_pseudoports(String, String);
  
  bool yport(int &port);
  bool yelement(int &element, bool comma_ok);
  void ydeclaration(const String &first_element = "");
  bool yconnection();
  void yelementclass();
  void ypseudoports();
  void ywithprefix();
  bool ystatement(bool nested = false);
  
  void clear();
  
  void find_connections(const Hookup &, bool, Vector<Hookup> &) const;
  void expand_connection(const Hookup &, bool, Vector<Hookup> &) const;
  
  Router *create_router();
  
};

class LexerSource { public:
  
  LexerSource()				{ }
  virtual ~LexerSource()		{ }
  
  virtual unsigned more_data(char *, unsigned) = 0;
  virtual String landmark(unsigned) const;

};
  
#ifndef __KERNEL__
class FileLexerSource : public LexerSource {
  
  const char *_filename;
  FILE *_f;
  bool _own_f;
  
 public:
  
  FileLexerSource(const char *, FILE * = 0);
  ~FileLexerSource();
  
  unsigned more_data(char *, unsigned);
  String landmark(unsigned lineno) const;
  
};
#endif

class MemoryLexerSource : public LexerSource {
  
  const char *_data;
  unsigned _pos;
  unsigned _len;
  
 public:
  
  MemoryLexerSource(const char *, unsigned);
  
  unsigned more_data(char *, unsigned);
  
};

#endif
