// -*- c-basic-offset: 4 -*-
/*
 * lexert.{cc,hh} -- configuration file parser for tools
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2004-2011 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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
    _ignore_line_directives(ignore_line_directives),
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
      else if (s[1] == '\"')
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
    char buf[14];
    if (kind == lexIdent)
	return String::make_stable("identifier", 10);
    else if (kind == lexVariable)
	return String::make_stable("variable", 8);
    else if (kind == lexArrow)
	return String::make_stable("'->'", 4);
    else if (kind == lex2Colon)
	return String::make_stable("'::'", 4);
    else if (kind == lex2Bar)
	return String::make_stable("'||'", 4);
    else if (kind == lex3Dot)
	return String::make_stable("'...'", 5);
    else if (kind == lexElementclass)
	return String::make_stable("'elementclass'", 14);
    else if (kind == lexRequire)
	return String::make_stable("'require'", 9);
    else if (kind == lexProvide)
	return String::make_stable("'provide'", 9);
    else if (kind == lexDefine)
	return String::make_stable("'define'", 8);
    else if (kind >= 32 && kind < 127) {
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
LexerT::yport(int &port, const char *pos[2])
{
    Lexeme tlbrack = lex();
    pos[0] = tlbrack.pos1();
    if (!tlbrack.is('[')) {
	unlex(tlbrack);
	pos[1] = pos[0];
	return false;
    }

    Lexeme tword = lex();
    if (tword.is(lexIdent)) {
	String p = tword.string();
	const char *ps = p.c_str();
	if (isdigit((unsigned char) ps[0]) || ps[0] == '-')
	    port = strtol(ps, (char **)&ps, 0);
	if (*ps != 0) {
	    lerror(tword, "syntax error: port number should be integer");
	    port = 0;
	}
	expect(']');
	pos[1] = next_pos();
	return true;
    } else if (tword.is(']')) {
	lerror(tword, "syntax error: expected port number");
	port = 0;
	pos[1] = tword.pos2();
	return true;
    } else {
	lerror(tword, "syntax error: expected port number");
	unlex(tword);
	return false;
    }
}

static void
add_portset(Vector<int> &ports, const int *take, int ntake, int nwant)
{
    for (int j = 0; j < ntake; ++j, ++take)
	ports.push_back(*take);
    if (ntake == 0 && nwant > 0)
	ports.push_back(0);
    for (int j = ntake ? ntake : 1; j < nwant; ++j)
	ports.push_back(-1);
}

static int
add_port(Vector<int> &ports, int ne, int oldnp, int newport)
{
    int newnp = newport < 0 ? oldnp : 1;
    if (oldnp < newnp) {
	Vector<int> newports;
	for (int i = 0, *p = ports.begin(); i < ne; ++i, p += oldnp)
	    add_portset(newports, p, oldnp, newnp);
	newports.swap(ports);
    } else
	newnp = oldnp;
    if (newport < 0 && newnp)
	newport = 0;
    add_portset(ports, &newport, 1, newnp);
    return newnp;
}

// Returned result is a vector with at least 2 elements.
// [0] number of input ports per element
// [1] number of output ports per element
// [2] first element index
// [3..X] input ports specified for first element index
// [X+1..Y] output ports specified for first element index
// [Y+1] second element index, and so forth.
// epos is used to store character positions.
// epos[0], [1] is beginning & end of first input port specification
// epos[2] is beginning of first element name
// epos[3], [4] is beginning & end of last output port specification
bool
LexerT::yelement(Vector<int> &result, bool in_allowed, const char *epos[5])
{
    Vector<String> names;
    Vector<Lexeme> decl_lexeme;
    Vector<const char *> decl_pos2;
    Vector<ElementClassT *> types;
    Vector<String> configurations;
    Vector<int> inports, outports;
    int ninports = 0, noutports = 0;
    const char *xport_pos[2];

    // parse list of names (which might include classes)
    Lexeme t;
    while (1) {
	// initial port
	int iport = -1;
	const char **inpos = names.size() ? xport_pos : epos;
	yport(iport, inpos);
	if (iport != -1 && !in_allowed) {
	    lerror(inpos[0], inpos[1], "input port useless at start of chain");
	    iport = -1;
	}
	ninports = add_port(inports, names.size(), ninports, iport);

	// element name or class
	if (names.empty())
	    epos[2] = next_pos();
	t = lex();
	const char *my_decl_pos2 = 0;
	if (t.is(lexIdent)) {
	    names.push_back(t.string());
	    types.push_back(element_type(t));
	    my_decl_pos2 = t.pos2();
	} else if (t.is('{')) {
	    types.push_back(ycompound(String(), t.pos1(), t.pos1()));
	    names.push_back(types.back()->name());
	    my_decl_pos2 = next_pos();
	} else if (names.empty() && types.empty()) {
	    unlex(t);
	    return false;
	} else
	    break;

	decl_lexeme.push_back(t);

	// configuration string
	t = lex();
	if (t.is('(')) {
	    if (!types.back())
		types.back() = force_element_type(decl_lexeme.back());
	    configurations.push_back(lex_config().string());
	    expect(')');
	    my_decl_pos2 = next_pos();
	    t = lex();
	} else
	    configurations.push_back(String());

	decl_pos2.push_back(my_decl_pos2);

	// final port
	int oport = -1;
	if (t.is('[')) {
	    unlex(t);
	    yport(oport, epos + 3);
	    t = lex();
	}
	noutports = add_port(outports, names.size() - 1, noutports, oport);

	if (!t.is(','))
	    break;
    }

    // parse ":: CLASS [(CONFIGSTRING)]"
    if (t.is(lex2Colon)) {
	t = lex();
	ElementClassT *decl_etype;
	if (t.is(lexIdent))
	    decl_etype = force_element_type(t);
	else if (t.is('{'))
	    decl_etype = ycompound(String(), t.pos1(), t.pos1());
	else {
	    lerror(t, "missing element type in declaration");
	    decl_etype = force_element_type(decl_lexeme[0]);
	}

	String decl_configuration;
	if (expect('(', true)) {
	    decl_configuration = lex_config().string();
	    expect(')');
	}

	const char *my_decl_pos2 = (names.size() == 1 ? next_pos() : 0);

	for (int i = 0; i < types.size(); ++i)
	    if (types[i] || names[i] == decl_etype->name())
		lerror(decl_lexeme[i], "class %<%s%> used as element name", names[i].c_str());
	    else if (ElementT *old_e = _router->element(names[i]))
		ElementT::redeclaration_error(_errh, "element", names[i], _file.landmark(), old_e->landmark());
	    else
		make_element(names[i], decl_lexeme[i], my_decl_pos2, decl_etype, decl_configuration);

	// parse optional output port after declaration
	if (names.size() == 1 && noutports == 0) {
	    int oport = -1;
	    yport(oport, epos + 3);
	    noutports = add_port(outports, names.size() - 1, noutports, oport);
	}

    } else
	unlex(t);

    // add elements
    int *inp = inports.begin(), *outp = outports.begin();
    result.push_back(ninports);
    result.push_back(noutports);
    for (int i = 0; i < names.size(); ++i) {
	int e;
	if (types[i])
	    e = make_anon_element(decl_lexeme[i], decl_pos2[i], types[i], configurations[i]);
	else {
	    e = _router->eindex(names[i]);
	    if (e < 0) {
		// assume it's an element type
		ElementClassT *etype = force_element_type(decl_lexeme[i]);
		e = make_anon_element(decl_lexeme[i], decl_lexeme[i].pos2(), etype, configurations[i]);
	    } else
		_lexinfo->notify_element_reference(_router->element(e), decl_lexeme[i].pos1(), decl_lexeme[i].pos2());
	}
	result.push_back(e);
	for (int j = 0; j < ninports; ++j, ++inp)
	    result.push_back(*inp);
	for (int j = 0; j < noutports; ++j, ++outp)
	    result.push_back(*outp);
    }

    return true;
}

bool
LexerT::yconnection()
{
    Vector<int> elements1, elements2;
    const char *epos[5], *last_element_pos = next_pos();
    Lexeme t;

    while (true) {
	// get element
	elements2.clear();
	if (!yelement(elements2, !elements1.empty(), epos)) {
	    if (elements1.size() && elements1[1] >= 0)
		lerror(epos[3], epos[4], "output ports useless at end of chain");
	    return !elements1.empty();
	}

	if (!elements1.empty()) {
	    int nin1 = elements1[0], nout1 = elements1[1];
	    int nin2 = elements2[0], nout2 = elements2[1];
	    for (int *e1 = elements1.begin() + 2; e1 != elements1.end(); e1 += 1 + nin1 + nout1)
		for (int *e2 = elements2.begin() + 2; e2 != elements2.end(); e2 += 1 + nin2 + nout2) {
		    int port1 = nout1 ? e1[1 + nin1] : 0;
		    int port2 = nin2 ? e2[1] : 0;
		    connect(e1[0], port1, port2, e2[0], last_element_pos, next_pos());
		}
	} else if (elements2[0])
	    lerror(epos[0], epos[1], "input ports useless at start of chain");

    relex:
	t = lex();
	switch (t.kind()) {

	case ',':
	case lex2Colon:
	case '[':
	    lerror(t, "syntax error before %<%s%>", t.string().c_str());
	    goto relex;

	case lexArrow:
	    break;

	case lexIdent:
	case '{':
	case '}':
	case lex2Bar:
	case lexElementclass:
	case lexRequire:
	case lexProvide:
	case lexDefine:
	    unlex(t);
	    // FALLTHRU
	case ';':
	case lexEOF:
	    if (elements2[1])
		lerror(epos[3], epos[4], "output ports useless at end of chain");
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

	ycompound_arguments(compound_class);
	while (ystatement(true))
	    /* nada */;

	compound_class->finish_type(_errh);

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
    _router = old_router;

    if (extension)
	last->set_overload_type(extension);
    old_router->add_declared_type(first, anonymous);
    _lexinfo->notify_class_declaration(first, anonymous, decl_pos1, name_pos1, pos2);
    return first;
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
    while (ystatement(false))
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
LexerT::ystatement(bool nested)
{
  Lexeme t = lex();
  switch (t.kind()) {

   case lexIdent:
   case '[':
   case '{':
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
    if (!nested)
      goto syntax_error;
    unlex(t);
    return false;

   case lexEOF:
    if (nested)
      lerror(t, "expected %<}%>");
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
    // returned router has one reference count
    return r;
}
