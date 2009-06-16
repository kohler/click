// -*- c-basic-offset: 4; related-file-name: "../../lib/lexer.cc" -*-
#ifndef CLICK_LEXER_HH
#define CLICK_LEXER_HH
#include <click/hashtable.hh>
#include <click/router.hh>
#include <click/glue.hh>
#include <click/variableenv.hh>
CLICK_DECLS
class LexerExtra;

enum Lexemes {
    lexEOF = 0,
    lexIdent = 256,
    lexVariable,
    lexArrow,
    lex2Colon,
    lex2Bar,
    lex3Dot,
    lexElementclass,
    lexRequire,
    lexDefine
};

class Lexeme { public:

    Lexeme()				: _kind(lexEOF) { }
    Lexeme(int k, const String &s)	: _kind(k), _s(s) { }

    int kind() const			{ return _kind; }
    bool is(int k) const		{ return _kind == k; }

    const String &string() const	{ return _s; }
    String &string()			{ return _s; }

  private:

    int _kind;
    String _s;

};

class Lexer { public:

    enum { TUNNEL_TYPE = 0, ERROR_TYPE = 1 };

    class TunnelEnd;
    class Compound;
    typedef Router::Port Port;
    typedef Router::Connection Connection;

    Lexer();
    virtual ~Lexer();

    int begin_parse(const String &data, const String &filename, LexerExtra *, ErrorHandler * = 0);
    void end_parse(int);

    VariableEnvironment &global_scope()	{ return _global_scope; }
    ErrorHandler *errh() const		{ return _errh; }

    String remaining_text() const;
    void set_remaining_text(const String &);

    const Lexeme &lex();
    void unlex(const Lexeme &);
    String lex_config();
    String landmark() const;

    bool expect(int, bool report_error = true);

    typedef Element *(*ElementFactory)(uintptr_t);
#ifdef CLICK_LINUXMODULE
    int add_element_type(const String &, ElementFactory factory, uintptr_t thunk, struct module *module, bool scoped = false);
#else
    int add_element_type(const String &, ElementFactory factory, uintptr_t thunk, bool scoped = false);
#endif
    int element_type(const String &name) const {
	return _element_type_map[name];
    }
    int force_element_type(String name, bool report_error = true);

    void element_type_names(Vector<String> &) const;

    int remove_element_type(int t)	{ return remove_element_type(t, 0); }

    String element_name(int) const;
    String element_landmark(int) const;

    void add_tunnel(String, String);

    bool yport(int &port);
    bool yelement(int &element, bool comma_ok);
    void ydeclaration(const String &first_element = String());
    bool yconnection();
    void yelementclass();
    void ycompound_arguments(Compound *);
    int ycompound(String name = String());
    void yrequire();
    void yvar();
    bool ystatement(bool nested = false);

    Router *create_router(Master *);

  private:

    // lexer
    String _big_string;

    const char *_data;
    const char *_end;
    const char *_pos;

    String _filename;
    String _original_filename;
    unsigned _lineno;
    LexerExtra *_lextra;

    const char *skip_line(const char *);
    const char *skip_slash_star(const char *);
    const char *skip_backslash_angle(const char *);
    const char *skip_quote(const char *, char);
    const char *process_line_directive(const char *);
    Lexeme next_lexeme();
    static String lexeme_string(int);

    // parser
    enum { TCIRCLE_SIZE = 8 };
    Lexeme _tcircle[TCIRCLE_SIZE];
    int _tpos;
    int _tfull;

    // element types
    struct ElementType {
	ElementFactory factory;
	uintptr_t thunk;
#ifdef CLICK_LINUXMODULE
	struct module *module;
#endif
	String name;
	int next;
    };
    HashTable<String, int> _element_type_map;
    Vector<ElementType> _element_types;
    enum { ET_SCOPED = 0x80000000, ET_TMASK = 0x7FFFFFFF, ET_NULL = 0x7FFFFFFF };
    int _last_element_type;
    int _free_element_type;
    VariableEnvironment _global_scope;

    // elements
    HashTable<String, int> _element_map;
    Compound *_c;

    Vector<TunnelEnd *> _tunnels;

    // compound elements
    int _anonymous_offset;

    // requirements
    Vector<String> _requirements;

    // errors
    ErrorHandler *_errh;

    int lerror(const char *, ...);

    String anon_element_name(const String &) const;
    String deanonymize_element_name(const String &, int);
    int get_element(String, int, const String & = String(), const String & = String());
    int lexical_scoping_in() const;
    void lexical_scoping_out(int);
    int remove_element_type(int, int *);
    int make_compound_element(int);
    void expand_compound_element(int, VariableEnvironment &);
    void add_router_connections(int, const Vector<int> &);

    TunnelEnd *find_tunnel(const Port &p, bool isoutput, bool insert);
    void expand_connection(const Port &p, bool isoutput, Vector<Port> &);

    friend class Compound;
    friend class TunnelEnd;

};

class LexerExtra { public:

    LexerExtra()			{ }
    virtual ~LexerExtra()		{ }

    virtual void require(String, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
