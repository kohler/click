// -*- c-basic-offset: 4 -*-
#ifndef CLICK_LEXERT_HH
#define CLICK_LEXERT_HH
#include <click/error.hh>
#include <click/hashtable.hh>
#include <click/archive.hh>
#include "landmarkt.hh"
#include <stdio.h>
class RouterT;
class ElementClassT;
class StringAccum;
class LexerTInfo;
class VariableEnvironment;
struct ArchiveElement;

enum {
    lexEOF = 0,
    lexIdent = 256,		// see also LexerT::lexeme_string
    lexVariable,
    lexConfig,
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

    Lexeme()				: _kind(lexEOF), _pos(0) { }
    Lexeme(int k, const String &s, const char *p) : _kind(k), _s(s), _pos(p) { }

    int kind() const			{ return _kind; }
    bool is(int k) const		{ return _kind == k; }
    operator bool() const		{ return _kind != lexEOF; }

    const String &string() const	{ return _s; }
    String &string()			{ return _s; }

    const char *pos1() const		{ return _pos; }
    const char *pos2() const		{ return _pos + _s.length(); }

  private:

    int _kind;
    String _s;
    const char *_pos;

};

class LexerT { public:

    LexerT(ErrorHandler *, bool ignore_line_directives);
    virtual ~LexerT();

    void reset(const String &data, const Vector<ArchiveElement> &archive, const String &filename);
    void clear();
    void set_lexinfo(LexerTInfo *);
    void ignore_line_directives(bool x)	{ _ignore_line_directives = x; }
    void expand_groups(bool x)		{ _expand_groups = x; }

    String remaining_text() const;
    void set_remaining_text(const String &);

    Lexeme lex() {
	return _unlex_pos ? _unlex[--_unlex_pos] : next_lexeme();
    }
    void unlex(const Lexeme &t) {
	assert(_unlex_pos < UNLEX_SIZE);
	_unlex[_unlex_pos++] = t;
    }
    Lexeme lex_config() {
	assert(!_unlex_pos);
	return _file.lex_config(this);
    }
    String landmark() const;
    inline LandmarkT landmarkt(const char *pos1, const char *pos2) const;

    bool yport(Vector<int> &ports, const char *pos[2]);
    bool yelement(Vector<int> &result, bool in_allowed, const char *epos[5]);
    bool yconnection();
    void ycompound_arguments(RouterT *);
    void yelementclass(const char *pos1);
    ElementClassT *ycompound(String, const char *decl_pos1, const char *name_pos1);
    void ygroup(String name, int group_nports[2], const char *open_pos1, const char *open_pos2);
    void yrequire();
    void yvar();
    bool ystatement(int nested = 0);

    RouterT *router() const		{ return _router; }
    RouterT *finish(const VariableEnvironment &global_scope);

  protected:

    struct FileState {
	String _big_string;
	const char *_end;
	const char *_pos;
	String _filename;
	String _original_filename;
	unsigned _lineno;
	LandmarkSetT *_lset;

	FileState(const String &text, const String &filename);
	FileState(const FileState &x)
	    : _big_string(x._big_string), _end(x._end), _pos(x._pos),
	      _filename(x._filename), _original_filename(x._original_filename),
	      _lineno(x._lineno), _lset(x._lset) {
	    _lset->ref();
	}
	~FileState() {
	    _lset->unref();
	}
	FileState &operator=(const FileState &x);
	const char *skip_line(const char *s);
	const char *skip_slash_star(const char *s);
	const char *skip_backslash_angle(const char *s);
	const char *skip_quote(const char *s, char end_c);
	const char *process_line_directive(const char *s, LexerT *lexer);
	Lexeme next_lexeme(LexerT *lexer);
	Lexeme lex_config(LexerT *lexer);
	String landmark() const;
	unsigned offset(const char *s) const {
	    assert(s >= _big_string.begin() && s <= _end);
	    return s - _big_string.begin();
	}
	LandmarkT landmarkt(const char *pos1, const char *pos2) const {
	    return LandmarkT(_lset, offset(pos1), offset(pos2));
	}
    };

    // lexer
    FileState _file;
    bool _ignore_line_directives;
    bool _expand_groups;

    bool get_data();
    Lexeme next_lexeme() {
	return _file.next_lexeme(this);
    }
    static String lexeme_string(int);

    // parser
    enum { UNLEX_SIZE = 2 };
    Lexeme _unlex[UNLEX_SIZE];
    int _unlex_pos;

    // router
    RouterT *_router;

    int _anonymous_offset;
    int _anonymous_class_count;
    int _group_depth;
    int _ngroups;
    bool _last_connection_ends_output;

    Vector<String> _libraries;

    // what names represent types? (builds up linearly)
    HashTable<String, ElementClassT *> _base_type_map;

    // errors
    LexerTInfo *_lexinfo;
    ErrorHandler *_errh;

    void xlerror(const char *pos1, const char *pos2, const String &landmark,
		 const char *anno, const char *format, va_list val);
    int lerror(const char *pos1, const char *pos2, const char *format, ...);
    int lerror(const Lexeme &t, const char *format, ...);
    int lwarning(const Lexeme &t, const char *format, ...);
    String anon_element_name(const String &) const;

    bool expect(int, bool no_error = false);
    const char *next_pos() const {
	return _unlex_pos ? _unlex[_unlex_pos - 1].pos1() : _file._pos;
    }

    ElementClassT *element_type(const Lexeme &) const;
    ElementClassT *force_element_type(const Lexeme &);
    void ydefine(RouterT *, const String &name, const String &value, const Lexeme &, bool &scope_order_error);
    void yrequire_library(const Lexeme &lexeme, const String &value);
    void yconnection_check_useless(const Vector<int> &x, bool isoutput, const char *epos[2], bool done);
    static void yconnection_analyze_ports(const Vector<int> &x, bool isoutput,
					  int &min_ports, int &expandable);
    void yconnection_connect_all(Vector<int> &outputs, Vector<int> &inputs, int connector, const char *pos1, const char *pos2);

    LexerT(const LexerT &);
    LexerT &operator=(const LexerT &);
    int make_element(String, const Lexeme &, const char *decl_pos2, ElementClassT *, const String &);
    int make_anon_element(const Lexeme &, const char *decl_pos2, ElementClassT *, const String &);
    void connect(int f1, int p1, int p2, int f2, const char *pos1, const char *pos2);

    friend struct FileState;

};

inline LandmarkT
LexerT::landmarkt(const char *pos1, const char *pos2) const
{
    return _file.landmarkt(pos1, pos2);
}

#endif
