/*
 * cxxclass.{cc,hh} -- representation of C++ classes
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "cxxclass.hh"
#include "straccum.hh"

CxxFunction::CxxFunction(const String &name, bool in_header,
			 const String &ret_type, const String &args,
			 const String &body, const String &clean_body)
  : _name(name), _in_header(in_header), _ret_type(ret_type), _args(args),
    _body(body), _clean_body(clean_body)
{
  //fprintf(stderr, "%s::%s\n", _name.cc(), _body.cc());
}

String
compile_pattern(const String &pattern0)
{
  static const char *three_tokens[] = { ">>=", "<<=", "->*", "::*", 0 };
  static const char *two_tokens[] = { "++", "--", "+=", "-=", "*=", "/=", "->",
				      "%=", "^=", "&=", "~=", "==", "!=",
				      "&&", "||",
				      ">=", "<=", "::", "<<", ">>", ".*", 0 };
  
  StringAccum sa;
  const char *s = pattern0.data();
  const char *end_s = s + pattern0.length();

  while (s < end_s && isspace(*s)) // skip leading space
    s++;

  // XXX not all constraints on patterns are expressed here
  while (s < end_s) {
    if (isspace(*s)) {
      sa << ' ';
      while (s < end_s && isspace(*s))
	s++;
      
    } else if (isalnum(*s) || *s == '_') {
      while (s < end_s && (isalnum(*s) || *s == '_'))
	sa << *s++;
      sa << ' ';
      
    } else if (*s == '#') {
      assert(s < end_s - 1 && isdigit(s[1]));
      sa << s[0] << s[1];
      s += 2;
      
    } else {
      const char *token = 0;
      if (s < end_s - 2)
	for (int i = 0; !token && three_tokens[i]; i++)
	  if (strncmp(three_tokens[i], s, 3) == 0)
	    token = three_tokens[i];
      if (!token && s < end_s - 1)
	for (int i = 0; !token && two_tokens[i]; i++)
	  if (strncmp(two_tokens[i], s, 2) == 0)
	    token = two_tokens[i];
      if (!token)
	sa << *s++ << ' ';
      else {
	sa << token << ' ';
	s += strlen(token);
      }
    }
  }

  return sa.take_string();
}

bool
CxxFunction::find_expr(const String &pattern, int *pos1, int *pos2,
		       int match_pos[10], int match_len[10]) const
{
  const char *ps = pattern.data();
  int plen = pattern.length();

  const char *ts = _clean_body.data();
  int tpos = 0;
  int tlen = _clean_body.length();

  while (tpos < tlen) {

    // fast loop: look for occurrences of first character in pattern
    while (tpos < tlen && ts[tpos] != ps[0])
      tpos++;

    int tpos1 = tpos;
    tpos++;
    int ppos = 1;

    while (tpos < tlen && ppos < plen) {

      if (isspace(ps[ppos])) {
	while (tpos < tlen && isspace(ts[tpos]))
	  tpos++;
	ppos++;

      } else if (ps[ppos] == '#') {
	// save expr and skip over it
	int paren_level = 0;
	int question_level = 0;
	int which = ps[ppos+1] - '0';
	match_pos[which] = tpos;
	while (tpos < tlen) {
	  if (ts[tpos] == '(')
	    paren_level++;
	  else if (ts[tpos] == ')') {
	    if (paren_level == 0)
	      break;
	    paren_level--;
	  } else if (ts[tpos] == ',') {
	    if (paren_level == 0 && question_level == 0)
	      break;
	  } else if (ts[tpos] == '?')
	    question_level++;
	  else if (ts[tpos] == ':' && question_level)
	    question_level--;
	  tpos++;
	}
	match_len[which] = tpos - match_pos[which];
	ppos += 2;

      } else if (ps[ppos] == ts[tpos])
	ppos++, tpos++;

      else
	break;

    }

    if (ppos >= plen) {
      // check that this pattern match didn't occur after some evil qualifier,
      // namely `.', `::', or `->'
      int p = tpos1 - 1;
      while (p >= 0 && isspace(ts[p]))
	p--;
      if (p < 0
	  || (ts[p] != '.'
	      && (p == 0 || ts[p-1] != ':' || ts[p] != ':')
	      && (p == 0 || ts[p-1] != '-' || ts[p] != '>'))) {
	*pos1 = tpos1;
	*pos2 = tpos;
	return true;
      }
    }

    // look for next match
    tpos = tpos1 + 1;
  }

  return false;
}

bool
CxxFunction::find_expr(const String &pattern) const
{
  int pos1, pos2, match_pos[10], match_len[10];
  return find_expr(pattern, &pos1, &pos2, match_pos, match_len);
}

bool
CxxFunction::replace_expr(const String &pattern, const String &replacement)
{
  int pos1, pos2, match_pos[10], match_len[10];
  if (!find_expr(pattern, &pos1, &pos2, match_pos, match_len))
    return false;

  fprintf(stderr, ":::::: %s\n", _body.cc());
  
  StringAccum sa, clean_sa;
  const char *s = replacement.data();
  const char *end_s = s + replacement.length();
  while (s < end_s) {
    if (*s == '#') {
      assert(s < end_s - 1 && isdigit(s[1]));
      int which = s[1] - '0';
      sa << _body.substring(match_pos[which], match_len[which]);
      clean_sa << _clean_body.substring(match_pos[which], match_len[which]);
      s += 2;
    } else {
      sa << *s;
      clean_sa << *s;
      s++;
    }
  }

  String new_body =
    _body.substring(0, pos1) + sa.take_string() + _body.substring(pos2);
  String new_clean_body =
    _clean_body.substring(0, pos1) + clean_sa.take_string()
    + _clean_body.substring(pos2);
  _body = new_body;
  _clean_body = new_clean_body;

  fprintf(stderr, ">>>>>> %s\n", _body.cc());
  return true;
}


/*****
 * CxxClass
 **/

CxxClass::CxxClass(const String &name)
  : _name(name), _fn_map(-1)
{
  //fprintf(stderr, "!!!!!%s\n", _name.cc());
}

void
CxxClass::defun(const CxxFunction &fn)
{
  int which = _functions.size();
  _functions.push_back(fn);
  _fn_map.insert(fn.name(), which);
}

bool
CxxClass::reach(int findex, Vector<int> &reached)
{
  if (findex < 0)
    return false;
  if (reached[findex])
    return _reachable_rewritable[findex];
  reached[findex] = true;
  
  // return true if reachable and rewritable
  const String &clean_body = _functions[findex].clean_body();
  const char *s = clean_body.data();
  int p = 0;
  int len = clean_body.length();
  bool reachable_rewritable = _rewritable[findex];

  while (p < len) {
    
    // look for a function call
    while (p < len && s[p] != '(')
      p++;
    if (p >= len)
      break;

    int paren_p = p;
    for (p--; p >= 0 && isspace(s[p]); p--)
      /* nada */;
    if (p < 0 || (!isalnum(s[p]) && s[p] != '_')) {
      p = paren_p + 1;
      continue;
    }
    int end_word_p = p + 1;
    while (p >= 0 && (isalnum(s[p]) || s[p] == '_'))
      p--;
    int start_word_p = p + 1;
    while (p >= 0 && isspace(s[p]))
      p--;

    // have found word; check that it is a direct call
    if (p >= 0 && (s[p] == '.' || (p > 0 && s[p-1] == '-' && s[p] == '>')))
      /* do nothing; a call of some object */;
    else {
      // XXX class-qualified?
      String name = clean_body.substring(start_word_p, end_word_p - start_word_p);
      int findex2 = _fn_map[name];
      if (findex2 >= 0 && reach(findex2, reached))
	reachable_rewritable = true;
    }

    // skip past word
    p = paren_p + 1;
  }

  _reachable_rewritable[findex] = reachable_rewritable;
  return reachable_rewritable;
}

void
CxxClass::mark_reachable_rewritable()
{
  _rewritable.assign(nfunctions(), 0);
  String push_pattern = compile_pattern("output(#0).push(#1)");
  String pull_pattern = compile_pattern("input(#0).pull()");
  String checked_push_pattern = compile_pattern("checked_push_output(#0,#1)");
  for (int i = 0; i < nfunctions(); i++) {
    if (_functions[i].find_expr(push_pattern)
	|| _functions[i].find_expr(pull_pattern)
	|| _functions[i].find_expr(checked_push_pattern))
      _rewritable[i] = 1;
  }

  _reachable_rewritable.assign(nfunctions(), 0);
  Vector<int> reached(nfunctions(), 0);
  reach(_fn_map["push"], reached);
  reach(_fn_map["pull"], reached);
  reach(_fn_map["simple_action"], reached);

  for (int i = 0; i < nfunctions(); i++)
    fprintf(stderr, "%s %s %s\n", _functions[i].name().cc(),
	    _rewritable[i] ? "rewritable" : "-",
	    _reachable_rewritable[i] ? "reachable" : "-");
}


/*****
 * CxxInfo
 **/

CxxInfo::CxxInfo()
  : _class_map(-1)
{
}

CxxInfo::~CxxInfo()
{
  for (int i = 0; i < _classes.size(); i++)
    delete _classes[i];
}

CxxClass *
CxxInfo::make_class(const String &name)
{
  int which = _class_map[name];
  if (which < 0) {
    CxxClass *nclass = new CxxClass(name);
    which = _classes.size();
    _classes.push_back(nclass);
    _class_map.insert(name, which);
  }
  return _classes[which];
}

static String
remove_crap(const String &original_text)
{
  // Get rid of preprocessor directives, comments, string literals, and
  // character literals by replacing them with the right number of spaces.

  const char *s = original_text.data();
  const char *end_s = s + original_text.length();

  StringAccum new_text;
  char *o = new_text.extend(original_text.length());

  while (s < end_s) {
    // read one line

    // skip spaces at beginning of line
    while (s < end_s && isspace(*s))
      *o++ = *s++;

    if (s >= end_s)		// end of data
      break;

    if (*s == '#') {		// preprocessor directive
      while (1) {
	while (s < end_s && *s != '\n' && *s != '\r')
	  *o++ = ' ', s++;
	bool backslash = (s[-1] == '\\');
	while (s < end_s && (*s == '\n' || *s == '\r'))
	  *o++ = *s++;
	if (!backslash)
	  break;
      }
      continue;
    }

    // scan; stop at EOL, comment start, or literal start
    while (s < end_s && *s != '\n' && *s != '\r') {

      // copy chars
      while (s < end_s && *s != '/' && *s != '\"' && *s != '\''
	     && *s != '\n' && *s != '\r')
	*o++ = *s++;

      if (s < end_s - 1 && *s == '/' && s[1] == '*') {
	// slash-star comment
	*o++ = ' ';
	*o++ = ' ';
	s += 2;
	while (s < end_s && (*s != '*' || s >= end_s - 1 || s[1] != '/')) {
	  *o++ = (*s == '\n' || *s == '\r' ? *s : ' ');
	  s++;
	}
	if (s < end_s) {
	  *o++ = ' ';
	  *o++ = ' ';
	  s += 2;
	}
	
      } else if (s < end_s - 1 && *s == '/' && s[1] == '/') {
	// slash-slash comment
	*o++ = ' ';
	*o++ = ' ';
	s += 2;
	while (s < end_s && *s != '\n' && *s != '\r')
	  *o++ = ' ', s++;
	
      } else if (*s == '\"' || *s == '\'') {
	// literal
	char stopper = *s;
	*o++ = ' ', s++;
	while (s < end_s && *s != stopper) {
	  *o++ = ' ', s++;
	  if (s[-1] == '\\')
	    *o++ = '$', s++;
	}
	if (s < end_s)
	  *o++ = '$', s++;
	
      } else if (*s != '\n' && *s != '\r')
	// random other character, fine
	*o++ = *s++;
    }

    // copy EOL characters
    while (s < end_s && (*s == '\n' || *s == '\r'))
      *o++ = *s++;
  }

  return new_text.take_string();
}

static int
skip_balanced_braces(const String &text, int p)
{
  const char *s = text.data();
  int len = text.length();
  int brace_level = 0;
  while (p < len) {
    if (s[p] == '{')
      brace_level++;
    else if (s[p] == '}') {
      if (!--brace_level)
	return p + 1;
    }
    p++;
  }
  return p;
}

static int
skip_balanced_parens(const String &text, int p)
{
  const char *s = text.data();
  int len = text.length();
  int brace_level = 0;
  while (p < len) {
    if (s[p] == '(')
      brace_level++;
    else if (s[p] == ')') {
      if (!--brace_level)
	return p + 1;
    }
    p++;
  }
  return p;
}

int
CxxInfo::parse_function_definition(const String &text, int fn_start_p,
				   int paren_p, const String &original,
				   CxxClass *cxx_class)
{
  // find where we think open brace should be
  int p = skip_balanced_parens(text, paren_p);
  const char *s = text.data();
  int len = text.length();
  while (p < len && isspace(s[p]))
    p++;
  if (p < len - 5 && strncmp(s+p, "const", 5) == 0) {
    for (p += 5; p < len && isspace(s[p]); p++)
      /* nada */;
  }
  // if open brace is not there, a function declaration or something similar;
  // return
  if (p >= len || s[p] != '{')
    return p;

  // save boundaries of function body
  int open_brace_p = p;
  int close_brace_p = skip_balanced_braces(text, open_brace_p);

  // find arguments; cut space from end
  for (p = open_brace_p - 1; p >= paren_p && isspace(s[p]); p--)
    /* nada */;
  String args = original.substring(paren_p, p + 1 - paren_p);

  // find function name and class name
  for (p = paren_p - 1; p > fn_start_p && isspace(s[p]); p--)
    /* nada */;
  int end_fn_name_p = p + 1;
  while (p > fn_start_p && (isalnum(s[p]) || s[p] == '_'))
    p--;
  String fn_name = original.substring(p + 1, end_fn_name_p - (p + 1));
  String class_name;
  if (p >= fn_start_p + 2 && s[p] == ':' && s[p-1] == ':') {
    int end_class_name_p = p - 1;
    for (p -= 2; p >= fn_start_p && (isalnum(s[p]) || s[p] == '_'); p--)
      /* nada */;
    if (p > fn_start_p && s[p] == ':') // nested class fns uninteresting
      return close_brace_p;
    class_name = original.substring(p + 1, end_class_name_p - (p + 1));
  }

  // find return type; skip access control declarations, cut space from end
  while (1) {
    int access_p;
    if (p >= fn_start_p + 6 && strncmp(s+fn_start_p, "public", 6) == 0)
      access_p = fn_start_p + 6;
    else if (p >= fn_start_p + 7 && strncmp(s+fn_start_p, "private", 7) == 0)
      access_p = fn_start_p + 7;
    else if (p >= fn_start_p + 9 && strncmp(s+fn_start_p, "protected", 9) == 0)
      access_p = fn_start_p + 9;
    else
      break;
    while (access_p < p && isspace(s[access_p]))
      access_p++;
    if (access_p == p || s[access_p] != ':')
      break;
    for (access_p++; access_p < p && isspace(s[access_p]); access_p++)
      /* nada */;
    fn_start_p = access_p;
  }
  while (p >= fn_start_p && isspace(s[p]))
    p--;
  String ret_type = original.substring(fn_start_p, p + 1 - fn_start_p);

  // decide if this function/class pair is OK
  CxxClass *relevant_class;
  if (class_name)
    relevant_class = make_class(class_name);
  else
    relevant_class = cxx_class;

  // define function
  if (relevant_class) {
    int body_pos = open_brace_p + 1;
    int body_len = close_brace_p - 1 - body_pos;
    relevant_class->defun
      (CxxFunction(fn_name, !class_name, ret_type, args,
		   original.substring(body_pos, body_len),
		   text.substring(body_pos, body_len)));
  }

  // done
  return close_brace_p;
}

int
CxxInfo::parse_class_definition(const String &text, int p,
				const String &original)
{
  // find start of class name
  const char *s = text.data();
  int len = text.length();
  while (p < len && isspace(s[p]))
    p++;
  int name_start_p = p;
  while (p < len && (isalnum(s[p]) || s[p] == '_'))
    p++;
  String class_name = original.substring(name_start_p, p - name_start_p);

  // XXX superclasses!!
  
  CxxClass *cxxc = make_class(class_name);
  while (p < len && s[p] != '{')
    p++;
  return parse_class(text, p + 1, original, cxxc);
}

int
CxxInfo::parse_class(const String &text, int p, const String &original,
		     CxxClass *cxx_class)
{
  // parse clean_text
  const char *s = text.data();
  int len = text.length();
  
  while (1) {

    // find first batch
    while (p < len && isspace(s[p]))
      p++;
    int p1 = p;
    while (p < len && s[p] != ';' && s[p] != '(' && s[p] != '{' &&
	   s[p] != '}')
      p++;

    //fprintf(stderr, "   %d %c\n", p, s[p]);
    if (p >= len)
      return len;
    else if (s[p] == ';') {
      // uninteresting
      p++;
      continue;
    } else if (s[p] == '}') {
      //fprintf(stderr, "!!!!!!/\n");
      return p + 1;
    }
    else if (s[p] == '{') {
      if (p > p1 + 6 && !cxx_class
	  && (strncmp(s+p1, "class", 5) == 0
	      || strncmp(s+p1, "struct", 6) == 0)) {
	// parse class definition
	p = parse_class_definition(text, p1 + 6, original);
      } else
	p = skip_balanced_braces(text, p);
    } else if (s[p] == '(')
      p = parse_function_definition(text, p1, p, original, cxx_class);
    
  }
}

void
CxxInfo::parse_file(const String &original_text)
{
  String clean_text = remove_crap(original_text);
  parse_class(clean_text, 0, original_text, 0);
}

// Vector template instantiation
#include "vector.cc"
template class Vector<CxxFunction>;
