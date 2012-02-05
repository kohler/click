// -*- c-basic-offset: 4 -*-
/*
 * lexert.{cc,hh} -- configuration file parser for tools
 * Eddie Kohler
 *
 * Copyright (c) 1999-2012 Eddie Kohler
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2004-2011 Regents of the University of California
 * Copyright (c) 2008-2012 Meraki, Inc.
 * Copyright (c) 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include "lexert.hh"
#include "lexertinfo.hh"
#include "routert.hh"
#include <click/confparse.hh>
#include <click/userutils.hh>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static LexerTInfo *stub_lexinfo = 0;

LexerT::FileState::FileState(const String &data, const String &filename)
  : _big_string(data), _end(data.end()), _pos(data.begin()),
    _filename(filename), _original_filename(filename), _lineno(1),
    _lset(new LandmarkSetT)
{
    _lset->new_line(0, _filename, _lineno);
}

LexerT::FileState &
LexerT::FileState::operator=(const FileState &x)
{
    x._lset->ref();
    _lset->unref();
    _big_string = x._big_string;
    _end = x._end;
    _pos = x._pos;
    _filename = x._filename;
    _original_filename = x._original_filename;
    _lineno = x._lineno;
    _lset = x._lset;
    return *this;
}

LexerT::LexerT(ErrorHandler *errh, bool ignore_line_directives)
  : _file(String(), String()),
    _ignore_line_directives(ignore_line_directives), _expand_groups(false),
    _unlex_pos(0), _router(0), _base_type_map(0), _errh(errh)
{
    if (!_errh)
	_errh = ErrorHandler::default_handler();
    if (!stub_lexinfo)
	stub_lexinfo = new LexerTInfo;
    _lexinfo = stub_lexinfo;
    clear();
}

LexerT::~LexerT()
{
    clear();
    _router->unuse();
}

void
LexerT::reset(const String &data, const Vector<ArchiveElement> &archive, const String &filename)
{
    clear();

    _file = FileState(data, filename);

    // provide archive elements
    for (int i = 0; i < archive.size(); i++)
	if (archive[i].live() && archive[i].name != "config")
	    _router->add_archive(archive[i]);
}

void
LexerT::clear()
{
    if (_router)
	_router->unuse();
    _router = new RouterT();
    _router->use();		// hold a reference to the router

    _file = FileState(String(), String());
    _unlex_pos = 0;

    _base_type_map.clear();
    _anonymous_offset = 0;
    _anonymous_class_count = 0;
    _group_depth = 0;
    _ngroups = 0;
    _libraries.clear();
}

void
LexerT::set_lexinfo(LexerTInfo *li)
{
    _lexinfo = (li ? li : stub_lexinfo);
}


// LEXING: LOWEST LEVEL

String
LexerT::remaining_text() const
{
    return _file._big_string.substring(_file._pos, _file._end);
}

void
LexerT::set_remaining_text(const String &s)
{
    _file._big_string = s;
    _file._pos = s.begin();
    _file._end = s.end();
}

const char *
LexerT::FileState::skip_line(const char *s)
{
  _lineno++;
  for (; s < _end; s++)
    if (*s == '\n')
      return s + 1;
    else if (*s == '\r') {
      if (s + 1 < _end && s[1] == '\n')
	return s + 2;
      else
	return s + 1;
    }
  _lineno--;
  return s;
}

const char *
LexerT::FileState::skip_slash_star(const char *s)
{
  for (; s < _end; s++)
    if (*s == '\n')
      _lineno++;
    else if (*s == '\r') {
      if (s + 1 < _end && s[1] == '\n')
	s++;
      _lineno++;
    } else if (*s == '*' && s + 1 < _end && s[1] == '/')
      return s + 2;
  return _end;
}

const char *
LexerT::FileState::skip_backslash_angle(const char *s)
{
  for (; s < _end; s++)
    if (*s == '\n')
      _lineno++;
    else if (*s == '\r') {
      if (s + 1 < _end && s[1] == '\n')
	s++;
      _lineno++;
    } else if (*s == '/' && s + 1 < _end) {
      if (s[1] == '/')
	s = skip_line(s + 2) - 1;
      else if (s[1] == '*')
	s = skip_slash_star(s + 2) - 1;
    } else if (*s == '>')
      return s + 1;
  return _end;
}

const char *
LexerT::FileState::skip_quote(const char *s, char endc)
{
  for (; s < _end; s++)
    if (*s == '\n')
      _lineno++;
    else if (*s == '\r') {
      if (s + 1 < _end && s[1] == '\n')
	s++;
      _lineno++;
    } else if (*s == '\\' && endc == '\"' && s + 1 < _end) {
      if (s[1] == '<')
	s = skip_backslash_angle(s + 2) - 1;
      else
	s++;
    } else if (*s == endc)
      return s + 1;
  return _end;
}

const char *
LexerT::FileState::process_line_directive(const char *s, LexerT *lexer)
{
  const char *first_pos = s;

  for (s++; s < _end && (*s == ' ' || *s == '\t'); s++)
    /* nada */;
  if (s + 4 < _end && *s == 'l' && s[1] == 'i'
      && s[2] == 'n' && s[3] == 'e'
      && (s[4] == ' ' || s[4] == '\t')) {
    for (s += 5; s < _end && (*s == ' ' || *s == '\t'); s++)
      /* nada */;
  }
  if (s >= _end || !isdigit((unsigned char) *s)) {
    // complain about bad directive
    lexer->lerror(first_pos, s, "unknown preprocessor directive");
    return skip_line(s);
  } else if (lexer->_ignore_line_directives)
    return skip_line(s);

  // parse line number
  for (_lineno = 0; s < _end && isdigit((unsigned char) *s); s++)
    _lineno = _lineno * 10 + *s - '0';
  _lineno--;			// account for extra line

  for (; s < _end && (*s == ' ' || *s == '\t'); s++)
    /* nada */;
  if (s < _end && *s == '\"') {
    // parse filename
    const char *first_in_filename = s;
    for (s++; s < _end && *s != '\"' && *s != '\n' && *s != '\r'; s++)
      if (*s == '\\' && s + 1 < _end && s[1] != '\n' && s[1] != '\r')
	s++;
    _filename = cp_unquote(_big_string.substring(first_in_filename, s) + "\"");
    // an empty filename means return to the input file's name
    if (!_filename)
      _filename = _original_filename;
  }

  // reach end of line
  for (; s < _end && *s != '\n' && *s != '\r'; s++)
    /* nada */;
  if (s + 1 < _end && *s == '\r' && s[1] == '\n')
    s++;
  return s;
}

Lexeme
LexerT::FileState::next_lexeme(LexerT *lexer)
{
  const char *s = _pos;
  while (true) {
    while (s < _end && isspace((unsigned char) *s)) {
      if (*s == '\n') {
	_lineno++;
	_lset->new_line(offset(s + 1), _filename, _lineno);
      } else if (*s == '\r') {
	if (s + 1 < _end && s[1] == '\n')
	  s++;
	_lineno++;
	_lset->new_line(offset(s + 1), _filename, _lineno);
      }
      s++;
    }
    const char *opos = s;
    if (s >= _end) {
      _pos = _end;
      return Lexeme(lexEOF, String(), _pos);
    } else if (*s == '/' && s + 1 < _end) {
      if (s[1] == '/')
	s = skip_line(s + 2);
      else if (s[1] == '*')
	s = skip_slash_star(s + 2);
      else
	break;
      lexer->_lexinfo->notify_comment(opos, s);
    } else if (*s == '#' && (s == _big_string.begin() || s[-1] == '\n' || s[-1] == '\r')) {
      s = process_line_directive(s, lexer);
      lexer->_lexinfo->notify_line_directive(opos, s);
    } else
      break;
  }

  const char *word_pos = s;

  // find length of current word
  if (isalnum((unsigned char) *s) || *s == '_' || *s == '@') {
   more_word_characters:
    s++;
    while (s < _end && (isalnum((unsigned char) *s) || *s == '_' || *s == '@'))
      s++;
    if (s + 1 < _end && *s == '/' && (isalnum((unsigned char) s[1]) || s[1] == '_' || s[1] == '@'))
      goto more_word_characters;
    _pos = s;
    String word = _big_string.substring(word_pos, s);
    if (word.equals("elementclass", 12)) {
      lexer->_lexinfo->notify_keyword(word, word_pos, s);
      return Lexeme(lexElementclass, word, word_pos);
    } else if (word.equals("require", 7)) {
      lexer->_lexinfo->notify_keyword(word, word_pos, s);
      return Lexeme(lexRequire, word, word_pos);
    } else if (word.equals("provide", 7)) {
      lexer->_lexinfo->notify_keyword(word, word_pos, s);
      return Lexeme(lexProvide, word, word_pos);
    } else if (word.equals("define", 6)) {
      lexer->_lexinfo->notify_keyword(word, word_pos, s);
      return Lexeme(lexDefine, word, word_pos);
    } else
      return Lexeme(lexIdent, word, word_pos);
  }

  // check for variable
  if (*s == '$') {
    s++;
    while (s < _end && (isalnum((unsigned char) *s) || *s == '_'))
      s++;
    if (s > word_pos + 1) {
      _pos = s;
      return Lexeme(lexVariable, _big_string.substring(word_pos + 1, s), word_pos);
    } else
      s--;
  }

  if (s + 1 < _end) {
    if (*s == '-' && s[1] == '>') {
      _pos = s + 2;
      return Lexeme(lexArrow, _big_string.substring(s, s + 2), word_pos);
    } else if (*s == '=' && s[1] == '>') {
      _pos = s + 2;
      return Lexeme(lex2Arrow, _big_string.substring(s, s + 2), word_pos);
    } else if (*s == ':' && s[1] == ':') {
      _pos = s + 2;
      return Lexeme(lex2Colon, _big_string.substring(s, s + 2), word_pos);
    } else if (*s == '|' && s[1] == '|') {
      _pos = s + 2;
      return Lexeme(lex2Bar, _big_string.substring(s, s + 2), word_pos);
    }
  }
  if (s + 2 < _end && *s == '.' && s[1] == '.' && s[2] == '.') {
    _pos = s + 3;
    return Lexeme(lex3Dot, _big_string.substring(s, s + 3), word_pos);
  }

  _pos = s + 1;
  return Lexeme(*s, _big_string.substring(s, s + 1), word_pos);
}

Lexeme
LexerT::FileState::lex_config(LexerT *lexer)
{
  const char *config_pos = _pos;
  const char *s = _pos;
  unsigned paren_depth = 1;

  for (; s < _end; s++)
    if (*s == '(')
      paren_depth++;
    else if (*s == ')') {
      paren_depth--;
      if (!paren_depth)
	break;
    } else if (*s == '\n') {
      _lineno++;
      _lset->new_line(offset(s + 1), _filename, _lineno);
    } else if (*s == '\r') {
      if (s + 1 < _end && s[1] == '\n')
	s++;
      _lineno++;
      _lset->new_line(offset(s + 1), _filename, _lineno);
    } else if (*s == '/' && s + 1 < _end) {
      if (s[1] == '/')
	s = skip_line(s + 2) - 1;
      else if (s[1] == '*')
	s = skip_slash_star(s + 2) - 1;
    } else if (*s == '\'' || *s == '\"')
      s = skip_quote(s + 1, *s) - 1;
    else if (*s == '\\' && s + 1 < _end && s[1] == '<')
      s = skip_backslash_angle(s + 2) - 1;

  _pos = s;
  lexer->_lexinfo->notify_config_string(config_pos, s);
  return Lexeme(lexConfig, _big_string.substring(config_pos, s),
		config_pos);
}

String
LexerT::lexeme_string(int kind)
{
    static const char names[] = "identifier\0variable\0'->'\0'=>'\0"
	"'::'\0'||'\0'...'\0'elementclass'\0'require'\0'provide'\0"
	"'define'";
    static const uint8_t offsets[] = {
	0, 11, 20, 25, 30, 35, 40, 46, 61, 71, 81, 90
    };
    static_assert(sizeof(names) == 90, "names screwup.");

    char buf[14];
    if (kind >= lexIdent && kind < lexIdent + (int) sizeof(offsets) - 1) {
	const uint8_t *op = offsets + (kind - lexIdent);
	return String::make_stable(names + op[0], op[1] - op[0] - 1);
    } else if (kind >= 32 && kind < 127) {
	sprintf(buf, "'%c'", kind);
	return buf;
    } else {
	sprintf(buf, "'\\%03d'", kind);
	return buf;
    }
}


// LEXING: MIDDLE LEVEL (WITH PUSHBACK)

bool
LexerT::expect(int kind, bool no_error)
{
  if (_unlex_pos) {
    if (_unlex[_unlex_pos - 1].is(kind)) {
      --_unlex_pos;
      return true;
    }
    if (!no_error)
      lerror(_unlex[_unlex_pos - 1], "expected %s", lexeme_string(kind).c_str());
  } else {
    // Never adds to _unlex, which requires a nonobvious implementation.
    String old_filename = _file._filename;
    unsigned old_lineno = _file._lineno;
    const char *old_pos = _file._pos;
    if (lex().is(kind))
      return true;
    _file._filename = old_filename;
    _file._lineno = old_lineno;
    if (!no_error)
      lerror(old_pos, _file._pos, "expected %s", lexeme_string(kind).c_str());
    _file._pos = old_pos;
  }
  return false;
}


// ERRORS

String
LexerT::FileState::landmark() const
{
    if (_filename && _filename.back() != ':' && !isspace((unsigned char) _filename.back()))
	return _filename + ":" + String(_lineno);
    else
	return _filename + String(_lineno);
}

void
LexerT::vlerror(const char *pos1, const char *pos2, const String &lm, const char *fmt, va_list val)
{
    String text = _errh->vformat(fmt, val);
    _lexinfo->notify_error(text, pos1, pos2);
    _errh->xmessage(lm, ErrorHandler::e_error, text);
}

int
LexerT::lerror(const char *pos1, const char *pos2, const char *format, ...)
{
    va_list val;
    va_start(val, format);
    vlerror(pos1, pos2, _file.landmark(), format, val);
    va_end(val);
    return -1;
}

int
LexerT::lerror(const Lexeme &t, const char *format, ...)
{
    va_list val;
    va_start(val, format);
    vlerror(t.pos1(), t.pos2(), _file.landmark(), format, val);
    va_end(val);
    return -1;
}


// ELEMENT TYPES

ElementClassT *
LexerT::element_type(const Lexeme &t) const
{
    assert(t.is(lexIdent));
    String name = t.string();
    ElementClassT *type = _router->declared_type(name);
    if (!type)
	type = _base_type_map[name];
    if (type)
	_lexinfo->notify_class_reference(type, t.pos1(), t.pos2());
    return type;
}

ElementClassT *
LexerT::force_element_type(const Lexeme &t)
{
    assert(t.is(lexIdent));
    String name = t.string();
    ElementClassT *type = _router->declared_type(name);
    if (!type)
	type = _base_type_map.get(name);
    if (!type) {
	if (_router->eindex(name) >= 0)
	    lerror(t, "%<%s%> was previously used as an element name", name.c_str());
	type = ElementClassT::base_type(name);
	_base_type_map.set(name, type);
    }
    _lexinfo->notify_class_reference(type, t.pos1(), t.pos2());
    return type;
}


// ELEMENTS

String
LexerT::anon_element_name(const String &class_name) const
{
    int anonymizer = _router->nelements() - _anonymous_offset + 1;
    return ";" + class_name + "@" + String(anonymizer);
}

int
LexerT::make_element(String name, const Lexeme &location, const char *decl_pos2,
		     ElementClassT *type, const String &conf)
{
    // check 'name' for validity
    for (int i = 0; i < name.length(); i++) {
	bool ok = false;
	for (; i < name.length() && name[i] != '/'; i++)
	    if (!isdigit((unsigned char) name[i]))
		ok = true;
	if (!ok) {
	    lerror(location, "element name %<%s%> has all-digit component", name.c_str());
	    break;
	}
    }
    const char *end_decl = (decl_pos2 ? decl_pos2 : location.pos2());
    ElementT *e = _router->get_element(name, type, conf, landmarkt(location.pos1(), location.pos2()));
    _lexinfo->notify_element_declaration(e, location.pos1(), location.pos2(), end_decl);
    return e->eindex();
}

int
LexerT::make_anon_element(const Lexeme &what, const char *decl_pos2,
			  ElementClassT *type, const String &conf)
{
    return make_element(anon_element_name(type->name()), what, decl_pos2, type, conf);
}

void
LexerT::connect(int element1, int port1, int port2, int element2, const char *pos1, const char *pos2)
{
    if (port1 < 0)
	port1 = 0;
    if (port2 < 0)
	port2 = 0;
    _router->add_connection
	(PortT(_router->element(element1), port1),
	 PortT(_router->element(element2), port2), landmarkt(pos1, pos2));
}


// PARSING

bool
LexerT::yport(Vector<int> &ports, const char *pos[2])
{
    Lexeme tlbrack = lex();
    pos[0] = tlbrack.pos1();
    if (!tlbrack.is('[')) {
	unlex(tlbrack);
	pos[1] = pos[0];
	return false;
    }

    int nports = ports.size();
    while (1) {
	Lexeme t = lex();
	if (t.is(lexIdent)) {
	    String p = t.string();
	    const char *ps = p.c_str();
	    int port = 0;
	    if (isdigit((unsigned char) ps[0]) || ps[0] == '-')
		port = strtol(ps, (char **)&ps, 0);
	    if (*ps != 0) {
		lerror(t, "syntax error: port number should be integer");
		port = 0;
	    }
	    ports.push_back(port);
	} else if (t.is(']')) {
	    if (nports == ports.size())
		ports.push_back(0);
	    ports.push_back(-1);
	    pos[1] = t.pos2();
	    return true;
	} else {
	    lerror(t, "syntax error: expected port number");
	    unlex(t);
	    return ports.size() != nports;
	}

	t = lex();
	if (t.is(']')) {
	    pos[1] = t.pos2();
	    return true;
	} else if (!t.is(',')) {
	    lerror(t, "syntax error: expected ','");
	    unlex(t);
	}
    }
}

namespace {
struct ElementState {
    String name;
    ElementClassT *type;
    ElementClassT *decl_type;
    bool bare;
    String configuration;
    Lexeme decl_lexeme;
    const char *decl_pos2;
    ElementState *next;
    ElementState(const String &name_, ElementClassT *type_, bool bare_,
		 const Lexeme &decl_lexeme_, const char *decl_pos2_,
		 ElementState **&tail)
	: name(name_), type(type_), decl_type(0), bare(bare_),
	  decl_lexeme(decl_lexeme_), decl_pos2(decl_pos2_), next(0) {
	*tail = this;
	tail = &next;
    }
};
}

// Returned result is a vector listing all elements and port references.
// The vector is a concatenated series of groups, each of which looks like:
// group[0] element index
// group[1] number of input ports
// group[2] number of output ports
// group[3...3+group[1]] input ports
// group[3+group[1]...3+group[1]+group[2]] output ports
// epos is used to store character positions.
// epos[0], [1] is beginning & end of first input port specification
// epos[2] is beginning of first element name
// epos[3], [4] is beginning & end of last output port specification
bool
LexerT::yelement(Vector<int> &result, bool in_allowed, const char *epos[5])
{
    ElementState *head = 0, **tail = &head;
    Vector<int> res;
    const char *xport_pos[2];
    bool any_implicit = false, any_ports = false;

    // parse list of names (which might include classes)
    Lexeme t;
    while (1) {
	int esize = res.size();
	res.push_back(-1);
	res.push_back(0);
	res.push_back(0);
	bool this_implicit = false;

	// initial port
	const char **inpos = head ? xport_pos : epos;
	yport(res, inpos);
	res[esize + 1] = res.size() - (esize + 3);

	// element name or class
	String name;
	ElementClassT *type;

	if (!head)
	    epos[2] = next_pos();
	t = lex();
	const char *decl_pos2 = t.pos2();
	if (t.is(lexIdent)) {
	    name = t.string();
	    type = element_type(t);
	} else if (t.is('{')) {
	    type = ycompound(String(), t.pos1(), t.pos1());
	    name = type->name();
	    decl_pos2 = next_pos();
	} else if (t.is('(')) {
	    name = anon_element_name("");
	    type = 0;
	    int group_nports[2];
	    ygroup(name, group_nports, t.pos1(), t.pos2());

	    // an anonymous group has implied, overridable port
	    // specifications on both sides for all inputs & outputs
	    for (int k = 0; k < 2; ++k)
		if (res[esize + 1 + k] == 0) {
		    res[esize + 1 + k] = group_nports[k];
		    for (int i = 0; i < group_nports[k]; ++i)
			res.push_back(i);
		}
	} else {
	    bool nested = _router->scope().depth() || _group_depth;
	    if (nested && (t.is(lexArrow) || t.is(lex2Arrow)))
		this_implicit = !in_allowed && (res[esize + 1] || !esize);
	    else if (nested && t.is(','))
		this_implicit = !!res[esize + 1];
	    else if (nested && !t.is(lex2Colon))
		this_implicit = in_allowed && (res[esize + 1] || !esize);
	    if (this_implicit) {
		any_implicit = true;
		name = (in_allowed ? "output" : "input");
		type = 0;
		if (!in_allowed)
		    click_swap(res[esize+1], res[esize+2]);
		unlex(t);
	    } else {
		if (res[esize + 1])
		    lerror(t, "stranded port ignored");
		res.resize(esize);
		if (esize == 0) {
		    if (in_allowed)
			unlex(t);
		    else
			lerror(t, "syntax error near %<%#s%>", t.string().c_str());
		    return false;
		}
		break;
	    }
	}

	ElementState *e = new ElementState(name, type, t.is(lexIdent), t, decl_pos2, tail);

	// ":: CLASS" declaration
	t = lex();
	if (t.is(lex2Colon) && !this_implicit) {
	    t = lex();
	    if (t.is(lexIdent))
		e->decl_type = force_element_type(t);
	    else if (t.is('{'))
		e->decl_type = ycompound(String(), t.pos1(), t.pos1());
	    else {
		lerror(t, "missing element type in declaration");
		e->decl_type = force_element_type(e->decl_lexeme);
		unlex(t);
	    }
	    e->bare = false;
	    t = lex();
	}

	// configuration string
	if (t.is('(') && !this_implicit) {
	    if (_router->element(e->name))
		lerror(t, "configuration string ignored on element reference");
	    e->configuration = lex_config().string();
	    expect(')');
	    e->decl_pos2 = next_pos();
	    e->bare = false;
	    t = lex();
	}

	// final port
	if (t.is('[') && !this_implicit) {
	    unlex(t);
	    if (res[esize + 2])	// delete any implied ports
		res.resize(esize + 3 + res[esize + 1]);
	    yport(res, epos + 3);
	    res[esize + 2] = res.size() - (esize + 3 + res[esize + 1]);
	    t = lex();
	}
	any_ports = any_ports || res[esize + 1] || res[esize + 2];

	if (!t.is(','))
	    break;
    }

    unlex(t);

    // maybe complain about implicits
    if (any_implicit && in_allowed && (t.is(lexArrow) || t.is(lex2Arrow)))
	lerror(t, "implicit ports used in the middle of a chain");

    // maybe spread class and configuration for standalone
    // multiple-element declaration
    if (head->next && !in_allowed && !(t.is(lexArrow) || t.is(lex2Arrow))
	&& !any_ports && !any_implicit) {
	ElementState *last = head;
	while (last->next && last->bare)
	    last = last->next;
	if (!last->next && last->decl_type)
	    for (ElementState *e = head; e->next; e = e->next) {
		e->decl_type = last->decl_type;
		e->configuration = last->configuration;
		e->decl_pos2 = last->decl_pos2;
	    }
    }

    // add elements
    int *resp = res.begin();
    while (ElementState *e = head) {
	if (e->type || (*resp = _router->eindex(e->name)) < 0) {
	    if (e->decl_type && (e->type || e->name == e->decl_type->name()))
		lerror(e->decl_lexeme, "class %<%s%> used as element name", e->name.c_str());
	    else if (!e->decl_type && !e->type)
		e->type = force_element_type(e->decl_lexeme);
	    if (e->type)
		e->name = anon_element_name(e->type->name());
	    *resp = make_element(e->name, e->decl_lexeme, e->decl_pos2, e->type ? e->type : e->decl_type, e->configuration);
	} else {
	    if (e->decl_type)
		ElementT::redeclaration_error(_errh, "element", e->name, _file.landmark(), _router->element(*resp)->landmark());
	    _lexinfo->notify_element_reference(_router->element(*resp), e->decl_lexeme.pos1(), e->decl_lexeme.pos2());
	}

	resp += 3 + resp[1] + resp[2];
	head = e->next;
	delete e;
    }

    result.swap(res);
    return true;
}

void
LexerT::yconnection_check_useless(const Vector<int> &x, bool isoutput, const char *epos[2])
{
    for (const int *it = x.begin(); it != x.end(); it += 3 + it[1] + it[2])
	if (it[isoutput ? 2 : 1] > 0) {
	    lerror(epos[0], epos[1], isoutput ? "output ports ignored at end of chain" : "input ports ignored at start of chain");
	    break;
	}
}

void
LexerT::yconnection_analyze_ports(const Vector<int> &x, bool isoutput,
				  int &min_ports, int &expandable)
{
    min_ports = 0;
    expandable = 0;
    for (const int *it = x.begin(); it != x.end(); it += 3 + it[1] + it[2]) {
	int n = it[isoutput ? 2 : 1];
	if (n <= 1)
	    min_ports += 1;
	else if (it[3 + (isoutput ? it[1] : 0) + n - 1] == -1) {
	    min_ports += n - 1;
	    ++expandable;
	} else
	    min_ports += n;
    }
}

void
LexerT::yconnection_connect_all(Vector<int> &outputs, Vector<int> &inputs,
				int connector,
				const char *pos1, const char *pos2)
{
    int minp[2];
    int expandable[2];
    yconnection_analyze_ports(outputs, true, minp[1], expandable[1]);
    yconnection_analyze_ports(inputs, false, minp[0], expandable[0]);

    if (expandable[0] + expandable[1] > 1) {
	lerror(pos1, pos2, "at most one expandable port allowed per connection");
	expandable[minp[0] < minp[1]] = 0;
    }

    if (connector == lex2Arrow)
	// '=>' can interpret missing ports as expandable ports
	for (int k = 0; k < 2; ++k) {
	    Vector<int> &myvec(k ? outputs : inputs);
	    if (minp[k] == 1 && minp[1-k] > 1 && myvec[1+k] == 0)
		expandable[k] = 1;
	}

    bool step[2];
    int nexpandable[2];
    for (int k = 0; k < 2; ++k) {
	step[k] = minp[k] > 1 || expandable[k];
	nexpandable[k] = expandable[k] ? minp[1-k] - minp[k] : 0;
    }

    if (step[0] && step[1]) {
	if (connector != lex2Arrow)
	    lerror(pos1, pos2, "syntax error: many-to-many connections require %<=>%>");
	if (!expandable[0] && !expandable[1] && minp[0] != minp[1])
	    lerror(pos1, pos2, "connection mismatch: %d outputs connected to %d inputs", minp[1], minp[0]);
	else if (!expandable[0] && minp[0] < minp[1])
	    lerror(pos1, pos2, "connection mismatch: %d or more outputs connected to %d inputs", minp[1], minp[0]);
	else if (!expandable[1] && minp[1] < minp[0])
	    lerror(pos1, pos2, "connection mismatch: %d outputs connected to %d or more inputs", minp[1], minp[0]);
    } else if (!step[0] && !step[1])
	step[0] = true;

    const int *it[2] = {inputs.begin(), outputs.begin()};
    int ppos[2] = {0, 0}, port[2] = {-1, -1};
    while (it[0] != inputs.end() && it[1] != outputs.end()) {
	for (int k = 0; k < 2; ++k)
	    if (port[k] < 0) {
		int np = it[k][1+k];
		port[k] = np ? it[k][3 + (k ? it[k][1] : 0)] : 0;
	    }

	connect(it[1][0], port[1], port[0], it[0][0], pos1, pos2);

	for (int k = 0; k < 2; ++k)
	    if (step[k]) {
		int np = it[k][1+k];
		const int *pvec = it[k] + 3 + (k ? it[k][1] : 0);
		++ppos[k];
		if (ppos[k] < np && pvec[ppos[k]] >= 0)
		    // port list
		    port[k] = pvec[ppos[k]];
		else if (np && pvec[np-1] == -1 && nexpandable[k] > 0) {
		    // expandable port
		    port[k] = pvec[np-2] + ppos[k] - (np-2);
		    --nexpandable[k];
		} else if (np == 0 && minp[k] == 1 && nexpandable[k] > 0) {
		    // missing port interpreted as expandable port
		    port[k] = ppos[k];
		    --nexpandable[k];
		} else {
		    // next element in comma-separated list
		    port[k] = -1;
		    ppos[k] = 0;
		    it[k] += 3 + it[k][1] + it[k][2];
		}
	    }
    }
}

bool
LexerT::yconnection()
{
    Vector<int> elements1, elements2;
    int connector = 0;
    const char *epos[5], *last_element_pos = next_pos();
    Lexeme t;

    while (true) {
	// get element
	elements2.clear();
	if (!yelement(elements2, !elements1.empty(), epos)) {
	    yconnection_check_useless(elements1, true, epos + 3);
	    return !elements1.empty();
	}

	if (elements1.empty())
	    yconnection_check_useless(elements2, false, epos);
	else
	    yconnection_connect_all(elements1, elements2, connector, last_element_pos, next_pos());

    relex:
	t = lex();
	switch (t.kind()) {

	case ',':
	case lex2Colon:
	    lerror(t, "syntax error before %<%s%>", t.string().c_str());
	    goto relex;

	case lexArrow:
	case lex2Arrow:
	    connector = t.kind();
	    break;

	case lexIdent:
	case '{':
	case '}':
	case '[':
	case ')':
	case lex2Bar:
	case lexElementclass:
	case lexRequire:
	case lexProvide:
	case lexDefine:
	    unlex(t);
	    // FALLTHRU
	case ';':
	case lexEOF:
	    yconnection_check_useless(elements2, true, epos + 3);
	    return true;

	default:
	    lerror(t, "syntax error near %<%#s%>", t.string().c_str());
	    if (t.kind() >= lexIdent)	// save meaningful tokens
		unlex(t);
	    return true;

	}

	// have 'x ->'
	elements1.swap(elements2);
    }
}

void
LexerT::yelementclass(const char *pos1)
{
    Lexeme tname = lex();
    String eclass_name;
    if (!tname.is(lexIdent)) {
	unlex(tname);
	lerror(tname, "expected element type name");
    } else {
	String n = tname.string();
	if (_router->eindex(n) >= 0)
	    lerror(tname, "%<%s%> already used as an element name", n.c_str());
	else
	    eclass_name = n;
    }

    Lexeme tnext = lex();
    if (tnext.is('{'))
	(void) ycompound(eclass_name, pos1, tname.pos1());

    else if (tnext.is(lexIdent)) {
	ElementClassT *ec = force_element_type(tnext);
	if (eclass_name) {
	    ElementClassT *new_ec = new SynonymElementClassT(eclass_name, ec, _router);
	    _router->add_declared_type(new_ec, false);
	    _lexinfo->notify_class_declaration(new_ec, false, pos1, tname.pos1(), tnext.pos2());
	}

    } else
	lerror(tnext, "syntax error near %<%#s%>", tnext.string().c_str());
}

void
LexerT::ydefine(RouterT *r, const String &fname, const String &ftype, const Lexeme &t, bool &scope_order_error)
{
    if (!r->add_formal(fname, ftype))
	lerror(t, "parameter %<$%s%> multiply defined", fname.c_str());
    else {
	if (ftype)
	    for (int i = 0; i < r->nformals() - 1; ++i)
		if (r->formal_type(i) == ftype) {
		    lerror(t, "repeated keyword parameter %<%s%> in compound element", ftype.c_str());
		    break;
		}
	if (!scope_order_error && r->nformals() > 1
	    && ((!ftype && r->formal_type(r->nformals() - 2))
		|| r->formal_type(r->nformals() - 2) == "__REST__")) {
	    lerror(t, "compound element parameters out of order\n(The correct order is %<[positional], [keywords], [__REST__]%>.)");
	    scope_order_error = true;
	}
    }
}

void
LexerT::ycompound_arguments(RouterT *comptype)
{
  Lexeme t1, t2;
  bool scope_order_error = false;

  while (1) {
    String vartype, varname;

    // read "IDENTIFIER $VARIABLE" or "$VARIABLE"
    t1 = lex();
    if (t1.is(lexIdent)) {
      t2 = lex();
      if (t2.is(lexVariable)) {
	vartype = t1.string();
	varname = t2.string();
      } else {
	if (comptype->nformals() > 0)
	  lerror(t2, "expected variable");
	unlex(t2);
	unlex(t1);
	break;
      }
    } else if (t1.is(lexVariable))
      varname = t1.string();
    else if (t1.is('|'))
      break;
    else {
      if (comptype->nformals() > 0)
	lerror(t1, "expected variable");
      unlex(t1);
      break;
    }

    ydefine(comptype, varname, vartype, t1, scope_order_error);

    Lexeme tsep = lex();
    if (tsep.is('|'))
      break;
    else if (!tsep.is(',')) {
      lerror(tsep, "expected %<,%> or %<|%>");
      unlex(tsep);
      break;
    }
  }
}

ElementClassT *
LexerT::ycompound(String name, const char *decl_pos1, const char *name_pos1)
{
    bool anonymous = (name.length() == 0);
    String printable_name = name;
    if (anonymous)
	printable_name = "<anonymous" + String(++_anonymous_class_count) + ">";

    // '{' was already read
    RouterT *old_router = _router;
    int old_offset = _anonymous_offset;
    int old_ngroups = _ngroups;

    RouterT *first = 0, *last = 0;
    ElementClassT *extension = 0;

    const char *class_pos1 = name_pos1;
    const char *pos2 = name_pos1;

    while (1) {
	Lexeme dots = lex();
	if (dots.is(lex3Dot)) {
	    // '...' marks an extension type
	    if (anonymous) {
		lerror(dots, "cannot extend anonymous compound element class");
		extension = ElementClassT::base_type("Error");
	    } else {
		extension = force_element_type(Lexeme(lexIdent, name, name_pos1));
		_lexinfo->notify_class_extension(extension, dots.pos1(), dots.pos2());
	    }

	    dots = lex();
	    if (!first || !dots.is('}'))
		lerror(dots.pos1(), dots.pos2(), "%<...%> should occur last, after one or more compounds");
	    if (dots.is('}') && first)
		break;
	}
	unlex(dots);

	// create a compound
	RouterT *compound_class = new RouterT(name, landmarkt(class_pos1, dots.pos1()), old_router);
	compound_class->set_printable_name(printable_name);
	_router = compound_class->cast_router();
	_anonymous_offset = 2;
	_ngroups = 0;

	ycompound_arguments(compound_class);
	while (ystatement('}'))
	    /* nada */;

	compound_class->finish_type(_errh);
	if (_ngroups && _expand_groups) {
	    compound_class->remove_tunnels();
	    compound_class->compact();
	}

	if (last)
	    last->set_overload_type(compound_class);
	else
	    first = compound_class;
	last = compound_class;

	// check for '||' or '}'
	Lexeme t = lex();
	compound_class->set_landmarkt(landmarkt(class_pos1, t.pos2()));
	class_pos1 = t.pos1();
	if (!t.is(lex2Bar)) {
	    pos2 = t.pos2();
	    break;
	}
    }

    _anonymous_offset = old_offset;
    _ngroups = old_ngroups;
    _router = old_router;

    if (extension)
	last->set_overload_type(extension);
    old_router->add_declared_type(first, anonymous);
    _lexinfo->notify_class_declaration(first, anonymous, decl_pos1, name_pos1, pos2);
    return first;
}

void
LexerT::ygroup(String name, int group_nports[2], const char *open_pos1, const char *open_pos2)
{
    LandmarkT landmark = landmarkt(open_pos1, open_pos2);
    LandmarkErrorHandler lerrh(_errh, landmark.decorated_str());
    String name_slash = name + "/";
    _router->add_tunnel(name, name_slash + "input", landmark, &lerrh);
    _router->add_tunnel(name_slash + "output", name, landmark, &lerrh);
    ElementT *new_input = _router->element(name_slash + "input");
    ElementT *new_output = _router->element(name_slash + "output");

    int old_input = _router->__map_element_name("input", new_input->eindex());
    int old_output = _router->__map_element_name("output", new_output->eindex());
    ++_group_depth;
    ++_ngroups;

    while (ystatement(')'))
	/* nada */;
    expect(')');

    // check that all inputs and outputs are used
    lerrh.set_landmark(landmarkt(open_pos1, next_pos()).decorated_str());
    const char *printable_name = (name[0] == ';' ? "<anonymous group>" : name.c_str());
    group_nports[0] = _router->check_pseudoelement(new_input, false, printable_name, &lerrh);
    group_nports[1] = _router->check_pseudoelement(new_output, true, printable_name, &lerrh);

    --_group_depth;
    _router->__map_element_name("input", old_input);
    _router->__map_element_name("output", old_output);
}

void
LexerT::yrequire_library(const Lexeme &lexeme, const String &value)
{
    if (_router->scope().depth()) {
	lerror(lexeme, "%<require library%> must be used at file scope");
	return;
    }

    String dir = _file._filename;
    int pos = dir.find_right('/');
    if (pos > 0)
	dir = dir.substring(0, pos);
    else
	dir = ".";
    String fn = clickpath_find_file(value, "conf", dir, 0);
    if (!fn) {
	lerror(lexeme, "library %<%#s%> not found in CLICKPATH/conf", fn.c_str());
	return;
    }

    for (String *it = _libraries.begin(); it != _libraries.end(); ++it)
	if (*it == fn)
	    return;
    _libraries.push_back(fn);

    LandmarkErrorHandler lerrh(_errh, _file.landmark());
    int before = lerrh.nerrors();
    String data = file_string(fn, &lerrh);
    if (lerrh.nerrors() != before)
	return;

    FileState old_file(_file);
    _file = FileState(data, fn);
    while (ystatement(0))
	/* do nothing */;
    _file = old_file;
}

void
LexerT::yrequire()
{
    if (!expect('('))
	return;

    Lexeme requirement = lex_config();
    expect(')');
    // pre-read ';' to make it easier to write parsing extensions
    expect(';', true);

    Vector<String> args;
    cp_argvec(requirement.string(), args);

    String compact_config_str = String::make_stable("compact_config", 14);
    String package_str = String::make_stable("package", 7);
    String library_str = String::make_stable("library", 7);

    for (int i = 0; i < args.size(); i++) {
	Vector<String> words;
	cp_spacevec(args[i], words);
	if (words.size() == 0)
	    continue;		// do nothing

	String type, value;
	(void) cp_word(words[0], &type);
	// "require(UNKNOWN)" means "require(package UNKNOWN)"
	if (type && type != compact_config_str && type != package_str
	    && words.size() == 1) {
	    words.push_back(type);
	    type = package_str;
	}

	if (type == compact_config_str && words.size() == 1)
	    /* OK */;
	else if (type == package_str && words.size() == 2
		 && cp_string(words[1], &value))
	    /* OK */;
	else if (type == library_str && words.size() == 2
		 && cp_string(words[1], &value)) {
	    yrequire_library(requirement, value);
	    continue;
	} else {
	    lerror(requirement, "syntax error at requirement");
	    continue;
	}

	_router->add_requirement(type, value);
    }
}

void
LexerT::yvar()
{
    if (expect('(')) {
	Lexeme vars = lex_config();
	expect(')');

	Vector<String> args;
	String word;
	cp_argvec(vars.string(), args);
	for (int i = 0; i < args.size(); i++)
	    if (args[i]) {
		String var = cp_shift_spacevec(args[i]);
		const char *s = var.begin();
		if (s != var.end() && *s == '$')
		    for (s++; s != var.end() && (isalnum((unsigned char) *s) || *s == '_'); s++)
			/* nada */;
		if (var.length() < 2 || s != var.end())
		    lerror(vars, "bad %<define%> declaration: not a variable");
		else {
		    var = var.substring(1);
		    if (!_router->define(var, args[i]))
			lerror(vars, "parameter %<%s%> multiply defined", var.c_str());
		}
	    }
    }
}

bool
LexerT::ystatement(int nested)
{
  Lexeme t = lex();
  switch (t.kind()) {

   case lexIdent:
   case '[':
   case '{':
   case '(':
   case lexArrow:
   case lex2Arrow:
    unlex(t);
    yconnection();
    return true;

   case lexElementclass:
    yelementclass(t.pos1());
    return true;

   case lexRequire:
    yrequire();
    return true;

   case lexDefine:
    yvar();
    return true;

   case ';':
    return true;

   case '}':
   case lex2Bar:
    if (nested != '}')
      goto syntax_error;
    unlex(t);
    return false;

   case ')':
    if (nested != ')')
      goto syntax_error;
    unlex(t);
    return false;

   case lexEOF:
    if (nested)
      lerror(t, "expected %<%c%>", nested);
    return false;

   default:
   syntax_error:
    lerror(t, "syntax error near %<%#s%>", t.string().c_str());
    return true;

  }
}


// COMPLETION

RouterT *
LexerT::finish(const VariableEnvironment &global_scope)
{
    RouterT *r = _router;
    _router = 0;
    r->redefine(global_scope);
    // resolve anonymous element names
    r->assign_element_names();
    if (_ngroups && _expand_groups) {
	r->remove_tunnels();
	r->compact();
    }
    // returned router has one reference count
    return r;
}
