/*
 * variableenv.{cc,hh} -- scoped configuration variables
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/variableenv.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>

VariableEnvironment::VariableEnvironment(const String &prefix)
  : _prefix(prefix)
{
  int pl = _prefix.length();
  if (pl > 0 && _prefix[pl - 1] != '/')
    _prefix += '/';
}

VariableEnvironment::VariableEnvironment(const VariableEnvironment &o, const String &suffix)
  : _prefix(o._prefix + suffix), _formals(o._formals), _values(o._values), _depths(o._depths)
{
  int pl = _prefix.length();
  if (pl > 0 && _prefix[pl - 1] != '/')
    _prefix += '/';
}

void
VariableEnvironment::enter(const VariableEnvironment &ve)
{
  assert(_depths.size() == 0 || ve._depths.size() == 0 || _depths.back() < ve._depths[0]);
  for (int i = 0; i < ve._formals.size(); i++) {
    _formals.push_back(ve._formals[i]);
    _values.push_back(ve._values[i]);
    _depths.push_back(ve._depths[i]);
  }
}

void
VariableEnvironment::enter(const Vector<String> &formals, const Vector<String> &values, int depth)
{
  assert(_depths.size() == 0 || depth > _depths.back());
  for (int i = 0; i < formals.size(); i++) {
    _formals.push_back(formals[i]);
    _values.push_back(values[i]);
    _depths.push_back(depth);
  }
}

void
VariableEnvironment::limit_depth(int deepest)
{
  int s = _formals.size();
  while (s > 0 && _depths[s-1] >= deepest)
    s--;
  _formals.resize(s);
  _values.resize(s);
  _depths.resize(s);
}

String
VariableEnvironment::interpolate(const String &config) const
{
  if (_formals.size() == 0 || !config)
    return config;
  
  const char *data = config.data();
  int config_pos = 0;
  int pos = 0;
  int len = config.length();
  int quote = 0;
  StringAccum output;
  
  for (; pos < len; pos++)
    if (data[pos] == '\\' && pos < len - 1 && quote == '\"')
      pos++;
    else if (data[pos] == '\'' && quote == 0)
      quote = '\'';
    else if (data[pos] == '\"' && quote == 0)
      quote = '\"';
    else if (data[pos] == quote)
      quote = 0;
    else if (data[pos] == '/' && pos < len - 1) {
      if (data[pos+1] == '/') {
	for (pos += 2; pos < len && data[pos] != '\n' && data[pos] != '\r'; )
	  pos++;
      } else if (data[pos+1] == '*') {
	for (pos += 2; pos < len; pos++)
	  if (data[pos] == '*' && pos < len - 1 && data[pos+1] == '/') {
	    pos++;
	    break;
	  }
      }
    } else if (data[pos] == '$' && quote != '\'') {
      unsigned word_pos = pos;
      for (pos++; isalnum(data[pos]) || data[pos] == '_'; pos++)
	/* nada */;
      String name = config.substring(word_pos, pos - word_pos);
      for (int variable = _formals.size() - 1; variable >= 0; variable--)
	if (name == _formals[variable]) {
	  output << config.substring(config_pos, word_pos - config_pos);
	  String value = _values[variable];
	  if (quote == '\"') {	// interpolate inside the quotes
	    value = cp_quote(cp_unquote(value));
	    if (value[0] == '\"')
	      value = value.substring(1, value.length() - 2);
	  }
	  output << value;
	  config_pos = pos;
	  break;
	}
      pos--;
    }

  if (!output.length())
    return config;
  else {
    output << config.substring(config_pos, pos - config_pos);
    return output.take_string();
  }
}

#if 0
void
VariableEnvironment::print() const
{
  for (int i = 0; i < _formals.size(); i++)
    fprintf(stderr, "%s.%d=%s ", String(_formals[i]).cc(), _depths[i], String(_values[i]).cc());
  fprintf(stderr, "\n");
}
#endif
