#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "lexert.hh"
#include "routert.hh"
#include <ctype.h>
#include <stdlib.h>

LexerT::LexerT(ErrorHandler *errh)
  : _data(0), _len(0), _pos(0), _source(0), _lineno(1),
    _tpos(0), _tfull(0), _router(0),
    _errh(errh)
{
  if (!_errh)
    _errh = ErrorHandler::default_handler();
  clear();
}

LexerT::~LexerT()
{
  clear();
}

void
LexerT::reset(LexerTSource *source)
{
  clear();
  _source = source;
  _lineno = 1;
}

void
LexerT::clear()
{
  if (_router)
    _router->unuse();
  _router = new RouterT;
  
  _big_string = "";
  // _data was freed by _big_string
  _data = 0;
  _len = 0;
  _pos = 0;
  _source = 0;
  _lineno = 0;
  _tpos = 0;
  _tfull = 0;
  
  _element_prefix = "";
  _anonymous_prefixes = 1;
}


// LEXING: LOWEST LEVEL

bool
LexerT::get_data()
{
  if (!_source) {
    _errh->error("no configuration source");
    return false;
  }
  
  char *data = 0;
  unsigned cap = 0;
  unsigned len = 0;
  
  while (cap < 65536) {
    cap = (cap ? cap * 2 : 1024);
    char *new_data = new char[cap];
    if (!new_data) break;
    if (data) memcpy(new_data, data, len);
    delete[] data;
    data = new_data;
    
    unsigned read = _source->more_data(data + len, cap - len);
    len += read;
    if (read == 0)
      goto done;
  }
  
  _errh->error("configuration file too large (there's currently a 64K max)");
  delete[] data;
  data = 0;
  cap = len = 0;
  return false;
  
 done:
  if (len < cap) data[len] = 0;
  _big_string = String::claim_string(data, (len < cap ? len + 1 : len));
  _data = _big_string.data();
  _len = len;
  return true;
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

Lexeme
LexerT::next_lexeme()
{
  if (!_data && !get_data()) return Lexeme();
  
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
    } else if (_data[pos] == '#')
      pos = skip_line(pos);
    else
      break;
  }
  
  unsigned word_pos = pos;
  
  // find length of current word
  if (isalnum(_data[pos]) || _data[pos] == '_' || _data[pos] == '/'
      || _data[pos] == '@') {
    while (pos < _len && (isalnum(_data[pos]) || _data[pos] == '_'
			  || _data[pos] == '/' || _data[pos] == '@'))
      pos++;
    _pos = pos;
    String word = _big_string.substring(word_pos, pos - word_pos);
    if (word.length() == 12 && word == "elementclass")
      return Lexeme(lexElementclass, word);
    else if (word.length() == 11 && word == "pseudoports")
      return Lexeme(lexPseudoports, word);
    else if (word.length() == 10 && word == "withprefix")
      return Lexeme(lexWithprefix, word);
    else
      return Lexeme(lexIdent, word);
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
LexerT::lex_config()
{
  if (!_data && !get_data()) return String();
  
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
    } else if (_data[pos] == '\\' && pos < _len - 1 &&
	       (_data[pos+1] == '(' || _data[pos+1] == ')' || _data[pos+1] == '\\'))
      pos++;
  
  _pos = pos;
  return _big_string.substring(config_pos, pos - config_pos);
}

String
LexerT::lexeme_string(int kind)
{
  char buf[12];
  if (kind == lexIdent)
    return "identifier";
  else if (kind == lexArrow)
    return "`->'";
  else if (kind == lex2Colon)
    return "`::'";
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
LexerT::expect(int kind, bool report_error = true)
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
LexerT::landmark() const
{
  return _source ? _source->landmark(_lineno) : String();
}

int
LexerT::lerror(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  _errh->verror(ErrorHandler::Error, landmark(), format, val);
  va_end(val);
  return -1;
}


// ELEMENT TYPES

int
LexerT::element_type(const String &s) const
{
  return _router->type_index(s);
}

int
LexerT::force_element_type(String s)
{
  if (_router->findex(s) >= 0 && _router->type_index(s) < 0)
    lerror("`%s' was previously used as an element name", s.cc());
  return _router->get_type_index(s);
}


// ELEMENTS

String
LexerT::anon_element_name(const String &class_name) const
{
  char buf[100];
  sprintf(buf, "@%d", _router->nelements() + 1);
  return _element_prefix + class_name + String(buf);
}

int
LexerT::make_element(String name, int ftype, const String &conf)
{
  return _router->get_findex(name, ftype, conf, landmark());
}

int
LexerT::make_anon_element(const String &class_name, int ftype,
			  const String &conf)
{
  return make_element(anon_element_name(class_name), ftype, conf);
}

void
LexerT::connect(int element1, int port1, int port2, int element2)
{
  if (port1 < 0) port1 = 0;
  if (port2 < 0) port2 = 0;
  _router->add_connection(Hookup(element1, port1), Hookup(element2, port2),
			  landmark());
}


// PARSING

bool
LexerT::yport(int &port)
{
  const Lexeme &tlbrack = lex();
  if (!tlbrack.is('[')) {
    unlex(tlbrack);
    return false;
  }
  
  const Lexeme &tword = lex();
  if (tword.is(lexIdent)) {
    String p = tword.string();
    const char *ps = p.cc();
    if (isdigit(ps[0]) || ps[0] == '-')
      port = strtol(ps, (char **)&ps, 0);
    if (*ps != 0) {
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
LexerT::yelement(int &element, bool comma_ok)
{
  Lexeme t = lex();
  if (!t.is(lexIdent)) {
    unlex(t);
    return false;
  }
  String name = t.string();
  int ftype = element_type(name);
  
  String configuration;
  const Lexeme &tparen = lex();
  if (tparen.is('(')) {
    if (ftype < 0) ftype = force_element_type(name);
    configuration = lex_config();
    expect(')');
  } else
    unlex(tparen);
  
  if (ftype >= 0)
    element = make_anon_element(name, ftype, configuration);
  else {
    String lookup_name = _element_prefix + name;
    element = _router->findex(lookup_name);
    if (element < 0) {
      const Lexeme &t2colon = lex();
      unlex(t2colon);
      if (t2colon.is(lex2Colon) || (t2colon.is(',') && comma_ok)) {
	ydeclaration(name);
	element = _router->findex(lookup_name);
      } else {
	// assume it's an element type
	ftype = force_element_type(name);
	element = make_anon_element(name, ftype, configuration);
      }
    }
  }
  
  return true;
}

void
LexerT::ydeclaration(const String &first_element)
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
  
  t = lex();
  if (!t.is(lexIdent)) {
    lerror("missing element type in declaration");
    return;
  }
  
  String ftype_name = t.string();
  int ftype = force_element_type(t.string());
  
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
    if (_router->findex(lookup_name) >= 0)
      lerror("element `%s' already declared", name.cc());
    else if (_router->type_index(name) >= 0)
      lerror("`%s' is an element class", name.cc());
    else if (_router->type_index(lookup_name) >= 0)
      lerror("`%s' is an element class", lookup_name.cc());
    else
      make_element(lookup_name, ftype, configuration);
  }
}

bool
LexerT::yconnection()
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
      connect(element1, port1, port2, element2);
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
      unlex(t);
      return true;
      
    }
    
    // have `x ->'
    element1 = element2;
  }
}

void
LexerT::yelementclass()
{
  Lexeme tname = lex();
  String facclass_name;
  if (!tname.is(lexIdent)) {
    unlex(tname);
    lerror("expected element type name");
  } else {
    String n = tname.string();
    if (_router->findex(n) >= 0)
      lerror("`%s' already used as an element name", n.cc());
    else if (_router->type_index(n) >= 0)
      lerror("element type `%s' already exists", n.cc());
    else
      facclass_name = n;
  }
  
  expect('{');
  RouterT *old_router = _router;
  _router = new RouterT;
  _router->get_findex("input", RouterT::PSEUDOPORT_TYPE);
  _router->get_findex("output", RouterT::PSEUDOPORT_TYPE);

  while (ystatement(true))
    /* nada */;
  
  // '}' was consumed

  if (facclass_name)
    old_router->get_type_index(facclass_name, _router);

  _router->unuse();
  _router = old_router;
}

void
LexerT::ypseudoports()
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
      _router->add_pseudoport_pair(tname1.string(), tname2.string(),
				   landmark(), _errh);
    
    const Lexeme &t = lex();
    if (!t.is(',')) {
      unlex(t);
      return;
    }
  }
}

void
LexerT::ywithprefix()
{
  Lexeme tname = lex();
  if (!tname.is(lexIdent)) {
    unlex(tname);
    if (!tname.is('{'))
      lerror("expected prefix (an identifier)");
  }
  
  expect('{');
  String old_prefix = _element_prefix;
  if (tname.is(lexIdent))
    _element_prefix = _element_prefix + tname.string()+"/";
  else {
    _element_prefix = _element_prefix + "@@"+String(_anonymous_prefixes)+"@@/";
    _anonymous_prefixes++;
  }
  
  while (ystatement(true))
    /* do nothing */;
  // '}' was consumed already
  
  _element_prefix = old_prefix;
}

bool
LexerT::ystatement(bool nested)
{
  const Lexeme &t = lex();
  switch (t.kind()) {
    
   case lexIdent:
   case '[':
    unlex(t);
    yconnection();
    return true;
    
   case lexElementclass:
    if (nested || _element_prefix)
      lerror("nested elementclass");
    yelementclass();
    return true;
    
   case lexPseudoports:
    ypseudoports();
    return true;

   case lexWithprefix:
    ywithprefix();
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

RouterT *
LexerT::take_router()
{
  RouterT *r = _router;
  _router = 0;
  return r;
}


//
// LexerTSOURCES
//

String
LexerTSource::landmark(unsigned lineno) const
{
  return String("line ") + String(lineno);
}


#ifndef __KERNEL__

FileLexerTSource::FileLexerTSource(const char *filename, FILE *f)
  : _filename(filename), _f(f), _own_f(f == 0)
{
  if (!_f) _f = fopen(filename, "r");
}

FileLexerTSource::~FileLexerTSource()
{
  if (_f && _own_f) fclose(_f);
}

unsigned
FileLexerTSource::more_data(char *data, unsigned max)
{
  if (!_f) return 0;
  
  return fread(data, 1, max, _f);
  // if amt == 0, is it possible there's been a transient error?
}

String
FileLexerTSource::landmark(unsigned lineno) const
{
  return String(_filename) + ":" + String(lineno);
}

#endif


MemoryLexerTSource::MemoryLexerTSource(const char *data, unsigned len)
  : _data(data), _pos(0), _len(len)
{
}
    
unsigned
MemoryLexerTSource::more_data(char *buf, unsigned max)
{
  if (_pos + max > _len)
    max = _len - _pos;
  memcpy(buf, _data + _pos, max);
  _pos += max;
  return max;
}
