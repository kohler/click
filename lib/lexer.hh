#ifndef LEXER_HH
#define LEXER_HH
#include "hashmap.hh"
#include "router.hh"
#include "glue.hh"
class LexerExtra;

enum Lexemes {
  lexEOF = 0,
  lexIdent = 256,
  lexVariable,
  lexArrow,
  lex2Colon,
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

class Lexer {
  
  class TunnelEnd;
  class Compound;
  typedef Router::Hookup Hookup;
  
  // lexer
  String _big_string;
  
  const char *_data;
  unsigned _len;
  unsigned _pos;
  
  String _filename;
  unsigned _lineno;
  LexerExtra *_lextra;
  
  unsigned skip_line(unsigned);
  unsigned skip_slash_star(unsigned);
  unsigned skip_quote(unsigned, char endc);
  Lexeme next_lexeme();
  static String lexeme_string(int);
  
  // parser
  static const int TCIRCLE_SIZE = 8;
  Lexeme _tcircle[TCIRCLE_SIZE];
  int _tpos;
  int _tfull;
  
  // element types
  HashMap<String, int> _element_type_map;
  Vector<Element *> _element_types;
  Vector<String> _element_type_names;
  Vector<int> _element_type_next;
  Element *_tunnel_element_type;
  int _last_element_type;
  int _free_element_type;

  // configuration arguments for compounds
  HashMap<String, int> _variable_map;
  Vector<String> _variable_values;
  
  // elements
  HashMap<String, int> _element_map;
  Vector<Element *> _elements;
  Vector<String> _element_names;
  Vector<String> _element_configurations;
  Vector<String> _element_landmarks;
  
  Vector<Hookup> _hookup_from;
  Vector<Hookup> _hookup_to;
  
  TunnelEnd *_definputs;
  TunnelEnd *_defoutputs;
  
  // compound elements
  String _element_prefix;
  int _anonymous_offset;

  // requirements
  Vector<String> _requirements;
  
  // errors
  ErrorHandler *_errh;
  
  int lerror(const char *, ...);

  String anon_element_name(const String &) const;
  int get_element(String, int, const String & = String());
  int lexical_scoping_in() const;
  void lexical_scoping_out(int);
  void lexical_scoping_back(int, Vector<int> &);
  void lexical_scoping_forward(const Vector<int> &);
  int make_compound_element(String, int, const String &);
  void add_router_connections(int, const Vector<int> &, Router *);
  
 public:

  static const int TUNNEL_TYPE = 0;
  static const int ERROR_TYPE = 1;
  
  Lexer(ErrorHandler * = 0);
  virtual ~Lexer();
  
  int begin_parse(const String &data, const String &filename, LexerExtra *);
  void end_parse(int);
  
  const Lexeme &lex();
  void unlex(const Lexeme &);
  String lex_config();
  String lex_compound_body();
  String landmark() const;
  
  bool expect(int, bool report_error = true);
  
  int add_element_type(Element *);
  int add_element_type(const String &, Element *);
  int element_type(const String &) const;
  int force_element_type(String);

  void element_type_names(Vector<String> &) const;
  
  void remove_element_type(int);

  void connect(int f1, int p1, int f2, int p2);
  String element_name(int) const;
  
  void add_tunnel(String, String);
  
  bool yport(int &port);
  bool yelement_upref(int &element);
  bool yelement(int &element, bool comma_ok);
  void ydeclaration(const String &first_element = "");
  bool yconnection();
  void yelementclass();
  void ytunnel();
  int ylocal(String = "");
  void yrequire();
  bool ystatement(bool nested = false);
  
  void find_connections(const Hookup &, bool, Vector<Hookup> &) const;
  void expand_connection(const Hookup &, bool, Vector<Hookup> &) const;
  
  Router *create_router();
  
};

class LexerExtra { public:
  
  LexerExtra()				{ }
  virtual ~LexerExtra()			{ }
  
  virtual void require(const String &, ErrorHandler *);

};
  
#endif
