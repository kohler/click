/*
 * lexer.{cc,hh} -- parses Click language files, produces Router objects
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "lexer.hh"
#include "errorelement.hh"
#include "router.hh"
#include "error.hh"
#include "confparse.hh"
#include "glue.hh"

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

class Lexer::Compound : public Element {
  
  mutable String _name;
  String _body;
  String _filename;
  unsigned _lineno;
  Vector<String> _arguments;
  int _scope;
  
 public:
  
  Compound(const String &, const String &, const String &, unsigned,
	   const Vector<String> &);
  Compound(const Compound &);
  
  const char *class_name() const	{ return _name.cc(); }
  void *cast(const char *);
  Compound *clone() const		{ return 0; }

  void set_scope(int s)			{ _scope = s; }
  
  const String &body() const		{ return _body; }
  const String &filename() const	{ return _filename; }
  unsigned lineno() const		{ return _lineno; }
  int narguments() const		{ return _arguments.size(); }
  const String &argument(int i) const	{ return _arguments[i]; }
  int scope() const			{ return _scope; }
  
};

Lexer::Compound::Compound(const String &name, const String &body,
			  const String &filename, unsigned lineno,
			  const Vector<String> &args)
  : _name(name), _body(body), _filename(filename), _lineno(lineno),
    _arguments(args), _scope(-1)
{
}

Lexer::Compound::Compound(const Compound &o)
  : Element(), _name(o._name), _body(o._body), _filename(o._filename),
    _lineno(o._lineno), _arguments(o._arguments), _scope(o._scope)
{
}

void *
Lexer::Compound::cast(const char *s)
{
  if (String("Lexer::Compound") == s || _name == s)
    return this;
  else
    return 0;
}

//
// LEXER
//

Lexer::Lexer(ErrorHandler *errh)
  : _data(0), _len(0), _pos(0), _lineno(1), _lextra(0),
    _tpos(0), _tfull(0),
    _element_type_map(-1),
    _tunnel_element_type(new ErrorElement),
    _last_element_type(-1),
    _free_element_type(-1),
    _variable_map(-1),
    _element_map(-1),
    _definputs(0), _defoutputs(0),
    _errh(errh)
{
  if (!_errh) _errh = ErrorHandler::default_handler();
  end_parse(-1);		// clear private state
  add_element_type("<tunnel>", _tunnel_element_type);
  add_element_type(new ErrorElement);
  assert(element_type("<tunnel>") == TUNNEL_TYPE
	 && element_type("Error") == ERROR_TYPE);
}

Lexer::~Lexer()
{
  end_parse(-1);
  // _default_element_type and _tunnel_element_type removed by end_parse()
}

int
Lexer::begin_parse(const String &data, const String &filename,
		   LexerExtra *lextra)
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
  return _last_element_type;
}

void
Lexer::end_parse(int cookie)
{
  lexical_scoping_out(cookie);
  for (int i = 0; i < _elements.size(); i++)
    // don't delete _tunnel_element_type!
    if (_elements[i] != _tunnel_element_type)
      delete _elements[i];
  
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
  
  _element_prefix = "";
  _anonymous_offset = 0;
}


// LEXING: LOWEST LEVEL

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
Lexer::skip_quote(unsigned pos, char endc)
{
  for (; pos < _len; pos++)
    if (_data[pos] == '\n')
      _lineno++;
    else if (_data[pos] == '\r') {
      if (pos < _len - 1 && _data[pos+1] == '\n') pos++;
      _lineno++;
    } else if (_data[pos] == '\\' && pos < _len - 1 && endc == '\"'
	       && _data[pos+1] == endc)
      pos++;
    else if (_data[pos] == endc)
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
    }
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
  bool have_arguments = _variable_values.size() != 0;
  String output;
  
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
      pos = skip_quote(pos + 1, _data[pos]);
    else if (_data[pos] == '$' && have_arguments) {
      unsigned word_pos = pos;
      for (pos++; isalnum(_data[pos]) || _data[pos] == '_'; pos++)
	/* nada */;
      String name = _big_string.substring(word_pos, pos - word_pos);
      int variable = _variable_map[name];
      if (variable >= 0) {
	output += _big_string.substring(config_pos, word_pos - config_pos);
	output += _variable_values[variable];
	config_pos = pos;
      }
      pos--;
    }
  
  _pos = pos;
  if (!output)
    return _big_string.substring(config_pos, pos - config_pos);
  else
    return output + _big_string.substring(config_pos, pos - config_pos);
}

String
Lexer::lex_compound_body()
{
  unsigned config_pos = _pos;
  unsigned pos = _pos;
  unsigned paren_depth = 0;
  unsigned brace_depth = 1;
  for (; pos < _len; pos++)
    if (_data[pos] == '(')
      paren_depth++;
    else if (_data[pos] == ')' && paren_depth)
      paren_depth--;
    else if (_data[pos] == '{' && !paren_depth)
      brace_depth++;
    else if (_data[pos] == '}' && !paren_depth) {
      brace_depth--;
      if (!brace_depth) break;
    } else if (_data[pos] == '/' && pos < _len - 1) {
      if (_data[pos+1] == '/')
	pos = skip_line(pos + 2);
      else if (_data[pos+1] == '*')
	pos = skip_slash_star(pos + 2);
    } else if (_data[pos] == '\n')
      _lineno++;
    else if (_data[pos] == '\r') {
      if (pos < _len - 1 && _data[pos+1] == '\n') pos++;
      _lineno++;
    } else if ((_data[pos] == '\'' || _data[pos] == '\"') && paren_depth)
      pos = skip_quote(pos + 1, _data[pos]);
  
  _pos = pos;
  return _big_string.substring(config_pos, pos - config_pos);
}

String
Lexer::lexeme_string(int kind)
{
  char buf[12];
  if (kind == lexIdent)
    return "identifier";
  else if (kind == lexIdent)
    return "variable";
  else if (kind == lexArrow)
    return "`->'";
  else if (kind == lex2Colon)
    return "`::'";
  else if (kind == lexTunnel)
    return "`connectiontunnel'";
  else if (kind == lexElementclass)
    return "`elementclass'";
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
  _tcircle[_tfull] = t;
  _tfull = (_tfull + 1) % TCIRCLE_SIZE;
  assert(_tfull != _tpos);
}

bool
Lexer::expect(int kind, bool report_error = true)
{
  const Lexeme &t = lex();
  if (t.is(kind))
    return true;
  else {
    if (report_error)
      lerror("expected %s", lexeme_string(kind).cc());
    unlex(t);
    return false;
  }
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
  _errh->verror(ErrorHandler::Error, landmark(), format, val);
  va_end(val);
  return -1;
}


// ELEMENT TYPES

int
Lexer::add_element_type(Element *e)
{
  // Lexer now owns `e'
  return add_element_type(e->class_name(), e);
}

int
Lexer::add_element_type(const String &name, Element *e)
{
  // Lexer now owns `e'
  int tid;
  if (_free_element_type < 0) {
    tid = _element_types.size();
    _element_types.push_back(e);
    _element_type_names.push_back(name);
    _element_type_next.push_back(_last_element_type);
  } else {
    tid = _free_element_type;
    _free_element_type = _element_type_next[tid];
    _element_types[tid] = e;
    _element_type_names[tid] = name;
    _element_type_next[tid] = _last_element_type;
  }
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
  lerror("unknown element class `%s'", s.cc());
  return add_element_type(s, new ErrorElement);
}

int
Lexer::lexical_scoping_in() const
{
  return _last_element_type;
}

void
Lexer::lexical_scoping_out(int last)
{
  if (last != -1) {
    for (int t = _last_element_type; t >= 0; t = _element_type_next[t])
      if (t == last)
	goto ok;
    return;
  }

 ok:
  while (_last_element_type != last)
    remove_element_type(_last_element_type);
}

void
Lexer::remove_element_type(int removed)
{
  // patch next array
  int prev = -1, trav = _last_element_type;
  while (trav != removed && trav >= 0) {
    prev = trav;
    trav = _element_type_next[trav];
  }
  if (trav < 0)
    return;
  if (prev >= 0)
    _element_type_next[prev] = _element_type_next[removed];
  else
    _last_element_type = _element_type_next[removed];

  // fix up element type name map
  const String &name = _element_type_names[removed];
  if (_element_type_map[name] == removed) {
    for (trav = _element_type_next[removed];
	 trav >= 0 && _element_type_names[trav] != name;
	 trav = _element_type_next[trav])
      /* nada */;
    _element_type_map.insert(name, trav);
  }

  // remove stuff
  _element_type_names[removed] = String();
  delete _element_types[removed];
  _element_types[removed] = 0;
  _element_type_next[removed] = _free_element_type;
  _free_element_type = removed;
}

void
Lexer::element_type_names(Vector<String> &v) const
{
  for (HashMap<String, int>::Iterator i = _element_type_map.first(); i; i++)
    if (i.value() >= 0 && i.key() != "<tunnel>")
      v.push_back(i.key());
}


// PORT TUNNELS

void
Lexer::add_tunnel(String namein, String nameout)
{
  Hookup hin(get_element(_element_prefix + namein, TUNNEL_TYPE), 0);
  Hookup hout(get_element(_element_prefix + nameout, TUNNEL_TYPE), 0);
  
  bool ok = true;
  if (_elements[hin.idx] != _tunnel_element_type)
    lerror("element `%s' already declared %s", namein.cc(), _elements[hin.idx]->class_name()), ok = 0;
  if (_elements[hout.idx] != _tunnel_element_type)
    lerror("element `%s' already declared", nameout.cc()), ok = 0;
  if (_definputs && _definputs->find(hin))
    lerror("connection tunnel input `%s' already defined", namein.cc()), ok = 0;
  if (_defoutputs && _defoutputs->find(hout))
    lerror("connection tunnel output `%s' already defined", nameout.cc()), ok = 0;
  if (ok) {
    _definputs = new TunnelEnd(hin, false, _definputs);
    _defoutputs = new TunnelEnd(hout, true, _defoutputs);
    _definputs->pair_with(_defoutputs);
  }
}

// ELEMENTS

int
Lexer::get_element(String name, int etype, const String &conf,
		   const String &lm)
{
  assert(name && etype >= 0 && etype < _element_types.size());
  
  // if an element `name' already exists return it
  if (_element_map[name] >= 0)
    return _element_map[name];

  // check type
  Element *e;
  Element *et = _element_types[etype];
  if (etype == TUNNEL_TYPE)
    e = et;
  else if ((e = et->clone()))
    /* do nothing */;
  else if (et->cast("Lexer::Compound"))
    return make_compound_element(name, etype, conf);
  else {
    lerror("can't clone `%s'", et->declaration().cc());
    e = 0;
  }

  int eid = _elements.size();
  _element_map.insert(name, eid);
  _element_names.push_back(name);
  _element_configurations.push_back(conf);
  _element_landmarks.push_back(lm ? lm : landmark());
  _elements.push_back(e);
  return eid;
}

String
Lexer::anon_element_name(const String &class_name) const
{
  char buf[100];
  sprintf(buf, "@%d", _elements.size() - _anonymous_offset + 1);
  return _element_prefix + class_name + String(buf);
}

void
Lexer::lexical_scoping_back(int first, Vector<int> &insertions)
{
  for (int t = _last_element_type; t >= 0; t = _element_type_next[t]) {
    const String &name = _element_type_names[t];
    if (_element_type_map[name] == t) {
      // remove it from map
      int prev = _element_type_next[first];
      while (prev >= 0 && _element_type_names[prev] != name)
	prev = _element_type_next[prev];
      _element_type_map.insert(name, prev);
      insertions.push_back(t);
    }
    if (t == first)
      break;
  }
}

void
Lexer::lexical_scoping_forward(const Vector<int> &insertions)
{
  for (int i = 0; i < insertions.size(); i++) {
    int j = insertions[i];
    _element_type_map.insert(_element_type_names[j], j);
  }
}

int
Lexer::make_compound_element(String name, int etype, const String &conf)
{
  Compound *compound = (Compound *)_element_types[etype];
  if (!name)
    name = anon_element_name(compound->class_name());
  // change `name' to not contain the current prefix
  name = name.substring(_element_prefix.length());

  // handle configuration string
  Vector<String> args;
  cp_argvec(conf, args);
  int nargs = compound->narguments();
  if (args.size() != nargs) {
    const char *whoops = (args.size() < nargs ? "few" : "many");
    String signature;
    for (int i = 0; i < nargs; i++) {
      if (i) signature += ", ";
      signature += compound->argument(i);
    }
    lerror("too %s arguments to compound element `%s(%s)'", whoops,
	   compound->class_name(), signature.cc());
    for (int i = args.size(); i < nargs; i++)
      args.push_back("");
  }
  
  // store arguments
  Vector<int> prev_variable_value;
  for (int i = 0; i < nargs; i++) {
    const String &name = compound->argument(i);
    int prev = _variable_map[name];
    _variable_values.push_back(args[i]);
    prev_variable_value.push_back(prev);
    _variable_map.insert(name, _variable_values.size() - 1);
  }

  // `name_slash' is `name' constrained to end with a slash
  String name_slash;
  if (name[name.length() - 1] == '/')
    name_slash = name;
  else
    name_slash = name + "/";
  
  int fake_index = get_element(_element_prefix + name, TUNNEL_TYPE);
  add_tunnel(name, name_slash + "input");
  add_tunnel(name_slash + "output", name);
  
  // save parser state
  String old_prefix = _element_prefix;
  String old_big_string = _big_string;
  unsigned old_len = _len;
  unsigned old_pos = _pos;
  String old_filename = _filename;
  unsigned old_lineno = _lineno;
  Lexeme old_tcircle[TCIRCLE_SIZE];
  old_tcircle = _tcircle;
  int old_tpos = _tpos;
  int old_tfull = _tfull;
  int old_anonymous_offset = _anonymous_offset;

  // reparse the saved compound element's body!
  _element_prefix += name_slash;
  _big_string = compound->body();
  _data = _big_string.data();
  _len = _big_string.length();
  _pos = 0;
  _filename = compound->filename();
  _lineno = compound->lineno();
  _tpos = _tfull;
  // manipulate the anonymous offset so anon elements in 2 instances of the
  // compound have identical suffixes
  int old_elements_size = _elements.size();
  _anonymous_offset = _elements.size();
  
  // lexical scoping of types
  int cookie = lexical_scoping_in();
  Vector<int> lexical_scoping_help;
  lexical_scoping_back(compound->scope(), lexical_scoping_help);
  
  while (ystatement())
    /* do nothing */;

  // restore parser state
  _element_prefix = old_prefix;
  _big_string = old_big_string;
  _data = _big_string.data();
  _len = old_len;
  _pos = old_pos;
  _filename = old_filename;
  _lineno = old_lineno;
  _tcircle = old_tcircle;
  _tpos = old_tpos;
  _tfull = old_tfull;
  // manipulate the anonymous offset to get consistent results with tools
  _anonymous_offset = old_anonymous_offset + _elements.size()
    - old_elements_size + 2;

  // lexical scoping fixup
  lexical_scoping_out(cookie);
  lexical_scoping_forward(lexical_scoping_help);
  
  // lexical scoping of arguments
  for (int i = nargs - 1; i >= 0; i--) {
    const String &name = compound->argument(i);
    _variable_map.insert(name, prev_variable_value[i]);
  }
  _variable_values.resize(_variable_values.size() - nargs);

  return fake_index;
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
Lexer::element_name(int fid) const
{
  if (fid < 0 || fid >= _elements.size())
    return "##no-such-element##";
  else if (_element_names[fid])
    return _element_names[fid];
  else {
    char buf[100];
    sprintf(buf, "@%d", fid);
    if (_elements[fid] == _tunnel_element_type)
      return "<tunnel " + String(buf) + ">";
    else if (!_elements[fid])
      return "<null " + String(buf) + ">";
    else
      return _elements[fid]->class_name() + String(buf);
  }
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
Lexer::yelement_upref(int &element)
{
  Lexeme t = lex();
  if (!t.is(lexIdent)) {
    lerror("syntax error: expected element name");
    unlex(t);
    return false;
  }

  String prefix = _element_prefix;
  while (1) {
    int pos = prefix.find_right('/', prefix.length() - 2);
    prefix = (pos >= 0 ? prefix.substring(0, pos + 1) : String());
    
    String name = prefix + t.string();
    int en = _element_map[name];
    if (en >= 0) {
      element = en;
      return true;
    }

    if (!prefix) {
      lerror("`%s' not found in enclosing scopes", t.string().cc());
      return false;
    }
  }
}

bool
Lexer::yelement(int &element, bool comma_ok)
{
  Lexeme t = lex();
  String name;
  int etype;

  if (t.is('^')) {
    return yelement_upref(element);
  } else if (t.is(lexIdent)) {
    name = t.string();
    etype = element_type(name);
  } else if (t.is('{')) {
    etype = ylocal();
    name = _element_types[etype]->class_name();
  } else {
    unlex(t);
    return false;
  }

  String configuration, lm;
  const Lexeme &tparen = lex();
  if (tparen.is('(')) {
    lm = landmark();		// report landmark from before config string
    if (etype < 0) etype = force_element_type(name);
    configuration = lex_config();
    expect(')');
  } else
    unlex(tparen);
  
  if (etype >= 0)
    element = get_element(anon_element_name(name), etype, configuration, lm);
  else {
    String lookup_name = _element_prefix + name;
    element = _element_map[lookup_name];
    if (element < 0) {
      const Lexeme &t2colon = lex();
      unlex(t2colon);
      if (t2colon.is(lex2Colon) || (t2colon.is(',') && comma_ok))
	ydeclaration(name);
      else {
	lerror("undeclared element `%s' (first use this block)", name.cc());
	get_element(lookup_name, ERROR_TYPE);
      }
      element = _element_map[lookup_name];
    }
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
      lerror("syntax error: expected `::' or `,'");
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
    etype = ylocal();
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
    String lookup_name = _element_prefix + name;
    if (_element_map[lookup_name] >= 0)
      lerror("element `%s' already declared", name.cc());
    else if (_element_type_map[name] >= 0)
      lerror("`%s' is an element class", name.cc());
    else if (_element_type_map[lookup_name] >= 0)
      lerror("`%s' is an element class", lookup_name.cc());
    else
      get_element(lookup_name, etype, configuration, lm);
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
      lerror("syntax error before `%s'", t.string().cc());
      goto relex;
      
     case lexArrow:
      break;
      
     case '[':
      unlex(t);
      yport(port1);
      goto relex;
      
     case lexIdent:
     case '^':
     case '{':
     case '}':
      unlex(t);
      // FALLTHRU
     case ';':
     case lexEOF:
      if (port1 >= 0)
	lerror("output port useless at end of chain", port1);
      return true;
      
     default:
      lerror("syntax error near `%s'", t.string().cc());
      if (t.kind() >= lexIdent)	// save meaningful tokens
	unlex(t);
      return true;
      
    }
    
    // have `x ->'
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
    ylocal(name);
    
  else if (tnext.is(lexIdent)) {
    // Go through some rigamarole because other code always assumes that
    // _element_type_map[x] == _element_type_map[y] <==>
    int t = force_element_type(tnext.string());
    Element *et = _element_types[t];
    Element *e = et->clone();
    if (!e) {
      Compound *comp = static_cast<Compound *>(et->cast("Lexer::Compound"));
      if (comp)
	e = new Compound(*comp);
    }
    if (!e) {
      lerror("can't clone `%s'", et->declaration().cc());
      add_element_type(name, new ErrorElement);
    } else
      add_element_type(name, e);

  } else {
    lerror("expected element class");
    add_element_type(name, new ErrorElement);
  }
}

void
Lexer::ytunnel()
{
  while (true) {
    Lexeme tname1 = lex();
    if (!tname1.is(lexIdent)) {
      unlex(tname1);
      lerror("expected port name");
    }
    
    expect(lexArrow);
    
    Lexeme tname2 = lex();
    if (!tname2.is(lexIdent)) {
      unlex(tname2);
      lerror("expected port name");
    }
    
    if (tname1.is(lexIdent) && tname2.is(lexIdent))
      add_tunnel(tname1.string(), tname2.string());
    
    const Lexeme &t = lex();
    if (!t.is(',')) {
      unlex(t);
      return;
    }
  }
}

int
Lexer::ylocal(String name)
{
  // OK because every used ylocal() corresponds to at least one element
  if (!name)
    name = "@X" + String(_elements.size() - _anonymous_offset + 1);

  // check for arguments
  Vector<String> arguments;
  unsigned old_pos = _pos;
  while (1) {
    Lexeme t = lex();
    if (!t.is(lexVariable))
      break;
    arguments.push_back(t.string());
    old_pos = _pos;
    const Lexeme &sep = lex();
    if (sep.is('|')) {
      old_pos = _pos;
      break;
    } else if (!sep.is(','))
      break;
  }
  _pos = old_pos;
  
  // opening brace was already read
  String body_filename = _filename;
  unsigned body_lineno = _lineno;
  String body = lex_compound_body();
  expect('}');

  Compound *c =
    new Compound(name, body, body_filename, body_lineno, arguments);
  int t = add_element_type(name, c);
  c->set_scope(t);
  return t;
}

void
Lexer::yrequire()
{
  if (expect('(')) {
    String requirement = lex_config();
    Vector<String> args;
    cp_argvec(requirement, args);
    String word;
    for (int i = 0; i < args.size(); i++) {
      if (!cp_word(args[i], &word))
	lerror("bad requirement: should be a single word");
      else {
	if (_lextra)
	  _lextra->require(word, _errh);
	_requirements.push_back(word);
      }
    }
    expect(')');
  }
}

bool
Lexer::ystatement(bool nested)
{
  const Lexeme &t = lex();
  switch (t.kind()) {
    
   case lexIdent:
   case '^':
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
    if (!nested)
      goto syntax_error;
    return false;
    
   case lexEOF:
    if (nested)
      lerror("expected `}'");
    return false;
    
   default:
   syntax_error:
    lerror("syntax error near `%s'", String(t.string()).cc());
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
    int fidx = router_id[hfrom[f].idx];
    if (fidx >= 0)
      for (int t = 0; t < hto.size(); t++) {
	int tidx = router_id[hto[t].idx];
	if (tidx >= 0)
	  router->add_connection(fidx, hfrom[f].port, tidx, hto[t].port);
      }
  }
}

Router *
Lexer::create_router()
{
  Router *router = new Router;
  if (!router)
    return 0;
  
  // check for elements w/o types
  for (int i = 0; i < _elements.size(); i++)
    if (!_elements[i])
      _errh->error("undeclared element `%s'", element_name(i).cc());
  
  // add elements to router
  Vector<int> router_id;
  for (int i = 0; i < _elements.size(); i++)
    if (_elements[i] && _elements[i] != _tunnel_element_type) {
      int fi = router->add_element
	(_elements[i], _element_names[i], _element_configurations[i],
	 _element_landmarks[i]);
      router_id.push_back(fi);
    } else
      router_id.push_back(-1);
  
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

  // clear _elements, as all elements are owned by router
  _elements.assign(_elements.size(), (Element *)0);
  
  return router;
}


//
// LEXEREXTRA
//

void
LexerExtra::require(const String &, ErrorHandler *)
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
  if (_elements[this_end.idx] != _tunnel_element_type)
    into.push_back(this_end);
  else {
    TunnelEnd *dp = (is_out ? _defoutputs : _definputs);
    if (dp)
      dp = dp->find(this_end);
    if (dp)
      dp->expand(this, into);
    else if ((dp = (is_out ? _definputs : _defoutputs)->find(this_end)))
      _errh->error("connection tunnel %s `%s' used as %s",
		   element_name(this_end.idx).cc(),
		   (is_out ? "input" : "output"),
		   (is_out ? "output" : "input"));
  }
}
