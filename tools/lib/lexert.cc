// -*- c-basic-offset: 4 -*-
/*
 * lexert.{cc,hh} -- configuration file parser for tools
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2004-2007 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static LexerTInfo *stub_lexinfo = 0;

LexerT::LexerT(ErrorHandler *errh, bool ignore_line_directives)
  : _data(0), _end(0), _pos(0), _lineno(1), _lset(0),
    _ignore_line_directives(ignore_line_directives),
    _tpos(0), _tfull(0), _router(0), _base_type_map(0), _errh(errh)
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
    _lset->unref();
}

void
LexerT::reset(const String &data, const Vector<ArchiveElement> &archive, const String &filename)
{
    clear();
  
    _big_string = data;
    _data = _pos = _big_string.begin();
    _end = _big_string.end();

    _original_filename = _filename = filename;
    _lineno = 1;
    _lset->new_line(0, _filename, _lineno);

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
    if (_lset)
	_lset->unref();
    _router = new RouterT();
    _router->use();		// hold a reference to the router
    _lset = new LandmarkSetT();

    _big_string = "";
    // _data was freed by _big_string
    _data = 0;
    _end = 0;
    _pos = 0;
    _filename = "";
    _lineno = 0;
    _tpos = 0;
    _tfull = 0;

    _base_type_map.clear();
    _anonymous_offset = 0;
    _anonymous_class_count = 0;
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
    return _big_string.substring(_pos, _end);
}

void
LexerT::set_remaining_text(const String &s)
{
    _big_string = s;
    _data = _pos = s.begin();
    _end = s.end();
}

const char *
LexerT::skip_line(const char *s)
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
LexerT::skip_slash_star(const char *s)
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
LexerT::skip_backslash_angle(const char *s)
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
LexerT::skip_quote(const char *s, char endc)
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
LexerT::process_line_directive(const char *s)
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
    lerror(first_pos, s, "unknown preprocessor directive");
    return skip_line(s);
  } else if (_ignore_line_directives)
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
    if (_filename == "")
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
LexerT::next_lexeme()
{
  const char *s = _pos;
  while (true) {
    while (s < _end && isspace((unsigned char) *s)) {
      if (*s == '\n') {
	_lineno++;
	_lset->new_line(s + 1 - _big_string.begin(), _filename, _lineno);
      } else if (*s == '\r') {
	if (s + 1 < _end && s[1] == '\n')
	  s++;
	_lineno++;
	_lset->new_line(s + 1 - _big_string.begin(), _filename, _lineno);
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
      _lexinfo->notify_comment(opos, s);
    } else if (*s == '#' && (s == _data || s[-1] == '\n' || s[-1] == '\r')) {
      s = process_line_directive(s);
      _lexinfo->notify_line_directive(opos, s);
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
      _lexinfo->notify_keyword(word, word_pos, s);
      return Lexeme(lexElementclass, word, word_pos);
    } else if (word.equals("require", 7)) {
      _lexinfo->notify_keyword(word, word_pos, s);
      return Lexeme(lexRequire, word, word_pos);
    } else if (word.equals("define", 6)) {
      _lexinfo->notify_keyword(word, word_pos, s);
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
LexerT::lex_config()
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
      _lset->new_line(s + 1 - _big_string.begin(), _filename, _lineno);
    } else if (*s == '\r') {
      if (s + 1 < _end && s[1] == '\n')
	s++;
      _lineno++;
      _lset->new_line(s + 1 - _big_string.begin(), _filename, _lineno);
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
  _lexinfo->notify_config_string(config_pos, s);
  return Lexeme(lexConfig, _big_string.substring(config_pos, s),
		config_pos);
}

String
LexerT::lexeme_string(int kind)
{
  char buf[12];
  if (kind == lexIdent)
    return "identifier";
  else if (kind == lexVariable)
    return "variable";
  else if (kind == lexArrow)
    return "'->'";
  else if (kind == lex2Colon)
    return "'::'";
  else if (kind == lex2Bar)
    return "'||'";
  else if (kind == lex3Dot)
    return "'...'";
  else if (kind == lexElementclass)
    return "'elementclass'";
  else if (kind == lexRequire)
    return "'require'";
  else if (kind == lexDefine)
    return "'define'";
  else if (kind >= 32 && kind < 127) {
    sprintf(buf, "'%c'", kind);
    return buf;
  } else {
    sprintf(buf, "'\\%03d'", kind);
    return buf;
  }
}


// LEXING: MIDDLE LEVEL (WITH PUSHBACK)

const Lexeme &
LexerT::lex()
{
  int p = _tpos;
  if (_tpos == _tfull) {
    _tcircle[p] = next_lexeme();
    _tfull = (_tfull + 1) % TCIRCLE_SIZE;
  }
  _tpos = (_tpos + 1) % TCIRCLE_SIZE;
  return _tcircle[p];
}

void
LexerT::unlex(const Lexeme &t)
{
  _tpos = (_tpos + TCIRCLE_SIZE - 1) % TCIRCLE_SIZE;
  _tcircle[_tpos] = t;
  assert(_tfull != _tpos);
}

bool
LexerT::expect(int kind, bool report_error)
{
    // Never adds anything to '_tcircle'. This requires a nonobvious
    // implementation.
    if (_tpos != _tfull) {
	if (_tcircle[_tpos].is(kind)) {
	    _tpos = (_tpos + 1) % TCIRCLE_SIZE;
	    return true;
	}
	if (report_error)
	    lerror(_tcircle[_tpos], "expected %s", lexeme_string(kind).c_str());
    } else {
	String old_filename = _filename;
	unsigned old_lineno = _lineno;
	const char *old_pos = _pos;
	if (next_lexeme().is(kind))
	    return true;
	_filename = old_filename;
	_lineno = old_lineno;
	if (report_error)
	    lerror(old_pos, _pos, "expected %s", lexeme_string(kind).c_str());
	_pos = old_pos;
    }
    return false;
}

const char *
LexerT::next_pos() const
{
    if (_tpos != _tfull)
	return _tcircle[_tpos].pos1();
    else
	return _pos;
}


// ERRORS

String
LexerT::landmark() const
{
    if (_filename && _filename.back() != ':' && !isspace((unsigned char) _filename.back()))
	return _filename + ":" + String(_lineno);
    else
	return _filename + String(_lineno);
}

void
LexerT::vlerror(const char *pos1, const char *pos2, const String &lm, const char *fmt, va_list val)
{
    String text = _errh->format(fmt, val);
    _lexinfo->notify_error(text, pos1, pos2);
    _errh->xmessage(lm, ErrorHandler::e_error, text);
}

int
LexerT::lerror(const char *pos1, const char *pos2, const char *format, ...)
{
    va_list val;
    va_start(val, format);
    vlerror(pos1, pos2, landmark(), format, val);
    va_end(val);
    return -1;
}

int
LexerT::lerror(const Lexeme &t, const char *format, ...)
{
    va_list val;
    va_start(val, format);
    vlerror(t.pos1(), t.pos2(), landmark(), format, val);
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
	    lerror(t, "'%s' was previously used as an element name", name.c_str());
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
	    lerror(location, "element name '%s' has all-digit component", name.c_str());
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
LexerT::yport(int &port, const char *&pos1, const char *&pos2)
{
    const Lexeme &tlbrack = lex();
    pos1 = tlbrack.pos1();
    if (!tlbrack.is('[')) {
	unlex(tlbrack);
	pos2 = pos1;
	return false;
    }

    const Lexeme &tword = lex();
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
	pos2 = next_pos();
	return true;
    } else if (tword.is(']')) {
	lerror(tword, "syntax error: expected port number");
	port = 0;
	pos2 = tword.pos2();
	return true;
    } else {
	lerror(tword, "syntax error: expected port number");
	unlex(tword);
	return false;
    }
}

bool
LexerT::yelement(int &element, bool comma_ok)
{
    Lexeme tname = lex();
    String name;
    ElementClassT *etype;
    const char *decl_pos2 = 0;

    if (tname.is(lexIdent)) {
	etype = element_type(tname);
	name = tname.string();
	decl_pos2 = tname.pos2();
    } else if (tname.is('{')) {
	etype = ycompound(String(), tname.pos1(), tname.pos1());
	name = etype->name();
	decl_pos2 = next_pos();
    } else {
	unlex(tname);
	return false;
    }
    
    Lexeme configuration;
    const Lexeme &tparen = lex();
    if (tparen.is('(')) {
	if (!etype)
	    etype = force_element_type(tname);
	configuration = lex_config();
	expect(')');
	decl_pos2 = next_pos();
    } else
	unlex(tparen);

    if (etype)
	element = make_anon_element(tname, decl_pos2, etype, configuration.string());
    else {
	const Lexeme &t2colon = lex();
	unlex(t2colon);
	if (t2colon.is(lex2Colon) || (t2colon.is(',') && comma_ok)) {
	    ydeclaration(tname);
	    element = _router->eindex(name);
	} else {
	    element = _router->eindex(name);
	    if (element < 0) {
		// assume it's an element type
		etype = force_element_type(tname);
		element = make_anon_element(tname, tname.pos2(), etype, configuration.string());
	    } else
		_lexinfo->notify_element_reference(_router->element(element), tname.pos1(), tname.pos2());
	}
    }

    return true;
}

void
LexerT::ydeclaration(const Lexeme &first_element)
{
    Vector<Lexeme> decls;
    Lexeme t;

    if (first_element) {
	decls.push_back(first_element);
	goto midpoint;
    }

    while (true) {
	t = lex();
	if (!t.is(lexIdent))
	    lerror(t, "syntax error: expected element name");
	else
	    decls.push_back(t);
    
      midpoint:
	const Lexeme &tsep = lex();
	if (tsep.is(','))
	    /* do nothing */;
	else if (tsep.is(lex2Colon))
	    break;
	else {
	    lerror(tsep, "syntax error: expected '::' or ','");
	    unlex(tsep);
	    return;
	}
    }

    ElementClassT *etype;
    Lexeme etypet = lex();
    if (etypet.is(lexIdent))
	etype = force_element_type(etypet);
    else if (etypet.is('{'))
	etype = ycompound(String(), etypet.pos1(), etypet.pos1());
    else {
	lerror(etypet, "missing element type in declaration");
	return;
    }

    Lexeme configuration;
    t = lex();
    if (t.is('(')) {
	configuration = lex_config();
	expect(')');
    } else
	unlex(t);

    const char *decl_pos2 = (decls.size() == 1 ? next_pos() : 0);

    for (int i = 0; i < decls.size(); i++) {
	String name = decls[i].string();
	if (ElementT *old_e = _router->element(name))
	    ElementT::redeclaration_error(_errh, "element", name, landmark(), old_e->landmark());
	else if (_router->declared_type(name) || _base_type_map.get(name))
	    lerror(decls[i], "class '%s' used as element name", name.c_str());
	else
	    make_element(name, decls[i], decl_pos2, etype, configuration.string());
    }
}

bool
LexerT::yconnection()
{
    int element1 = -1, port1 = -1;
    const char *last_element_pos = next_pos(), *port1_pos1 = 0, *port1_pos2 = 0;
    Lexeme t;

    while (true) {
	int element2, port2 = -1;
	const char *port2_pos1, *port2_pos2, *next_element_pos;

	// get element
	yport(port2, port2_pos1, port2_pos2);
	next_element_pos = next_pos();
	if (!yelement(element2, element1 < 0)) {
	    if (port1 >= 0)
		lerror(port1_pos1, port1_pos2, "output port useless at end of chain");
	    return element1 >= 0;
	}

	if (element1 >= 0)
	    connect(element1, port1, port2, element2, last_element_pos, next_pos());
	else if (port2 >= 0)
	    lerror(port2_pos1, port2_pos2, "input port useless at start of chain");
    
	port1 = -1;
	port1_pos1 = port1_pos2 = 0;
	last_element_pos = next_element_pos;
    
      relex:
	t = lex();
	switch (t.kind()) {
      
	  case ',':
	  case lex2Colon:
	    if (router()->element(element2)->anonymous())
		// type used as name
		lerror(t, "class '%s' used as element name", router()->etype_name(element2).c_str());
	    else
		lerror(t, "syntax error before '%s'", t.string().c_str());
	    goto relex;
      
	  case lexArrow:
	    break;
      
	  case '[':
	    unlex(t);
	    yport(port1, port1_pos1, port1_pos2);
	    goto relex;
      
	  case lexIdent:
	  case '{':
	  case '}':
	  case lex2Bar:
	  case lexElementclass:
	  case lexRequire:
	  case lexDefine:
	    unlex(t);
	    // FALLTHRU
	  case ';':
	  case lexEOF:
	    if (port1 >= 0)
		lerror(port1_pos1, port1_pos2, "output port useless at end of chain", port1);
	    return true;
      
	  default:
	    lerror(t, "syntax error near '%#s'", t.string().c_str());
	    if (t.kind() >= lexIdent)	// save meaningful tokens
		unlex(t);
	    return true;
      
	}
    
	// have 'x ->'
	element1 = element2;
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
	    lerror(tname, "'%s' already used as an element name", n.c_str());
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
	lerror(tnext, "syntax error near '%#s'", tnext.string().c_str());
}

void
LexerT::ydefine(RouterT *r, const String &fname, const String &ftype, bool isformal, const Lexeme &t, bool &scope_order_error)
{
    if (!r->define(fname, ftype, isformal))
	lerror(t, "parameter '$%s' multiply defined", fname.c_str());
    else if (isformal) {
	if (ftype)
	    for (int i = 0; i < r->scope().size() - 1; i++)
		if (r->scope().value(i) == ftype) {
		    lerror(t, "repeated keyword parameter '%s' in compound element", ftype.c_str());
		    break;
		}
	if (!scope_order_error && r->nformals() > 1
	    && ((!ftype && r->scope().value(r->nformals() - 2))
		|| r->scope().value(r->nformals() - 2) == "__REST__")) {
	    lerror(t, "compound element parameters out of order\n(The correct order is '[positional], [keywords], [__REST__]'.)");
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

    ydefine(comptype, varname, vartype, true, t1, scope_order_error);

    const Lexeme &tsep = lex();
    if (tsep.is('|'))
      break;
    else if (!tsep.is(',')) {
      lerror(tsep, "expected ',' or '|'");
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
		lerror(dots.pos1(), dots.pos2(), "'...' should occur last, after one or more compounds");
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
	const Lexeme &t = lex();
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
LexerT::yrequire()
{
    if (expect('(')) {
	Lexeme requirement = lex_config();
	expect(')');
	// pre-read ';' to make it easier to write parsing extensions
	expect(';', false);

	Vector<String> args;
	String word;
	cp_argvec(requirement.string(), args);
	for (int i = 0; i < args.size(); i++) {
	    Vector<String> words;
	    cp_spacevec(args[i], words);
	    if (words.size() == 0)
		/* do nothing */;
	    else if (!cp_word(words[0], &word))
		lerror(requirement, "bad requirement: not a word");
	    else if (words.size() > 1)
		lerror(requirement, "bad requirement: too many words");
	    else
		_router->add_requirement(word);
	}
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
		String var = cp_pop_spacevec(args[i]);
		const char *s = var.begin();
		if (s != var.end() && *s == '$')
		    for (s++; s != var.end() && (isalnum((unsigned char) *s) || *s == '_'); s++)
			/* nada */;
		if (var.length() < 2 || s != var.end())
		    lerror(vars, "bad 'var' declaration: not a variable");
		else {
		    var = var.substring(1);
		    if (!_router->define(var, args[i], false))
			lerror(vars, "parameter '%s' multiply defined", var.c_str());
		}
	    }
    }
}

bool
LexerT::ystatement(bool nested)
{
  const Lexeme &t = lex();
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
      lerror(t, "expected '}'");
    return false;
    
   default:
   syntax_error:
    lerror(t, "syntax error near '%#s'", t.string().c_str());
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
    r->deanonymize_elements();
    // returned router has one reference count
    return r;
}
