// -*- c-basic-offset: 4; related-file-name: "../include/click/variableenv.hh" -*-
/*
 * variableenv.{cc,hh} -- scoped configuration variables
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include <click/variableenv.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
CLICK_DECLS

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
VariableEnvironment::enter(const Vector<String> &formals, const Vector<String> &values, int enter_depth)
{
    assert(enter_depth > depth());
    for (int arg = 0; arg < formals.size(); arg++) {
	_formals.push_back(formals[arg]);
	_values.push_back(values[arg]);
	_depths.push_back(enter_depth);
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

static const char *
interpolate_string(StringAccum &output, const String &config,
		   const char *uninterpolated, const char *word,
		   const char *s, String value, int quote)
{
    output << config.substring(uninterpolated, word);
    if (quote == '\"') {	// interpolate inside the quotes
	value = cp_quote(cp_unquote(value));
	if (value[0] == '\"')
	    value = value.substring(1, value.length() - 2);
    }
    output << value;
    return s;
}

String
VariableEnvironment::interpolate(const String &config) const
{
    if (!config || (_formals.size() == 0 && find(config, '$') == config.end()))
	return config;
  
    const char *s = config.begin();
    const char *end = config.end();
    const char *uninterpolated = s;
    int quote = 0;
    StringAccum output;
    
    for (; s < end; s++)
	if (*s == '\\' && s + 1 < end && quote == '\"')
	    s++;
	else if (*s == '\'' && quote == 0)
	    quote = '\'';
	else if (*s == '\"' && quote == 0)
	    quote = '\"';
	else if (*s == quote)
	    quote = 0;
	else if (*s == '/' && s + 1 < end && (s[1] == '/' || s[1] == '*') && quote == 0)
	    s = cp_skip_comment_space(s, end) - 1;
	else if (*s == '$' && quote != '\'') {
	    const char *word = s;
      
	    String name, extension;
	    if (s + 1 < end && s[1] == '{') {
		const char *extension_ptr = 0;
		for (s += 2; s < end && *s != '}'; s++)
		    if (*s == '-' && !extension_ptr)
			extension_ptr = s;
		if (!extension_ptr)
		    extension_ptr = s;
		name = "$" + config.substring(word + 2, extension_ptr);
		extension = config.substring(extension_ptr, s);
		if (s < end)
		    s++;
	    } else {
		for (s++; s < end && (isalnum(*s) || *s == '_'); s++)
		    /* nada */;
		name = config.substring(word, s);
	    }

	    for (int variable = _formals.size() - 1; variable >= 0; variable--)
		if (name == _formals[variable]) {
		    uninterpolated = s = interpolate_string(output, config, uninterpolated, word, s, _values[variable], quote);
		    goto found_expansion;
		}

	    // no expansion if we get here
	    if (extension && extension[0] == '-')
		// interpolate default value
		uninterpolated = s = interpolate_string(output, config, uninterpolated, word, s, extension.substring(1), quote);
      
	  found_expansion:
	    s--;
	}

    if (!output.length())
	return config;
    else {
	output << config.substring(uninterpolated, s);
	return output.take_string();
    }
}

#if 0
void
VariableEnvironment::print() const
{
    for (int i = 0; i < _formals.size(); i++)
	fprintf(stderr, "%s.%d=%s ", _formals[i].c_str(), _depths[i], _values[i].c_str());
    fprintf(stderr, "\n");
}
#endif

CLICK_ENDDECLS
