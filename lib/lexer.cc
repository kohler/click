// -*- c-basic-offset: 2; related-file-name: "../include/click/lexer.hh" -*-
/*
 * lexer.{cc,hh} -- parses Click language files, produces Router objects
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
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
#include <click/lexer.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/variableenv.hh>
#include <click/standard/errorelement.hh>
CLICK_DECLS

static void
redeclaration_error(ErrorHandler *errh, const char *what, String name, const String &landmark, const String &old_landmark)
{
  if (!what)
    what = "";
  const char *sp = (strlen(what) ? " " : "");
  errh->lerror(landmark, "redeclaration of %s%s'%s'", what, sp, name.cc());
  errh->lerror(old_landmark, "'%s' previously declared here", name.cc());
}

//
// ELEMENT FACTORIES
//

static Element *
error_element_factory(uintptr_t)
{
  return new ErrorElement;
}

static Element *
compound_element_factory(uintptr_t)
{
  assert(0);
  return 0;
}

//
// CLASS LEXER::TUNNELEND
//

class Lexer::TunnelEnd {
  
  Router::Hookup _port;
  Vector<Router::Hookup> _correspond;
  int _expanded;
  bool _output;
  TunnelEnd *_other;
  TunnelEnd *_next;
  
 public:
  
  TunnelEnd(const Router::Hookup &, bool, TunnelEnd *);
  ~TunnelEnd()				{ delete _next; }
  
  const Router::Hookup &port() const	{ return _port; }
  bool output() const			{ return _output; }
  TunnelEnd *next() const		{ return _next; }
  void pair_with(TunnelEnd *d)		{ _other = d; d->_other = this; }
  
  TunnelEnd *find(const Router::Hookup &);
  void expand(const Lexer *, Vector<Router::Hookup> &);
  
};

//
// CLASS LEXER::COMPOUND
//

class Lexer::Compound : public Element { public:
  
  Compound(const String &, const String &, int);

  const String &name() const		{ return _name; }
  const char *printable_name_cc();
  const String &landmark() const	{ return _landmark; }
  int nformals() const			{ return _formals.size(); }
  const Vector<String> &formals() const	{ return _formals; }
  const Vector<String> &formal_types() const { return _formal_types; }
  inline void add_formal(const String &fn, const String &ft);
  int depth() const			{ return _depth; }

  void swap_router(Lexer *);
  void finish(Lexer *, ErrorHandler *);

  int resolve(Lexer *, int etype, int ninputs, int noutputs, Vector<String> &, ErrorHandler *, const String &landmark);
  void expand_into(Lexer *, int, const VariableEnvironment &);
  
  const char *class_name() const	{ return _name.cc(); }
  void *cast(const char *);
  Compound *clone() const		{ return 0; }

  void set_overload_type(int t)		{ _overload_type = t; }
  inline Compound *overload_compound(Lexer *) const;

  String signature() const;
  static String signature(const String &name, const Vector<String> *formal_types, int nargs, int ninputs, int noutputs);

 private:
  
  mutable String _name;
  String _landmark;
  int _depth;
  int _overload_type;

  Vector<String> _formals;
  Vector<String> _formal_types;
  int _ninputs;
  int _noutputs;
  
  Vector<int> _elements;
  Vector<String> _element_names;
  Vector<String> _element_configurations;
  Vector<String> _element_landmarks;
  
  Vector<Hookup> _hookup_from;
  Vector<Hookup> _hookup_to;
  
};

Lexer::Compound::Compound(const String &name, const String &lm, int depth)
  : _name(name), _landmark(lm), _depth(depth), _overload_type(-1),
    _ninputs(0), _noutputs(0)
{
}

const char *
Lexer::Compound::printable_name_cc()
{
  if (_name)
    return _name.cc();
  else
    return "<anonymous>";
}

void *
Lexer::Compound::cast(const char *s)
{
  if (strcmp(s, "Lexer::Compound") == 0 || _name == s)
    return this;
  else
    return 0;
}

void
Lexer::Compound::swap_router(Lexer *lexer)
{
  lexer->_elements.swap(_elements);
  lexer->_element_names.swap(_element_names);
  lexer->_element_configurations.swap(_element_configurations);
  lexer->_element_landmarks.swap(_element_landmarks);

  lexer->_hookup_from.swap(_hookup_from);
  lexer->_hookup_to.swap(_hookup_to);
}

inline void
Lexer::Compound::add_formal(const String &fname, const String &ftype)
{
  _formals.push_back(fname);
  _formal_types.push_back(ftype);
}

void
Lexer::Compound::finish(Lexer *lexer, ErrorHandler *errh)
{
  assert(_element_names[0] == "input" && _element_names[1] == "output");

  // count numbers of inputs and outputs
  Vector<int> from_in, to_out;
  bool to_in = false, from_out = false;
  for (int i = 0; i < _hookup_from.size(); i++) {
    const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    
    if (hf.idx == 0) {
      if (from_in.size() <= hf.port)
	from_in.resize(hf.port + 1, 0);
      from_in[hf.port] = 1;
    } else if (hf.idx == 1)
      from_out = true;
    
    if (ht.idx == 1) {
      if (to_out.size() <= ht.port)
	to_out.resize(ht.port + 1, 0);
      to_out[ht.port] = 1;
    } else if (ht.idx == 0)
      to_in = true;
  }
  
  // store information
  _ninputs = from_in.size();
  if (to_in)
    errh->lerror(_landmark, "'%s' pseudoelement 'input' may only be used as output", printable_name_cc());
  for (int i = 0; i < from_in.size(); i++)
    if (!from_in[i])
      errh->lerror(_landmark, "compound element '%s' input %d unused", printable_name_cc(), i);
  
  _noutputs = to_out.size();
  if (from_out)
    errh->lerror(_landmark, "'%s' pseudoelement 'output' may only be used as input", printable_name_cc());
  for (int i = 0; i < to_out.size(); i++)
    if (!to_out[i])
      errh->lerror(_landmark, "compound element '%s' output %d unused", printable_name_cc(), i);

  // deanonymize element names
  for (int i = 0; i < _elements.size(); i++)
    if (_element_names[i][0] == ';')
      _element_names[i] = lexer->deanonymize_element_name(_element_names[i], i);
}

inline Lexer::Compound *
Lexer::Compound::overload_compound(Lexer *lexer) const
{
  if (_overload_type >= 0 && lexer->_element_types[_overload_type].factory == compound_element_factory)
    return (Compound *) lexer->_element_types[_overload_type].thunk;
  else
    return 0;
}

int
Lexer::Compound::resolve(Lexer *lexer, int etype, int ninputs, int noutputs, Vector<String> &args, ErrorHandler *errh, const String &landmark)
{
  // Try to return an element class, even if it is wrong -- the error messages
  // are friendlier
  Compound *ct = this;
  int closest_etype = -1;
  
  while (ct) {
    if (ct->_ninputs == ninputs && ct->_noutputs == noutputs
	&& cp_assign_arguments(args, ct->_formal_types, &args) >= 0)
      return etype;
    else if (cp_assign_arguments(args, ct->_formal_types) >= 0)
      closest_etype = etype;

    if (Compound *next = ct->overload_compound(lexer)) {
      etype = ct->_overload_type;
      ct = next;
    } else if (ct->_overload_type >= 0)
      return ct->_overload_type;
    else
      break;
  }

  errh->lerror(landmark, "no match for '%s'", signature(name(), 0, args.size(), ninputs, noutputs).c_str());
  ContextErrorHandler cerrh(errh, "candidates are:", "  ");
  for (ct = this; ct; ct = ct->overload_compound(lexer))
    cerrh.lmessage(ct->landmark(), "%s", ct->signature().c_str());
  ct = (closest_etype >= 0 ? (Compound *) lexer->_element_types[closest_etype].thunk : 0);
  if (ct)
    cp_assign_arguments(args, ct->_formal_types, &args);
  return closest_etype;
}

String
Lexer::Compound::signature(const String &name, const Vector<String> *formal_types, int nargs, int ninputs, int noutputs)
{
  StringAccum sa;
  sa << (name ? name : String("<anonymous>"));
  
  if (formal_types && formal_types->size()) {
    sa << '(';
    for (int i = 0; i < formal_types->size(); i++) {
      if (i)
	sa << ", ";
      if ((*formal_types)[i] == "")
	sa << "<arg>";
      else if ((*formal_types)[i] == "__REST__")
	sa << "...";
      else
	sa << (*formal_types)[i];
    }
    sa << ')';
  }

  const char *pl_args = (nargs == 1 ? " argument, " : " arguments, ");
  const char *pl_ins = (ninputs == 1 ? " input, " : " inputs, ");
  const char *pl_outs = (noutputs == 1 ? " output" : " outputs");
  sa << '[';
  if (!formal_types && nargs > 0)
    sa << nargs << pl_args;
  sa << ninputs << pl_ins << noutputs << pl_outs;
  sa << ']';
  
  return sa.take_string();
}

String
Lexer::Compound::signature() const
{
  return signature(_name, &_formal_types, -1, _ninputs, _noutputs);
}

void
Lexer::Compound::expand_into(Lexer *lexer, int which, const VariableEnvironment &ve)
{
  ErrorHandler *errh = lexer->_errh;
  
  // 'name_slash' is 'name' constrained to end with a slash
  String ename = lexer->_element_names[which];
  String ename_slash = ename + "/";

  assert(_element_names[0] == "input" && _element_names[1] == "output");

  lexer->_elements[which] = TUNNEL_TYPE;
  lexer->add_tunnel(ename, ename_slash + "input");
  lexer->add_tunnel(ename_slash + "output", ename);

  Vector<int> eidx_map;
  eidx_map.push_back(lexer->_element_map[ename_slash + "input"]);
  eidx_map.push_back(lexer->_element_map[ename_slash + "output"]);

  for (int i = 2; i < _elements.size(); i++) {
    String cname = ename_slash + _element_names[i];
    int eidx = lexer->_element_map[cname];
    if (eidx >= 0) {
      redeclaration_error(errh, "element", cname, lexer->element_landmark(which), lexer->element_landmark(eidx));
      eidx_map.push_back(-1);
    } else {
      if (lexer->_element_type_map[cname] >= 0)
	errh->lerror(lexer->element_landmark(which), "'%s' is an element class", cname.cc());
      eidx = lexer->get_element(cname, _elements[i], ve.interpolate(_element_configurations[i]), _element_landmarks[i]);
      eidx_map.push_back(eidx);
    }
  }

  // now copy hookups
  int nh = _hookup_from.size();
  for (int i = 0; i < nh; i++) {
    const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    if (eidx_map[hf.idx] >= 0 && eidx_map[ht.idx] >= 0)
      lexer->connect(eidx_map[hf.idx], hf.port, eidx_map[ht.idx], ht.port);
  }

  // now expand those
  for (int i = 2; i < eidx_map.size(); i++)
    if (eidx_map[i] >= 0)
      lexer->expand_compound_element(eidx_map[i], ve);
}

//
// LEXER
//

Lexer::Lexer()
  : _data(0), _len(0), _pos(0), _lineno(1), _lextra(0),
    _tpos(0), _tfull(0),
    _element_type_map(-1),
    _last_element_type(ET_NULL),
    _free_element_type(-1),
    _element_map(-1),
    _definputs(0), _defoutputs(0),
    _errh(ErrorHandler::default_handler())
{
  end_parse(ET_NULL);		// clear private state
  add_element_type("<tunnel>", error_element_factory, 0);
  add_element_type("Error", error_element_factory, 0);
  assert(element_type("<tunnel>") == TUNNEL_TYPE && element_type("Error") == ERROR_TYPE);
}

Lexer::~Lexer()
{
  end_parse(ET_NULL);

  // get rid of nonscoped element types
  for (int t = 0; t < _element_types.size(); t++)
    if (_element_types[t].factory == compound_element_factory) {
      Lexer::Compound *compound = (Lexer::Compound *) _element_types[t].thunk;
      delete compound;
    }
}

int
Lexer::begin_parse(const String &data, const String &filename,
		   LexerExtra *lextra, ErrorHandler *errh)
{
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

  _lextra = lextra;
  _errh = (errh ? errh : ErrorHandler::default_handler());
  
  return lexical_scoping_in();
}

void
Lexer::end_parse(int cookie)
{
  lexical_scoping_out(cookie);
  
  delete _definputs;
  _definputs = 0;
  delete _defoutputs;
  _defoutputs = 0;
  
  _elements.clear();
  _element_names.clear();
  _element_configurations.clear();
  _element_landmarks.clear();
  _element_map.clear();
  _hookup_from.clear();
  _hookup_to.clear();
  _requirements.clear();
  
  _big_string = "";
  // _data was freed by _big_string
  _data = 0;
  _len = 0;
  _pos = 0;
  _filename = "";
  _lineno = 0;
  _lextra = 0;
  _tpos = 0;
  _tfull = 0;
  
  _anonymous_offset = 0;
  _compound_depth = 0;

  _errh = ErrorHandler::default_handler();
}


// LEXING: LOWEST LEVEL

String
Lexer::remaining_text() const
{
  return _big_string.substring(_pos);
}

void
Lexer::set_remaining_text(const String &s)
{
  _big_string = s;
  _data = s.data();
  _pos = 0;
  _len = s.length();
}

unsigned
Lexer::skip_line(unsigned pos)
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
Lexer::skip_slash_star(unsigned pos)
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
Lexer::skip_backslash_angle(unsigned pos)
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
Lexer::skip_quote(unsigned pos, char endc)
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
Lexer::process_line_directive(unsigned pos)
{
  const char *data = _data;
  unsigned len = _len;
  
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
    lerror("unknown preprocessor directive");
    return skip_line(pos);
  }
  
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
Lexer::next_lexeme()
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
    } else if (_data[pos] == '#' && (pos == 0 || _data[pos-1] == '\n' || _data[pos-1] == '\r'))
      pos = process_line_directive(pos);
    else
      break;
  }
  
  unsigned word_pos = pos;
  
  // find length of current word
  if (isalnum(_data[pos]) || _data[pos] == '_' || _data[pos] == '@') {
   more_word_characters:
    pos++;
    while (pos < _len && (isalnum(_data[pos]) || _data[pos] == '_' || _data[pos] == '@'))
      pos++;
    if (pos < _len - 1 && _data[pos] == '/' && (isalnum(_data[pos+1]) || _data[pos+1] == '_' || _data[pos+1] == '@'))
      goto more_word_characters;
    _pos = pos;
    String word = _big_string.substring(word_pos, pos - word_pos);
    if (word.length() == 16 && word == "connectiontunnel")
      return Lexeme(lexTunnel, word);
    else if (word.length() == 12 && word == "elementclass")
      return Lexeme(lexElementclass, word);
    else if (word.length() == 7 && word == "require")
      return Lexeme(lexRequire, word);
    else
      return Lexeme(lexIdent, word);
  }

  // check for variable
  if (_data[pos] == '$') {
    pos++;
    while (pos < _len && (isalnum(_data[pos]) || _data[pos] == '_'))
      pos++;
    if (pos > word_pos + 1) {
      _pos = pos;
      return Lexeme(lexVariable, _big_string.substring(word_pos, pos - word_pos));
    } else
      pos--;
  }
  
  if (pos < _len - 1) {
    if (_data[pos] == '-' && _data[pos+1] == '>') {
      _pos = pos + 2;
      return Lexeme(lexArrow, _big_string.substring(word_pos, 2));
    } else if (_data[pos] == ':' && _data[pos+1] == ':') {
      _pos = pos + 2;
      return Lexeme(lex2Colon, _big_string.substring(word_pos, 2));
    } else if (_data[pos] == '|' && _data[pos+1] == '|') {
      _pos = pos + 2;
      return Lexeme(lex2Bar, _big_string.substring(word_pos, 2));
    }
  }
  if (pos < _len - 2 && _data[pos] == '.' && _data[pos+1] == '.' && _data[pos+2] == '.') {
    _pos = pos + 3;
    return Lexeme(lex3Dot, _big_string.substring(word_pos, 3));
  }
  
  _pos = pos + 1;
  return Lexeme(_data[word_pos], _big_string.substring(word_pos, 1));
}

String
Lexer::lex_config()
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
  return _big_string.substring(config_pos, pos - config_pos);
}

String
Lexer::lexeme_string(int kind)
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
  else if (kind == lexTunnel)
    return "'connectiontunnel'";
  else if (kind == lexElementclass)
    return "'elementclass'";
  else if (kind == lexRequire)
    return "'require'";
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
Lexer::lex()
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
Lexer::unlex(const Lexeme &t)
{
  _tpos = (_tpos + TCIRCLE_SIZE - 1) % TCIRCLE_SIZE;
  _tcircle[_tpos] = t;
  assert(_tfull != _tpos);
}

bool
Lexer::expect(int kind, bool report_error)
{
  // Never adds anything to '_tcircle'. This requires a nonobvious
  // implementation.
  if (_tpos != _tfull) {
    if (_tcircle[_tpos].is(kind)) {
      _tpos = (_tpos + 1) % TCIRCLE_SIZE;
      return true;
    }
  } else {
    unsigned old_pos = _pos;
    if (next_lexeme().is(kind))
      return true;
    _pos = old_pos;
  }
  if (report_error)
    lerror("expected %s", lexeme_string(kind).cc());
  return false;
}


// ERRORS

String
Lexer::landmark() const
{
  return _filename + String(_lineno);
}

int
Lexer::lerror(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  _errh->verror(ErrorHandler::ERR_ERROR, landmark(), format, val);
  va_end(val);
  return -1;
}


// ELEMENT TYPES

int
Lexer::add_element_type(const String &name, ElementFactory factory, uintptr_t thunk, bool scoped)
{
  assert(factory);	       // 3.Sep.2003: anonymous compounds have name ""
  int tid;
  if (_free_element_type < 0) {
    tid = _element_types.size();
    _element_types.push_back(ElementType());
    _element_types[tid].factory = factory;
    _element_types[tid].thunk = thunk;
    _element_types[tid].name = name;
    _element_types[tid].next = _last_element_type | (scoped ? (int)ET_SCOPED : 0);
  } else {
    tid = _free_element_type;
    _free_element_type = _element_types[tid].next;
    _element_types[tid].factory = factory;
    _element_types[tid].thunk = thunk;
    _element_types[tid].name = name;
    _element_types[tid].next = _last_element_type | (scoped ? (int)ET_SCOPED : 0);
  }
  if (name)
    _element_type_map.insert(name, tid);
  _last_element_type = tid;
  return tid;
}

int
Lexer::element_type(const String &s) const
{
  return _element_type_map[s];
}

int
Lexer::force_element_type(String s)
{
  int ftid = _element_type_map[s];
  if (ftid >= 0)
    return ftid;
  lerror("unknown element class '%s'", s.cc());
  return add_element_type(s, error_element_factory, 0, true);
}

int
Lexer::lexical_scoping_in() const
{
  return _last_element_type;
}

void
Lexer::lexical_scoping_out(int last)
{
  int *prev = &_last_element_type;
  while (*prev != last && *prev != ET_NULL) {
    assert(!(*prev & ET_SCOPED));
    int *next = &_element_types[*prev].next;
    if (*next & ET_SCOPED)
      remove_element_type(*prev, prev);
    else
      prev = next;
  }
}

int
Lexer::remove_element_type(int removed, int *prev_hint)
{
  // exit early if trying to remove bad type
  if (removed < 0 || removed >= _element_types.size() || _element_types[removed].factory == 0)
    return -1;

  // fix _element_type_next chain
  if (!prev_hint || (int)(*prev_hint & ET_TMASK) != removed)
    for (prev_hint = &_last_element_type;
	 (*prev_hint & ET_TMASK) != ET_NULL && (int)(*prev_hint & ET_TMASK) != removed;
	 prev_hint = &_element_types[*prev_hint & ET_TMASK].next)
      /* nada */;
  assert(prev_hint);
  if ((int)(*prev_hint & ET_TMASK) == removed)
    *prev_hint = (*prev_hint & ~ET_TMASK) | (_element_types[removed].next & ET_TMASK);
  
  // fix up element type name map
  const String &name = _element_types[removed].name;
  if (name && _element_type_map[name] == removed) {
    int trav;
    for (trav = _element_types[removed].next & ET_TMASK;
	 trav != ET_NULL && _element_types[trav].name != name;
	 trav = _element_types[trav].next & ET_TMASK)
      /* nada */;
    _element_type_map.insert(name, (trav == ET_NULL ? -1 : trav));
  }

  // remove stuff
  if (_element_types[removed].factory == compound_element_factory) {
    Lexer::Compound *compound = (Lexer::Compound *) _element_types[removed].thunk;
    delete compound;
  }
  _element_types[removed].factory = 0;
  _element_types[removed].name = String();
  _element_types[removed].next = _free_element_type;
  _free_element_type = removed;

  return 0;
}

void
Lexer::element_type_names(Vector<String> &v) const
{
  for (HashMap<String, int>::const_iterator i = _element_type_map.begin(); i; i++)
    if (i.value() >= 0 && i.key() != "<tunnel>")
      v.push_back(i.key());
}


// PORT TUNNELS

void
Lexer::add_tunnel(String namein, String nameout)
{
  Hookup hin(get_element(namein, TUNNEL_TYPE), 0);
  Hookup hout(get_element(nameout, TUNNEL_TYPE), 0);
  
  bool ok = true;
  if (_elements[hin.idx] != TUNNEL_TYPE) {
    redeclaration_error(_errh, "element", namein, landmark(), _element_landmarks[hin.idx]);
    ok = false;
  }
  if (_elements[hout.idx] != TUNNEL_TYPE) {
    redeclaration_error(_errh, "element", nameout, landmark(), _element_landmarks[hout.idx]);
    ok = false;
  }
  if (_definputs && _definputs->find(hin)) {
    redeclaration_error(_errh, "connection tunnel input", namein, landmark(), _element_landmarks[hin.idx]);
    ok = false;
  }
  if (_defoutputs && _defoutputs->find(hout)) {
    redeclaration_error(_errh, "connection tunnel output", nameout, landmark(), _element_landmarks[hout.idx]);
    ok = false;
  }
  if (ok) {
    _definputs = new TunnelEnd(hin, false, _definputs);
    _defoutputs = new TunnelEnd(hout, true, _defoutputs);
    _definputs->pair_with(_defoutputs);
  }
}

// ELEMENTS

int
Lexer::get_element(String name, int etype, const String &conf, const String &lm)
{
  assert(name && etype >= 0 && etype < _element_types.size());
  
  // if an element 'name' already exists return it
  if (_element_map[name] >= 0)
    return _element_map[name];

  int eid = _elements.size();
  _element_map.insert(name, eid);
  
  // check 'name' for validity
  for (int i = 0; i < name.length(); i++) {
    bool ok = false;
    for (; i < name.length() && name[i] != '/'; i++)
      if (!isdigit(name[i]))
	ok = true;
    if (!ok) {
      lerror("element name '%s' has all-digit component", name.cc());
      break;
    }
  }
  
  _element_names.push_back(name);
  _element_configurations.push_back(conf);
  _element_landmarks.push_back(lm ? lm : landmark());
  _elements.push_back(etype);
  return eid;
}

String
Lexer::anon_element_name(const String &class_name) const
{
  int anonymizer = _elements.size() - _anonymous_offset + 1;
  return ";" + class_name + "@" + String(anonymizer);
}

String
Lexer::deanonymize_element_name(const String &ename, int eidx)
{
  // This function uses _element_map.
  assert(ename && ename[0] == ';');
  String name = ename.substring(1);
  if (_element_map[name] >= 0) {
    int at_pos = name.find_right('@');
    assert(at_pos >= 0);
    String prefix = name.substring(0, at_pos + 1);
    int anonymizer;
    cp_integer(name.substring(at_pos + 1), &anonymizer);
    do {
      anonymizer++;
      name = prefix + String(anonymizer);
    } while (_element_map[name] >= 0);
  }
  _element_map.insert(name, eidx);
  return name;
}

void
Lexer::connect(int element1, int port1, int element2, int port2)
{
  if (port1 < 0) port1 = 0;
  if (port2 < 0) port2 = 0;
  _hookup_from.push_back(Router::Hookup(element1, port1));
  _hookup_to.push_back(Router::Hookup(element2, port2));
}

String
Lexer::element_name(int eid) const
{
  if (eid < 0 || eid >= _elements.size())
    return "##no-such-element##";
  else if (_element_names[eid])
    return _element_names[eid];
  else {
    char buf[100];
    sprintf(buf, "@%d", eid);
    int t = _elements[eid];
    if (t == TUNNEL_TYPE)
      return "<tunnel" + String(buf) + ">";
    else if (!_element_types[t].factory)
      return "<null" + String(buf) + ">";
    else
      return _element_types[t].name + String(buf);
  }
}

String
Lexer::element_landmark(int eid) const
{
  if (eid < 0 || eid >= _elements.size())
    return "##no-such-element##";
  else if (_element_landmarks[eid])
    return _element_landmarks[eid];
  else
    return "<unknown>";
}


// PARSING

bool
Lexer::yport(int &port)
{
  const Lexeme &tlbrack = lex();
  if (!tlbrack.is('[')) {
    unlex(tlbrack);
    return false;
  }
  
  const Lexeme &tword = lex();
  if (tword.is(lexIdent)) {
    if (!cp_integer(tword.string(), &port)) {
      lerror("syntax error: port number should be integer");
      port = 0;
    }
    expect(']');
    return true;
  } else if (tword.is(']')) {
    lerror("syntax error: expected port number");
    port = 0;
    return true;
  } else {
    lerror("syntax error: expected port number");
    unlex(tword);
    return false;
  }
}

bool
Lexer::yelement(int &element, bool comma_ok)
{
  Lexeme t = lex();
  String name;
  int etype;

  if (t.is(lexIdent)) {
    name = t.string();
    etype = element_type(name);
  } else if (t.is('{')) {
    etype = ycompound();
    name = _element_types[etype].name;
  } else {
    unlex(t);
    return false;
  }

  String configuration, lm;
  const Lexeme &tparen = lex();
  if (tparen.is('(')) {
    lm = landmark();		// report landmark from before config string
    if (etype < 0)
      etype = force_element_type(name);
    configuration = lex_config();
    expect(')');
  } else
    unlex(tparen);
  
  if (etype >= 0)
    element = get_element(anon_element_name(name), etype, configuration, lm);
  else {
    const Lexeme &t2colon = lex();
    unlex(t2colon);
    if (t2colon.is(lex2Colon) || (t2colon.is(',') && comma_ok))
      ydeclaration(name);
    else if (_element_map[name] < 0) {
      lerror("undeclared element '%s' (first use this block)", name.cc());
      get_element(name, ERROR_TYPE);
    }
    element = _element_map[name];
  }
  
  return true;
}

void
Lexer::ydeclaration(const String &first_element)
{
  Vector<String> decls;
  Lexeme t;

  if (first_element) {
    decls.push_back(first_element);
    goto midpoint;
  }
  
  while (true) {
    t = lex();
    if (!t.is(lexIdent))
      lerror("syntax error: expected element name");
    else
      decls.push_back(t.string());
    
   midpoint:
    const Lexeme &tsep = lex();
    if (tsep.is(','))
      /* do nothing */;
    else if (tsep.is(lex2Colon))
      break;
    else {
      lerror("syntax error: expected '::' or ','");
      unlex(tsep);
      return;
    }
  }

  String lm = landmark();
  int etype;
  t = lex();
  if (t.is(lexIdent))
    etype = force_element_type(t.string());
  else if (t.is('{'))
    etype = ycompound();
  else {
    lerror("missing element type in declaration");
    return;
  }
  
  String configuration;
  t = lex();
  if (t.is('(')) {
    configuration = lex_config();
    expect(')');
  } else
    unlex(t);

  for (int i = 0; i < decls.size(); i++) {
    String name = decls[i];
    if (_element_map[name] >= 0) {
      int e = _element_map[name];
      lerror("redeclaration of element '%s'", name.cc());
      if (_elements[e] != TUNNEL_TYPE)
	_errh->lerror(_element_landmarks[e], "element '%s' previously declared here", name.cc());
    } else if (_element_type_map[name] >= 0)
      lerror("'%s' is an element class", name.cc());
    else
      get_element(name, etype, configuration, lm);
  }
}

bool
Lexer::yconnection()
{
  int element1 = -1;
  int port1;
  Lexeme t;
  
  while (true) {
    int element2;
    int port2 = -1;
    
    // get element
    yport(port2);
    if (!yelement(element2, element1 < 0)) {
      if (port1 >= 0)
	lerror("output port useless at end of chain");
      return element1 >= 0;
    }
    
    if (element1 >= 0)
      connect(element1, port1, element2, port2);
    else if (port2 >= 0)
      lerror("input port useless at start of chain");
    
    port1 = -1;
    
   relex:
    t = lex();
    switch (t.kind()) {
      
     case ',':
     case lex2Colon:
      lerror("syntax error before '%#s'", t.string().cc());
      goto relex;
      
     case lexArrow:
      break;
      
     case '[':
      unlex(t);
      yport(port1);
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
	lerror("output port useless at end of chain", port1);
      return true;
      
     default:
      lerror("syntax error near '%#s'", t.string().cc());
      if (t.kind() >= lexIdent)	// save meaningful tokens
	unlex(t);
      return true;
      
    }
    
    // have 'x ->'
    element1 = element2;
  }
}

void
Lexer::yelementclass()
{
  Lexeme tname = lex();
  String name;
  if (tname.is(lexIdent))
    name = tname.string();
  else {
    unlex(tname);
    lerror("expected element type name");
  }

  Lexeme tnext = lex();
  if (tnext.is('{'))
    ycompound(name);
    
  else if (tnext.is(lexIdent)) {
    // define synonym type
    int t = force_element_type(tnext.string());
    add_element_type(name, _element_types[t].factory, _element_types[t].thunk, true);

  } else {
    lerror("syntax error near '%#s'", tnext.string().cc());
    add_element_type(name, error_element_factory, 0, true);
  }
}

void
Lexer::ytunnel()
{
  Lexeme tname1 = lex();
  if (!tname1.is(lexIdent)) {
    unlex(tname1);
    lerror("expected tunnel input name");
  }

  expect(lexArrow);
    
  Lexeme tname2 = lex();
  if (!tname2.is(lexIdent)) {
    unlex(tname2);
    lerror("expected tunnel output name");
  }
  
  if (tname1.is(lexIdent) && tname2.is(lexIdent))
    add_tunnel(tname1.string(), tname2.string());
}

void
Lexer::ycompound_arguments(Compound *comptype)
{
  Lexeme t1, t2;
  
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
	  lerror("expected variable");
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
	lerror("expected variable");
      unlex(t1);
      break;
    }

    comptype->add_formal(varname, vartype);

    const Lexeme &tsep = lex();
    if (tsep.is('|'))
      break;
    else if (!tsep.is(',')) {
      lerror("expected ',' or '|'");
      unlex(tsep);
      break;
    }
  }

  // check argument types
  bool positional = true, error = false;
  for (int i = 0; i < comptype->nformals(); i++)
    if (const String &ftype = comptype->formal_types()[i]) {
      positional = false;
      if (ftype == "__REST__") {
	if (i < comptype->nformals() - 1)
	  error = true;
      } else
	for (int j = i + 1; j < comptype->nformals(); j++)
	  if (comptype->formal_types()[j] == ftype) {
	    lerror("repeated keyword parameter '%s' in compound element", ftype.c_str());
	    break;
	  }
    } else if (!positional)
      error = true;
  if (error)
    lerror("compound element parameters out of order\n(The correct order is '[positional], [keywords], [__REST__]'.)");
}

int
Lexer::ycompound(String name)
{
  HashMap<String, int> old_element_map(-1);
  old_element_map.swap(_element_map);
  HashMap<String, int> old_type_map(_element_type_map);
  int old_offset = _anonymous_offset;
  
  Compound *first = 0, *last = 0;
  int extension = -1;

  while (1) {
    Lexeme dots = lex();
    if (dots.is(lex3Dot)) {
      // '...' marks an extension type
      if (_element_type_map[name] < 0) {
	lerror("cannot extend unknown element class '%s'", name.cc());
	add_element_type(name, error_element_factory, 0, true);
      }
      extension = _element_type_map[name];
      
      dots = lex();
      if (!first || !dots.is('}'))
	lerror("'...' should occur last, after one or more compounds");
      if (dots.is('}') && first)
	break;
    }
    unlex(dots);
    
    // create a compound
    _element_map.clear();
    Compound *ct = new Compound(name, landmark(), _compound_depth);
    ct->swap_router(this);
    get_element("input", TUNNEL_TYPE);
    get_element("output", TUNNEL_TYPE);
    _anonymous_offset = 2;
    _compound_depth++;

    ycompound_arguments(ct);
    while (ystatement(true))
      /* nada */;

    _compound_depth--;
    _anonymous_offset = old_offset;
    _element_type_map = old_type_map;
    ct->swap_router(this);

    ct->finish(this, _errh);

    if (last) {
      int t = add_element_type(name, compound_element_factory, (uintptr_t) ct, true);
      last->set_overload_type(t);
    } else
      first = ct;
    last = ct;

    // check for '||' or '}'
    const Lexeme &t = lex();
    if (!t.is(lex2Bar))
      break;
  }

  // on the way out
  old_element_map.swap(_element_map);

  // add all types to ensure they're freed later
  if (extension)
    last->set_overload_type(extension);
  return add_element_type(name, compound_element_factory, (uintptr_t) first, true);
}

void
Lexer::yrequire()
{
  if (expect('(')) {
    String requirement = lex_config();
    expect(')');
    // pre-read ';' to make it easier to write parsing extensions
    expect(';', false);
    
    Vector<String> args;
    String word;
    cp_argvec(requirement, args);
    for (int i = 0; i < args.size(); i++) {
      Vector<String> words;
      cp_spacevec(args[i], words);
      if (words.size() == 0)
	/* do nothing */;
      else if (!cp_word(words[0], &word))
	lerror("bad requirement: not a word");
      else if (words.size() > 1)
	lerror("bad requirement: too many words");
      else {
	if (_lextra) _lextra->require(word, _errh);
	_requirements.push_back(word);
      }
    }
  }
}

bool
Lexer::ystatement(bool nested)
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
    yelementclass();
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
      lerror("expected '}'");
    return false;
    
   default:
   syntax_error:
    lerror("syntax error near '%#s'", t.string().cc());
    return true;
    
  }
}


// COMPLETION

void
Lexer::add_router_connections(int c, const Vector<int> &router_id,
			      Router *router)
{
  Vector<Hookup> hfrom;
  expand_connection(_hookup_from[c], true, hfrom);
  Vector<Hookup> hto;
  expand_connection(_hookup_to[c], false, hto);
  for (int f = 0; f < hfrom.size(); f++) {
    int eidx = router_id[hfrom[f].idx];
    if (eidx >= 0)
      for (int t = 0; t < hto.size(); t++) {
	int tidx = router_id[hto[t].idx];
	if (tidx >= 0)
	  router->add_connection(eidx, hfrom[f].port, tidx, hto[t].port);
      }
  }
}

void
Lexer::expand_compound_element(int which, const VariableEnvironment &ve)
{
  String name = _element_names[which];
  int etype = _elements[which];
  assert(name);

  // deanonymize element name if necessary
  if (name[0] == ';')
    name = _element_names[which] = deanonymize_element_name(name, which);

  // avoid TUNNEL_TYPE
  if (etype == TUNNEL_TYPE)
    return;

  // expand config string
  _element_configurations[which] = ve.interpolate(_element_configurations[which]);

  // exit if not compound
  if (_element_types[etype].factory != compound_element_factory)
    return;
  Compound *c = (Compound *) _element_types[etype].thunk;
  
  // find right version
  Vector<String> args;
  cp_argvec(_element_configurations[which], args);
  int inputs_used = 0, outputs_used = 0;
  for (int i = 0; i < _hookup_from.size(); i++) {
    const Hookup &hf = _hookup_from[i], &ht = _hookup_to[i];
    if (ht.idx == which && ht.port >= inputs_used)
      inputs_used = ht.port + 1;
    if (hf.idx == which && hf.port >= outputs_used)
      outputs_used = hf.port + 1;
  }
  
  int found_type = c->resolve(this, etype, inputs_used, outputs_used, args, _errh, landmark());

  // check for error or non-compound, or expand compound
  if (found_type < 0)
    _elements[which] = ERROR_TYPE;
  else if (_element_types[found_type].factory != compound_element_factory)
    _elements[which] = found_type;
  else {
    Compound *found_comp = (Compound *) _element_types[found_type].thunk;
    
    VariableEnvironment new_ve;
    new_ve.enter(ve);
    new_ve.limit_depth(found_comp->depth());
    new_ve.enter(found_comp->formals(), args, found_comp->depth());

    found_comp->expand_into(this, which, new_ve);
  }
}

Router *
Lexer::create_router(Master *master)
{
  Router *router = new Router(_big_string, master);
  if (!router)
    return 0;
  
  // expand compounds
  int initial_elements_size = _elements.size();
  VariableEnvironment ve;
  for (int i = 0; i < initial_elements_size; i++)
    expand_compound_element(i, ve);

  // add elements to router
  Vector<int> router_id;
  for (int i = 0; i < _elements.size(); i++) {
    int etype = _elements[i];
    if (etype == TUNNEL_TYPE)
      router_id.push_back(-1);
    else if (Element *e = (*_element_types[etype].factory)(_element_types[etype].thunk)) {
      int ei = router->add_element(e, _element_names[i], _element_configurations[i], _element_landmarks[i]);
      router_id.push_back(ei);
    } else {
      _errh->lerror(_element_landmarks[i], "failed to create element '%s'", _element_names[i].c_str());
      router_id.push_back(-1);
    }
  }
  
  // add connections to router
  for (int i = 0; i < _hookup_from.size(); i++) {
    int fromi = router_id[ _hookup_from[i].idx ];
    int toi = router_id[ _hookup_to[i].idx ];
    if (fromi >= 0 && toi >= 0)
      router->add_connection(fromi, _hookup_from[i].port,
			     toi, _hookup_to[i].port);
    else
      add_router_connections(i, router_id, router);
  }

  // add requirements to router
  for (int i = 0; i < _requirements.size(); i++)
    router->add_requirement(_requirements[i]);

  return router;
}


//
// LEXEREXTRA
//

void
LexerExtra::require(String, ErrorHandler *)
{
}


//
// LEXER::TUNNELEND RELATED STUFF
//

Lexer::TunnelEnd::TunnelEnd(const Router::Hookup &port, bool output,
			    TunnelEnd *next)
  : _port(port), _expanded(0), _output(output), _other(0), _next(next)
{
  assert(!next || next->_output == _output);
}

Lexer::TunnelEnd *
Lexer::TunnelEnd::find(const Router::Hookup &h)
{
  TunnelEnd *d = this;
  TunnelEnd *parent = 0;
  while (d) {
    if (d->_port == h)
      return d;
    else if (d->_port.idx == h.idx)
      parent = d;
    d = d->_next;
  }
  // didn't find the particular port pair; make a new one if possible
  if (parent) {
    Hookup other(parent->_other->_port.idx, h.port);
    TunnelEnd *new_me = new TunnelEnd(h, _output, parent->_next);
    TunnelEnd *new_other = new TunnelEnd(other, !_output, parent->_other->_next);
    new_me->pair_with(new_other);
    parent->_next = new_me;
    parent->_other->_next = new_other;
    return new_me;
  } else
    return 0;
}

void
Lexer::TunnelEnd::expand(const Lexer *lexer, Vector<Router::Hookup> &into)
{
  if (_expanded == 1)
    return;
  
  if (_expanded == 0) {
    _expanded = 1;
    
    Vector<Router::Hookup> connections;
    lexer->find_connections(_other->_port, !_output, connections);

    // give good errors for unused or nonexistent compound element ports
    if (!connections.size()) {
      Hookup inh = (_output ? _other->_port : _port);
      Hookup outh = (_output ? _port : _other->_port);
      String in_name = lexer->element_name(inh.idx);
      String out_name = lexer->element_name(outh.idx);
      if (in_name + "/input" == out_name) {
	const char *message = (_output ? "'%s' input %d unused"
			       : "'%s' has no input %d");
	lexer->errh()->lerror(lexer->element_landmark(inh.idx), message,
			      in_name.cc(), inh.port);
      } else if (in_name == out_name + "/output") {
	const char *message = (_output ? "'%s' has no output %d"
			       : "'%s' output %d unused");
	lexer->errh()->lerror(lexer->element_landmark(outh.idx), message,
			      out_name.cc(), outh.port);
      } else {
	lexer->errh()->lerror(lexer->element_landmark(_other->_port.idx),
			      "tunnel '%s -> %s' %s %d unused",
			      in_name.cc(), out_name.cc(),
			      (_output ? "input" : "output"), _port.idx);
      }
    }

    for (int i = 0; i < connections.size(); i++)
      lexer->expand_connection(connections[i], _output, _correspond);
    
    _expanded = 2;
  }
  
  for (int i = 0; i < _correspond.size(); i++)
    into.push_back(_correspond[i]);
}

void
Lexer::find_connections(const Hookup &this_end, bool is_out,
			Vector<Hookup> &into) const
{
  const Vector<Hookup> &hookup_this(is_out ? _hookup_from : _hookup_to);
  const Vector<Hookup> &hookup_that(is_out ? _hookup_to : _hookup_from);
  for (int i = 0; i < hookup_this.size(); i++)
    if (hookup_this[i] == this_end)
      into.push_back(hookup_that[i]);
}

void
Lexer::expand_connection(const Hookup &this_end, bool is_out,
			 Vector<Hookup> &into) const
{
  if (_elements[this_end.idx] != TUNNEL_TYPE)
    into.push_back(this_end);
  else {
    TunnelEnd *dp = (is_out ? _defoutputs : _definputs);
    if (dp)
      dp = dp->find(this_end);
    if (dp)
      dp->expand(this, into);
    else if ((dp = (is_out ? _definputs : _defoutputs)->find(this_end)))
      _errh->lerror(_element_landmarks[this_end.idx],
		    (is_out ? "'%s' used as output" : "'%s' used as input"),
		    element_name(this_end.idx).cc());
  }
}

#include <click/vector.cc>
CLICK_ENDDECLS
