// -*- c-basic-offset: 4 -*-
#ifndef CLICK_LEXERT_HH
#define CLICK_LEXERT_HH
#include <click/error.hh>
#include <cstdio>
class RouterT;
class ElementClassT;
class CompoundElementClassT;
class StringAccum;
class LexerTInfo;

enum {
    lexEOF = 0,
    lexIdent = 256,
    lexVariable,
    lexConfig,
    lexArrow,
    lex2Colon,
    lex2Bar,
    lex3Dot,
    lexTunnel,
    lexElementclass,
    lexRequire
};

class Lexeme { public:

    Lexeme()				: _kind(lexEOF) { }
    Lexeme(int k, const String &s, int p) : _kind(k), _s(s), _pos(p) { }
    
    int kind() const			{ return _kind; }
    bool is(int k) const		{ return _kind == k; }
    operator bool() const		{ return _kind != lexEOF; }
    
    const String &string() const	{ return _s; }
    String &string()			{ return _s; }

    int pos1() const			{ return _pos; }
    int pos2() const			{ return _pos + _s.length(); }
  
  private:
  
    int _kind;
    String _s;
    int _pos;

};

class LexerT { public:

    LexerT(ErrorHandler * = 0, bool ignore_line_directives = false);
    virtual ~LexerT();
  
    void reset(const String &data, const String &filename = String());
    void clear();
    void set_lexinfo(LexerTInfo *);
    void ignore_line_directives(bool g)	{ _ignore_line_directives = g; }

    String remaining_text() const;
    void set_remaining_text(const String &);
  
    const Lexeme &lex();
    void unlex(const Lexeme &);
    Lexeme lex_config();
    String landmark() const;
  
    bool yport(int &port, int &pos1, int &pos2);
    bool yelement(int &element, bool comma_ok);
    void ydeclaration(const Lexeme &first_element = Lexeme());
    bool yconnection();
    void ycompound_arguments(CompoundElementClassT *);
    void yelementclass(int pos1);
    void ytunnel();
    ElementClassT *ycompound(String, int decl_pos1, int name_pos1);
    void yrequire();
    bool ystatement(bool nested = false);

    RouterT *router() const		{ return _router; }
    RouterT *finish();
  
  protected:
  
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
    unsigned skip_backslash_angle(unsigned);
    unsigned skip_quote(unsigned, char);
    unsigned process_line_directive(unsigned);
    Lexeme next_lexeme();
    static String lexeme_string(int);
  
    // parser
    enum { TCIRCLE_SIZE = 8 };
    Lexeme _tcircle[TCIRCLE_SIZE];
    int _tpos;
    int _tfull;
  
    // router
    RouterT *_router;
    
    int _anonymous_offset;
    int _compound_depth;
  
    // errors
    LexerTInfo *_lexinfo;
    ErrorHandler *_errh;

    void vlerror(int, int, const String &, const char *, va_list);
    int lerror(int, int, const char *, ...);
    int lerror(const Lexeme &, const char *, ...);
    String anon_element_name(const String &) const;

    bool expect(int, bool report_error = true);
    int next_pos() const;
    
    ElementClassT *element_type(const Lexeme &) const;
    ElementClassT *force_element_type(const Lexeme &);
  
    int make_element(String, const Lexeme &, int decl_pos2, ElementClassT *, const String &, const String &);
    int make_anon_element(const Lexeme &, int decl_pos2, ElementClassT *, const String &, const String &);
    void connect(int f1, int p1, int p2, int f2);
  
};

#endif
