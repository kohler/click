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
    lexIdent = 256,		// see also Lexer::lexeme_string
    lexVariable,
    lexArrow,
    lex2Arrow,
    lex2Colon,
    lex2Bar,
    lex3Dot,
    lexElementclass,
    lexRequire,
    lexProvide,
    lexDefine
};

class Lexeme { public:

    Lexeme()
	: _kind(lexEOF) {
    }
    Lexeme(int k, const String &s, bool compact = false)
	: _kind(k), _s(compact ? s.compact() : s) {
    }

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

    Lexeme lex() {
	return _unlex_pos ? _unlex[--_unlex_pos] : next_lexeme();
    }
    void unlex(const Lexeme &t) {
	assert(_unlex_pos < UNLEX_SIZE);
	_unlex[_unlex_pos++] = t;
    }
    String lex_config() {
	assert(!_unlex_pos);
	return _file.lex_config(this);
    }

    bool expect(int, bool no_error = false);

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

    void add_tunnels(String name, int *eidexes);

    bool ydone() const			{ return !_ps; }
    void ystep();

    Router *create_router(Master *);

  private:

    enum {
	max_depth = 50
    };

    struct FileState {
	String _big_string;
	const char *_end;
	const char *_pos;
	String _filename;
	String _original_filename;
	unsigned _lineno;

	FileState(const String &data, const String &filename);
	const char *skip_line(const char *s);
	const char *skip_slash_star(const char *s);
	const char *skip_backslash_angle(const char *s);
	const char *skip_quote(const char *s, char end_c);
	const char *process_line_directive(const char *s, Lexer *lexer);
	Lexeme next_lexeme(Lexer *lexer);
	String lex_config(Lexer *lexer);
	String landmark() const;
    };

    struct ElementState;
    struct ParseState;

    // lexer
    FileState _file;
    bool _compact_config;
    LexerExtra *_lextra;

    Lexeme next_lexeme() {
	return _file.next_lexeme(this);
    }
    static String lexeme_string(int);

    // parser
    enum { UNLEX_SIZE = 2 };
    Lexeme _unlex[2];
    int _unlex_pos;

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
    Compound *_c;
    ParseState *_ps;
    int _group_depth;

    Vector<TunnelEnd *> _tunnels;

    // compound elements
    int _anonymous_offset;

    // requirements
    Vector<String> _requirements;
    Vector<String> _libraries;

    // errors
    ErrorHandler *_errh;

    int lerror(const char *, ...);
    int lerror_syntax(const Lexeme &t);

    String anon_element_name(const String &) const;
    int get_element(String name, int etype,
		    const String &configuration = String(),
		    const String &filename = String(), unsigned lineno = 0);
    int lexical_scoping_in() const;
    void lexical_scoping_out(int);
    int remove_element_type(int, int *);
    int make_compound_element(int);
    void expand_compound_element(int, VariableEnvironment &);
    void add_router_connections(int, const Vector<int> &);

    void yport(bool isoutput);
    void yelement_name();
    void yelement_type(String name, int type,
		       bool this_ident, bool this_implicit);
    void yelement_config(ElementState *e, bool this_implicit);
    void yelement_next();

    void yconnection_connector();
    void yconnection_check_useless(const Vector<int> &x, bool isoutput);
    static void yconnection_analyze_ports(const Vector<int> &x, bool isoutput,
					  int &min_ports, int &expandable);
    void yconnection_connect_all(Vector<int> &outputs, Vector<int> &inputs, int connector);

    void ycompound_arguments(Compound *ct);
    void ycompound();
    void ycompound_next();
    void ycompound_end(const Lexeme &t);
    void ygroup();
    void ygroup_end();

    void yelementclass();
    void yrequire();
    void yrequire_library(const String &value);
    void yvar();

    void ystatement();

    TunnelEnd *find_tunnel(const Port &p, bool isoutput, bool insert);
    void expand_connection(const Port &p, bool isoutput, Vector<Port> &);

    friend class Compound;
    friend class TunnelEnd;
    friend struct FileState;

};

class LexerExtra { public:

    LexerExtra()			{ }
    virtual ~LexerExtra()		{ }

    virtual void require(String type, String value, ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif
