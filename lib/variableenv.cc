/*
 * variableenv.{cc,hh} -- scoped configuration variables
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

static void
interpolate_string(StringAccum &output, const String &config, int pos1,
		   int pos2, String value, int quote)
{
  output << config.substring(pos1, pos2 - pos1);
  if (quote == '\"') {		// interpolate inside the quotes
    value = cp_quote(cp_unquote(value));
    if (value[0] == '\"')
      value = value.substring(1, value.length() - 2);
  }
  output << value;
}

String
VariableEnvironment::interpolate(const String &config) const
{
  if (!config || (_formals.size() == 0 && config.find_left('$') < 0))
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
      int word_pos = pos;
      
      String name, extension;
      int extension_pos = 0;
      if (pos < len - 1 && data[pos+1] == '{') {
	for (pos += 2; pos < len && data[pos] != '}'; pos++)
	  if (data[pos] == '-')
	    extension_pos = pos;
	if (!extension_pos)
	  extension_pos = pos;
	name = "$" + config.substring(word_pos + 2, extension_pos - word_pos - 2);
	extension = config.substring(extension_pos, pos - extension_pos);
	if (pos < len) pos++;
      } else {
	for (pos++; pos < len && (isalnum(data[pos]) || data[pos] == '_'); pos++)
	  /* nada */;
	name = config.substring(word_pos, pos - word_pos);
      }

      for (int variable = _formals.size() - 1; variable >= 0; variable--)
	if (name == _formals[variable]) {
	  interpolate_string(output, config, config_pos, word_pos, _values[variable], quote);
	  config_pos = pos;
	  goto found_expansion;
	}

      // no expansion if we get here
      if (extension && extension[0] == '-') {
	// interpolate default value
	interpolate_string(output, config, config_pos, word_pos, extension.substring(1), quote);
	config_pos = pos;
      }
      
     found_expansion:
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
