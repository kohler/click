// -*- c-basic-offset: 4 -*-
/*
 * lexert.{cc,hh} -- configuration file parser for tools
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
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

static LexerTInfo *stub_lexinfo = 0;

LexerT::LexerT(ErrorHandler *errh, bool ignore_line_directives)
  : _data(0), _len(0), _pos(0), _lineno(1),
    _ignore_line_directives(ignore_line_directives),
    _tpos(0), _tfull(0), _router(0), _compound_class(0),
    _errh(errh)
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
}

void
LexerT::reset(const String &data, const String &filename)
{
  clear();
  
  _big_string = data;
  _data = _big_string.data();
  _len = _big_string.length();

  if (!filename)
    _filename = "line ";
  else if (filename.back() != ':' && !isspace(filename.back()))
    _filename = filename + ":";
  else
    _filename = filename;
  _original_filename = _filename;
  _lineno = 1;
}

void
LexerT::clear()
{
  if (_router)
    delete _router;
  _router = new RouterT;
  _compound_class = 0;
  
  _big_string = "";
  // _data was freed by _big_string
  _data = 0;
  _len = 0;
  _pos = 0;
  _filename = "";
  _lineno = 0;
  _tpos = 0;
  _tfull = 0;
  
  _anonymous_offset = 0;
  _compound_depth = 0;
}

void
LexerT::set_lexinfo(LexerTInfo *li)
{
  _lexinfo = (li ? li : stub_lexinfo);
}

void
LexerT::set_router(RouterT *r)
{
  if (_router)
    delete _router;
  _router = r;
}


// LEXING: LOWEST LEVEL

String
LexerT::remaining_text() const
{
  return _big_string.substring(_pos);
}

void
LexerT::set_remaining_text(const String &s)
{
  _big_string = s;
  _data = s.data();
  _pos = 0;
  _len = s.length();
}

unsigned
LexerT::skip_line(unsigned pos)
{
  _lineno++;
  for (; pos < _len; pos++)
    if (_data[pos] == '\n')
      return pos + 1;
    else if (_data[pos] == '\r') {
      if (pos < _len - 1 && _data[pos+1] == '\n')
	return pos + 2;
      else
	return pos + 1;
    }
  _lineno--;
  return _len;
}

unsigned
LexerT::skip_slash_star(unsigned pos)
{
  for (; pos < _len; pos++)
    if (_data[pos] == '\n')
      _lineno++;
    else if (_data[pos] == '\r') {
      if (pos < _len - 1 && _data[pos+1] == '\n') pos++;
      _lineno++;
    } else if (_data[pos] == '*' && pos < _len - 1 && _data[pos+1] == '/')
      return pos + 2;
  return _len;
}

unsigned
LexerT::skip_backslash_angle(unsigned pos)
{
  for (; pos < _len; pos++)
    if (_data[pos] == '\n')
      _lineno++;
    else if (_data[pos] == '\r') {
      if (pos < _len - 1 && _data[pos+1] == '\n') pos++;
      _lineno++;
    } else if (_data[pos] == '/' && pos < _len - 1) {
      if (_data[pos+1] == '/')
	pos = skip_line(pos + 2) - 1;
      else if (_data[pos+1] == '*')
	pos = skip_slash_star(pos + 2) - 1;
    } else if (_data[pos] == '>')
      return pos + 1;
  return _len;
}

unsigned
LexerT::skip_quote(unsigned pos, char endc)
{
  for (; pos < _len; pos++)
    if (_data[pos] == '\n')
      _lineno++;
    else if (_data[pos] == '\r') {
      if (pos < _len - 1 && _data[pos+1] == '\n') pos++;
      _lineno++;
    } else if (_data[pos] == '\\' && endc == '\"' && pos < _len - 1) {
      if (_data[pos+1] == '<')
	pos = skip_backslash_angle(pos + 2) - 1;
      else if (_data[pos+1] == '\"')
	pos++;
    } else if (_data[pos] == endc)
      return pos + 1;
  return _len;
}

unsigned
LexerT::process_line_directive(unsigned pos)
{
  const char *data = _data;
  unsigned len = _len;
  unsigned first_pos = pos;
  
  for (pos++; pos < len && (data[pos] == ' ' || data[pos] == '\t'); pos++)
    /* nada */;
  if (pos < len - 4 && data[pos] == 'l' && data[pos+1] == 'i'
      && data[pos+2] == 'n' && data[pos+3] == 'e'
      && (data[pos+4] == ' ' || data[pos+4] == '\t')) {
    for (pos += 5; pos < len && (data[pos] == ' ' || data[pos] == '\t'); pos++)
      /* nada */;
  }
  if (pos >= len || !isdigit(data[pos])) {
    // complain about bad directive
    lerror(first_pos, pos, "unknown preprocessor directive");
    return skip_line(pos);
  } else if (_ignore_line_directives)
    return skip_line(pos);
  
  // parse line number
  for (_lineno = 0; pos < len && isdigit(data[pos]); pos++)
    _lineno = _lineno * 10 + data[pos] - '0';
  _lineno--;			// account for extra line
  
  for (; pos < len && (data[pos] == ' ' || data[pos] == '\t'); pos++)
    /* nada */;
  if (pos < len && data[pos] == '\"') {
    // parse filename
    unsigned first_in_filename = pos;
    for (pos++; pos < len && data[pos] != '\"' && data[pos] != '\n' && data[pos] != '\r'; pos++)
      if (data[pos] == '\\' && pos < len - 1 && data[pos+1] != '\n' && data[pos+1] != '\r')
	pos++;
    _filename = cp_unquote(_big_string.substring(first_in_filename, pos - first_in_filename) + "\":");
    // an empty filename means return to the input file's name
    if (_filename == ":")
      _filename = _original_filename;
  }

  // reach end of line
  for (; pos < len && data[pos] != '\n' && data[pos] != '\r'; pos++)
    /* nada */;
  if (pos < len - 1 && data[pos] == '\r' && data[pos+1] == '\n')
    pos++;
  return pos;
}

Lexeme
LexerT::next_lexeme()
{
  unsigned pos = _pos;
  while (true) {
    while (pos < _len && isspace(_data[pos])) {
      if (_data[pos] == '\n')
	_lineno++;
      else if (_data[pos] == '\r') {
	if (pos < _len - 1 && _data[pos+1] == '\n') pos++;
	_lineno++;
      }
      pos++;
    }
    unsigned opos = pos;
    if (pos >= _len) {
      _pos = _len;
      return Lexeme();
    } else if (_data[pos] == '/' && pos < _len - 1) {
      if (_data[pos+1] == '/')
	pos = skip_line(pos + 2);
      else if (_data[pos+1] == '*')
	pos = skip_slash_star(pos + 2);
      else
	break;
      _lexinfo->notify_comment(opos, pos);
    } else if (_data[pos] == '#' && (pos == 0 || _data[pos-1] == '\n' || _data[pos-1] == '\r')) {
      pos = process_line_directive(pos);
      _lexinfo->notify_line_directive(opos, pos);
    } else
      break;
  }
  
  unsigned word_pos = pos;
  
  // find length of current word
  if (isalnum(_data[pos]) || _data[pos] == '_' || _data[pos] == '@') {
    pos++;
    while (pos < _len && (isalnum(_data[pos]) || _data[pos] == '_'
			  || _data[pos] == '/' || _data[pos] == '@')) {
      if (_data[pos] == '/' && pos < _len - 1
	  && (_data[pos+1] == '/' || _data[pos+1] == '*'))
	break;
      pos++;
    }
    _pos = pos;
    String word = _big_string.substring(word_pos, pos - word_pos);
    if (word.length() == 16 && word == "connectiontunnel") {
      _lexinfo->notify_keyword(word, word_pos, pos);
      return Lexeme(lexTunnel, word, word_pos);
    } else if (word.length() == 12 && word == "elementclass") {
      _lexinfo->notify_keyword(word, word_pos, pos);
      return Lexeme(lexElementclass, word, word_pos);
    } else if (word.length() == 7 && word == "require") {
      _lexinfo->notify_keyword(word, word_pos, pos);
      return Lexeme(lexRequire, word, word_pos);
    } else
      return Lexeme(lexIdent, word, word_pos);
  }

  // check for variable
  if (_data[pos] == '$') {
    pos++;
    while (pos < _len && (isalnum(_data[pos]) || _data[pos] == '_'))
      pos++;
    if (pos > word_pos + 1) {
      _pos = pos;
      return Lexeme(lexVariable, _big_string.substring(word_pos, pos - word_pos), word_pos);
    } else
      pos--;
  }

  if (pos < _len - 1) {
    if (_data[pos] == '-' && _data[pos+1] == '>') {
      _pos = pos + 2;
      return Lexeme(lexArrow, _big_string.substring(word_pos, 2), word_pos);
    } else if (_data[pos] == ':' && _data[pos+1] == ':') {
      _pos = pos + 2;
      return Lexeme(lex2Colon, _big_string.substring(word_pos, 2), word_pos);
    } else if (_data[pos] == '|' && _data[pos+1] == '|') {
      _pos = pos + 2;
      return Lexeme(lex2Bar, _big_string.substring(word_pos, 2), word_pos);
    }
  }
  if (pos < _len - 2 && _data[pos] == '.' && _data[pos+1] == '.' && _data[pos+2] == '.') {
    _pos = pos + 3;
    return Lexeme(lex3Dot, _big_string.substring(word_pos, 3), word_pos);
  }
  
  _pos = pos + 1;
  return Lexeme(_data[word_pos], _big_string.substring(word_pos, 1), word_pos);
}

Lexeme
LexerT::lex_config()
{
  unsigned config_pos = _pos;
  unsigned pos = _pos;
  unsigned paren_depth = 1;
  
  for (; pos < _len; pos++)
    if (_data[pos] == '(')
      paren_depth++;
    else if (_data[pos] == ')') {
      paren_depth--;
      if (!paren_depth) break;
    } else if (_data[pos] == '\n')
      _lineno++;
    else if (_data[pos] == '\r') {
      if (pos < _len - 1 && _data[pos+1] == '\n') pos++;
      _lineno++;
    } else if (_data[pos] == '/' && pos < _len - 1) {
      if (_data[pos+1] == '/')
	pos = skip_line(pos + 2) - 1;
      else if (_data[pos+1] == '*')
	pos = skip_slash_star(pos + 2) - 1;
    } else if (_data[pos] == '\'' || _data[pos] == '\"')
      pos = skip_quote(pos + 1, _data[pos]) - 1;
    else if (_data[pos] == '\\' && pos < _len - 1 && _data[pos+1] == '<')
      pos = skip_backslash_angle(pos + 2) - 1;
  
  _pos = pos;
  _lexinfo->notify_config_string(config_pos, pos);
  return Lexeme(lexConfig, _big_string.substring(config_pos, pos - config_pos),
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
    return "`->'";
  else if (kind == lex2Colon)
    return "`::'";
  else if (kind == lex2Bar)
    return "`||'";
  else if (kind == lex3Dot)
    return "`...'";
  else if (kind == lexTunnel)
    return "`connectiontunnel'";
  else if (kind == lexElementclass)
    return "`elementclass'";
  else if (kind == lexRequire)
    return "`require'";
  else if (kind >= 32 && kind < 127) {
    sprintf(buf, "`%c'", kind);
    return buf;
  } else {
    sprintf(buf, "`\\%03d'", kind);
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
  _tcircle[_tfull] = t;
  _tfull = (_tfull + 1) % TCIRCLE_SIZE;
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
	    lerror(_tcircle[_tpos], "expected %s", lexeme_string(kind).cc());
    } else {
	unsigned old_pos = _pos;
	if (next_lexeme().is(kind))
	    return true;
	if (report_error)
	    lerror(old_pos, _pos, "expected %s", lexeme_string(kind).cc());
	_pos = old_pos;
    }
    return false;
}

int
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
    return _filename + String(_lineno);
}

void
LexerT::vlerror(int pos1, int pos2, const String &lm, const char *format, va_list val)
{
    String text = _errh->make_text(ErrorHandler::ERR_ERROR, format, val);
    _lexinfo->notify_error(text, pos1, pos2);
    text = _errh->decorate_text(ErrorHandler::ERR_ERROR, String(), lm, text);
    _errh->handle_text(ErrorHandler::ERR_ERROR, text);
}

int
LexerT::lerror(int pos1, int pos2, const char *format, ...)
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
    ElementClassT *type = _router->try_type(t.string());
    if (type)
	_lexinfo->notify_class_reference(type, t.pos1(), t.pos2());
    return type;
}

ElementClassT *
LexerT::force_element_type(const Lexeme &t)
{
    assert(t.is(lexIdent));
    String name = t.string();
    if (_router->eindex(name) >= 0 && !_router->try_type(name))
	lerror(t, "`%s' was previously used as an element name", name.cc());
    ElementClassT *type = _router->get_type(name);
    _lexinfo->notify_class_reference(type, t.pos1(), t.pos2());
    return type;
}


// ELEMENTS

String
LexerT::anon_element_name(const String &class_name) const
{
    String prefix = class_name + "@";
    int anonymizer = _router->nelements() - _anonymous_offset + 1;
    String name = prefix + String(anonymizer);
    while (_router->eindex(name) >= 0) {
	anonymizer++;
	name = prefix + String(anonymizer);
    }
    return name;
}

String
LexerT::anon_element_class_name(String prefix) const
{
    int anonymizer = _router->nelements() - _anonymous_offset + 1;
    String name = prefix + String(anonymizer);
    while (_router->try_type(name)) {
	anonymizer++;
	name = prefix + String(anonymizer);
    }
    return name;
}

int
LexerT::make_element(String name, const Lexeme &location, int decl_pos2,
		     ElementClassT *type, const String &conf, const String &lm)
{
    // check `name' for validity
    for (int i = 0; i < name.length(); i++) {
	bool ok = false;
	for (; i < name.length() && name[i] != '/'; i++)
	    if (!isdigit(name[i]))
		ok = true;
	if (!ok) {
	    lerror(location, "element name `%s' has all-digit component", name.cc());
	    break;
	}
    }
    _lexinfo->notify_element_declaration(name, type, _compound_class, location.pos1(), location.pos2(), (decl_pos2 < 0 ? location.pos2() : decl_pos2));
    return _router->get_eindex(name, type, conf, lm ? lm : landmark());
}

int
LexerT::make_anon_element(const Lexeme &what, int decl_pos2,
			  ElementClassT *type, const String &conf,
			  const String &lm)
{
    return make_element(anon_element_name(type->name()), what, decl_pos2, type, conf, lm);
}

void
LexerT::connect(int element1, int port1, int port2, int element2)
{
    if (port1 < 0)
	port1 = 0;
    if (port2 < 0)
	port2 = 0;
    _router->add_connection
	(Hookup(_router->elt(element1), port1),
	 Hookup(_router->elt(element2), port2), landmark());
}


// PARSING

bool
LexerT::yport(int &port, int &pos1, int &pos2)
{
    const Lexeme &tlbrack = lex();
    if (!tlbrack.is('[')) {
	unlex(tlbrack);
	return false;
    }
    pos1 = tlbrack.pos1();

    const Lexeme &tword = lex();
    if (tword.is(lexIdent)) {
	String p = tword.string();
	const char *ps = p.cc();
	if (isdigit(ps[0]) || ps[0] == '-')
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
    int decl_pos2 = -1;

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
    String lm;
    const Lexeme &tparen = lex();
    if (tparen.is('(')) {
	lm = landmark();	// report landmark from before config string
	if (!etype)
	    etype = force_element_type(tname);
	configuration = lex_config();
	expect(')');
	decl_pos2 = next_pos();
    } else
	unlex(tparen);

    if (etype)
	element = make_anon_element(tname, decl_pos2, etype, configuration.string(), lm);
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
		element = make_anon_element(tname, tname.pos2(), etype, configuration.string(), lm);
	    } else
		_lexinfo->notify_element_reference(name, _router->etype(element), _compound_class, tname.pos1(), tname.pos2());
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
	    lerror(tsep, "syntax error: expected `::' or `,'");
	    unlex(tsep);
	    return;
	}
    }

    String lm = landmark();
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

    int decl_pos2 = (decls.size() == 1 ? next_pos() : -1);

    for (int i = 0; i < decls.size(); i++) {
	String name = decls[i].string();
	int old_eidx = _router->eindex(name);
	if (old_eidx >= 0) {
	    lerror(decls[i], "redeclaration of element `%s'", name.cc());
	    _errh->lerror(_router->elandmark(old_eidx), "`%s' previously declared here", _router->edeclaration(old_eidx).cc());
	} else if (_router->try_type(name))
	    lerror(decls[i], "`%s' is an element class", name.cc());
	else
	    make_element(name, decls[i], decl_pos2, etype, configuration.string(), lm);
    }
}

bool
LexerT::yconnection()
{
    int element1 = -1;
    int port1 = -1, port1_pos1 = -1, port1_pos2 = -1;
    Lexeme t;

    while (true) {
	int element2;
	int port2 = -1, port2_pos1, port2_pos2;

	// get element
	yport(port2, port2_pos1, port2_pos2);
	if (!yelement(element2, element1 < 0)) {
	    if (port1 >= 0)
		lerror(port1_pos1, port1_pos2, "output port useless at end of chain");
	    return element1 >= 0;
	}

	if (element1 >= 0)
	    connect(element1, port1, port2, element2);
	else if (port2 >= 0)
	    lerror(port2_pos1, port2_pos2, "input port useless at start of chain");
    
	port1 = -1;
    
      relex:
	t = lex();
	switch (t.kind()) {
      
	  case ',':
	  case lex2Colon:
	    lerror(t, "syntax error before `%s'", t.string().cc());
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
	  case lexTunnel:
	  case lexElementclass:
	  case lexRequire:
	    unlex(t);
	    // FALLTHRU
	  case ';':
	  case lexEOF:
	    if (port1 >= 0)
		lerror(port1_pos1, port1_pos2, "output port useless at end of chain", port1);
	    return true;
      
	  default:
	    lerror(t, "syntax error near `%#s'", String(t.string()).cc());
	    if (t.kind() >= lexIdent)	// save meaningful tokens
		unlex(t);
	    return true;
      
	}
    
	// have `x ->'
	element1 = element2;
    }
}

void
LexerT::yelementclass(int pos1)
{
    Lexeme tname = lex();
    String eclass_name;
    if (!tname.is(lexIdent)) {
	unlex(tname);
	lerror(tname, "expected element type name");
    } else {
	String n = tname.string();
	if (_router->eindex(n) >= 0)
	    lerror(tname, "`%s' already used as an element name", n.cc());
	else
	    eclass_name = n;
    }

    Lexeme tnext = lex();
    if (tnext.is('{'))
	(void) ycompound(eclass_name, pos1, tname.pos1());

    else if (tnext.is(lexIdent)) {
	ElementClassT *ec = _router->get_type(tnext.string());
	if (eclass_name) {
	    ElementClassT *new_ec = new SynonymElementClassT(eclass_name, ec);
	    _router->get_type(new_ec, true);
	    _lexinfo->notify_class_declaration(new_ec, false, pos1, tname.pos1(), tnext.pos2());
	}
	
    } else
	lerror(tnext, "syntax error near `%#s'", String(tnext.string()).cc());
}

void
LexerT::ytunnel()
{
    Lexeme tname1 = lex();
    if (!tname1.is(lexIdent)) {
	unlex(tname1);
	lerror(tname1, "expected tunnel input name");
    }
    
    expect(lexArrow);
  
    Lexeme tname2 = lex();
    if (!tname2.is(lexIdent)) {
	unlex(tname2);
	lerror(tname2, "expected tunnel output name");
    }
  
    if (tname1.is(lexIdent) && tname2.is(lexIdent))
	_router->add_tunnel(tname1.string(), tname2.string(), landmark(), _errh);
}

void
LexerT::ycompound_arguments(CompoundElementClassT *comptype)
{
  while (1) {
    const Lexeme &tvar = lex();
    if (!tvar.is(lexVariable)) {
      if (!tvar.is('|') || comptype->nformals() > 0)
	unlex(tvar);
      return;
    }
    comptype->add_formal(tvar.string());
    const Lexeme &tsep = lex();
    if (tsep.is('|'))
      return;
    else if (!tsep.is(',')) {
      lerror(tsep, "expected `,' or `|'");
      unlex(tsep);
      return;
    }
  }
}

ElementClassT *
LexerT::ycompound(String name, int decl_pos1, int name_pos1)
{
    bool anonymous = (name.length() == 0);
    if (anonymous)
	name = anon_element_class_name("@Class");

    // '{' was already read
    RouterT *old_router = _router;
    CompoundElementClassT *old_compound_class = _compound_class;
    int old_offset = _anonymous_offset;
    _compound_depth++;
    
    ElementClassT *created = 0;

    // check for '...'
    const Lexeme &t = lex();
    if (t.is(lex3Dot)) {
	if (anonymous) {
	    lerror(t, "cannot extend anonymous compound element class");
	    created = _router->get_type("Error");
	} else {
	    created = _router->get_type(name);
	    _lexinfo->notify_class_extension(created, t.pos1(), t.pos2());
	}
	expect(lex2Bar);
    } else
	unlex(t);

    ElementClassT *first = created;
    int pos2;
    
    while (1) {
	_router = new RouterT(old_router);
	_router->get_eindex("input", ElementClassT::tunnel_type(), String(), landmark());
	_router->get_eindex("output", ElementClassT::tunnel_type(), String(), landmark());
	_compound_class = new CompoundElementClassT(name, landmark(), _router, created, _compound_depth);
	_anonymous_offset = 2;

	ycompound_arguments(_compound_class);
	while (ystatement(true))
	    /* nada */;

	_compound_class->finish(_errh);
	created = _compound_class;

	// check for '||' or '}'
	const Lexeme &t = lex();
	if (!t.is(lex2Bar)) {
	    pos2 = t.pos2();
	    break;
	}
    }

    _compound_depth--;
    _anonymous_offset = old_offset;
    _router = old_router;
    _compound_class = old_compound_class;
    
    created->cast_compound()->check_duplicates_until(first, _errh);
    _lexinfo->notify_class_declaration(created, anonymous, decl_pos1, name_pos1, pos2);

    return old_router->get_type(created, !anonymous);
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
    
   case lexTunnel:
    ytunnel();
    return true;

   case lexRequire:
    yrequire();
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
      lerror(t, "expected `}'");
    return false;
    
   default:
   syntax_error:
    lerror(t, "syntax error near `%#s'", String(t.string()).cc());
    return true;
    
  }
}


// COMPLETION

RouterT *
LexerT::take_router()
{
  RouterT *r = _router;
  _router = 0;
  return r;
}

#include <click/vector.cc>
