// -*- c-basic-offset: 2; related-file-name: "../../lib/lexer.cc" -*-
#ifndef CLICK_LEXER_HH
#define CLICK_LEXER_HH
#include <click/hashmap.hh>
#include <click/router.hh>
#include <click/glue.hh>
class LexerExtra;
class VariableEnvironment;

enum Lexemes {
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

class Lexeme { public:

  Lexeme()				: _kind(lexEOF) { }
  Lexeme(int k, const String &s)	: _kind(k), _s(s) { }
  
  int kind() const			{ return _kind; }
  bool is(int k) const			{ return _kind == k; }
  
  const String &string() const		{ return _s; }
  String &string()			{ return _s; }
  
 private:
  
  int _kind;
  String _s;
  
};

class Lexer { public:

  static const int TUNNEL_TYPE = 0;
  static const int ERROR_TYPE = 1;
  
  class TunnelEnd;
  class Compound;
  class Synonym;
  typedef Router::Hookup Hookup;
  
  Lexer(ErrorHandler * = 0);
  virtual ~Lexer();
  
  int begin_parse(const String &data, const String &filename, LexerExtra *);
  void end_parse(int);

  ErrorHandler *errh() const		{ return _errh; }
  
  String remaining_text() const;
  void set_remaining_text(const String &);
  
  const Lexeme &lex();
  void unlex(const Lexeme &);
  String lex_config();
  String landmark() const;
  
  bool expect(int, bool report_error = true);
  
  int add_element_type(Element *);
  int add_element_type(String, Element *);
  int element_type(const String &) const;
  int force_element_type(String);

  void element_type_names(Vector<String> &) const;
  
  void remove_element_type(int);

  void connect(int element1, int port1, int element2, int port2);
  String element_name(int) const;
  String element_landmark(int) const;
  
  void add_tunnel(String, String);
  
  bool yport(int &port);
  bool yelement(int &element, bool comma_ok);
  void ydeclaration(const String &first_element = String());
  bool yconnection();
  void yelementclass();
  void ytunnel();
  void ycompound_arguments(Compound *);
  int ycompound(String name = String());
  void yrequire();
  bool ystatement(bool nested = false);
  
  void find_connections(const Hookup &, bool, Vector<Hookup> &) const;
  void expand_connection(const Hookup &, bool, Vector<Hookup> &) const;
  
  Router *create_router();

 private:
    
  // lexer
  String _big_string;
  
  const char *_data;
  unsigned _len;
  unsigned _pos;
  
  String _filename;
  String _original_filename;
  unsigned _lineno;
  LexerExtra *_lextra;
  
  unsigned skip_line(unsigned);
  unsigned skip_slash_star(unsigned);
  unsigned skip_backslash_angle(unsigned);
  unsigned skip_quote(unsigned, char);
  unsigned process_line_directive(unsigned);
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
  int _last_element_type;
  int _free_element_type;

  // elements
  HashMap<String, int> _element_map;
  Vector<int> _elements;
  Vector<String> _element_names;
  Vector<String> _element_configurations;
  Vector<String> _element_landmarks;

  Vector<Hookup> _hookup_from;
  Vector<Hookup> _hookup_to;
  
  TunnelEnd *_definputs;
  TunnelEnd *_defoutputs;
  
  // compound elements
  int _anonymous_offset;
  int _compound_depth;

  // requirements
  Vector<String> _requirements;
  
  // errors
  ErrorHandler *_errh;
  
  int lerror(const char *, ...);

  String anon_element_name(const String &) const;
  String anon_element_class_name(String) const;
  int get_element(String, int, const String & = String(), const String & = String());
  int lexical_scoping_in() const;
  void lexical_scoping_out(int);
  int make_compound_element(int);
  void expand_compound_element(int, const VariableEnvironment &);
  void add_router_connections(int, const Vector<int> &, Router *);

  friend class Compound;
  
};

class LexerExtra { public:
  
  LexerExtra()				{ }
  virtual ~LexerExtra()			{ }
  
  virtual void require(String, ErrorHandler *);

};
  
#endif
