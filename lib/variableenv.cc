// -*- c-basic-offset: 4; related-file-name: "../include/click/variableenv.hh" -*-
/*
 * variableenv.{cc,hh} -- scoped configuration variables
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2004-2005 Regents of the University of California
 * Copyright (c) 2011 Meraki, Inc.
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

String
cp_expand(const String &config, const VariableExpander &ve,
	  bool expand_quote, int depth)
{
    if (!config || find(config, '$') == config.end())
	return config;

    const char *s = config.begin();
    const char *end = config.end();
    const char *uninterpolated = s;
    int quote = 0;
    StringAccum output;

    for (; s < end; s++)
	switch (*s) {

	case '\\':
	    if (s + 1 < end && quote == '\"')
		s++;
	    break;

	case '\'':
	case '\"':
	    if (quote == 0)
		quote = *s;
	    else if (quote == *s)
		quote = 0;
	    break;

	case '/':
	    if (s + 1 < end && (s[1] == '/' || s[1] == '*') && quote == 0)
		s = cp_skip_comment_space(s, end) - 1;
	    break;

	case '$': {
	    if (s + 1 >= end || quote == '\'' || depth > 10)
		break;

	    const char *beforedollar = s, *cstart;
	    String vname;
	    int vtype, expand_vname = 0;

	    if (s[1] == '{') {
		vtype = '{';
		s += 2;
		for (cstart = s; s < end && *s != '}'; s++)
		    if (*s == '$')
			expand_vname = 1;
		if (s == end)
		    goto done;
		vname = config.substring(cstart, s++);

	    } else if (s[1] == '(') {
		int level = 1, nquote = 0;
		vtype = '(';
		s += 2;
		for (cstart = s; s < end && level; s++)
		    switch (*s) {
		      case '(':
			if (nquote == 0)
			    level++;
			break;
		      case ')':
			if (nquote == 0)
			    level--;
			break;
		      case '\"':
		      case '\'':
			if (nquote == 0)
			    nquote = *s;
			else if (nquote == *s)
			    nquote = 0;
			break;
		      case '\\':
			if (s + 1 < end && nquote != '\'')
			    s++;
			break;
		      case '$':
			if (nquote != '\'')
			    expand_vname = 1;
			break;
		    }

		if (s == cstart || s[-1] != ')')
		    goto done;
		vname = config.substring(cstart, s - 1);

	    } else if (isalnum((unsigned char) s[1]) || s[1] == '_') {
		vtype = 'a';
		s++;
		for (cstart = s; s < end && (isalnum((unsigned char) *s) || *s == '_'); s++)
		    /* nada */;
		vname = config.substring(cstart, s);

	    } else if (s[1] == '?' || s[1] == '#' || s[1] == '$') {
		vtype = 'a';
		s++;
		vname = config.substring(s, s + 1);
		s++;

	    } else
		break;

	    output << config.substring(uninterpolated, beforedollar);

	    if (expand_vname)
		vname = cp_expand(vname, ve, false, depth + 1);
	    String text;
	    bool result = ve.expand(vname, text, vtype, depth);

	    uninterpolated = s;
	    if (!result) {
		if (expand_vname)
		    output << beforedollar[0] << beforedollar[1] << vname << s[-1];
		else
		    uninterpolated = beforedollar;
	    } else if (quote == '\"' || (expand_quote && quote == 0)) {
		text = cp_quote(text);
		if (text[0] == '\"')
		    text = text.substring(text.begin() + 1, text.end() - 1);
		if (quote == '\"')
		    output << text;
		else
		    output << '\"' << text << '\"';
	    } else
		output << text;

	    s--;
	    break;
	}
	}

  done:
    if (!output.length())
	return config;
    else {
	output << config.substring(uninterpolated, s);
	return output.take_string();
    }
}

VariableEnvironment::VariableEnvironment(VariableEnvironment *parent)
    : _depth(parent ? parent->_depth + 1 : 0), _parent(parent)
{
}

VariableEnvironment *
VariableEnvironment::parent_of(int depth) const
{
    VariableEnvironment *v = const_cast<VariableEnvironment *>(this);
    while (v && v->_depth >= depth)
	v = v->_parent;
    return v;
}

bool
VariableEnvironment::defines(const String &name) const
{
    for (const String *s = _names.begin(); s != _names.end(); ++s)
	if (*s == name)
	    return true;
    return false;
}

bool
VariableEnvironment::define(const String &name, const String &value, bool override)
{
    for (String *s = _names.begin(); s != _names.end(); s++)
	if (*s == name) {
	    if (override)
		_values[s - _names.begin()] = value;
	    return false;
	}
    _names.push_back(name);
    _values.push_back(value);
    return true;
}

const String &
VariableEnvironment::value(const String &formal, bool &found) const
{
    const VariableEnvironment *v = this;
    while (v) {
	for (int i = 0; i < v->_names.size(); i++)
	    if (v->_names[i] == formal) {
		found = true;
		return v->_values[i];
	    }
	v = v->_parent;
    }
    found = false;
    return String::make_empty();
}

int
VariableEnvironment::expand(const String &var, String &expansion,
			    int vartype, int depth) const
{
    String v(var);
    const char *minus = 0;
    int space_index = -1;

    if (vartype == '{') {
	const char *end = var.end();
	const char *s = cp_skip_space(var.begin(), end);
	const char *namebegin = s;
	while (s < end && (isalnum((unsigned char) *s) || *s == '_'))
	    ++s;
	const char *nameend = s;
	s = cp_skip_space(s, end);
	if (s < end && *s == '[') {
	    const char *nstart = cp_skip_space(s + 1, end);
	    s = cp_integer(nstart, end, 0, &space_index);
	    if (s > nstart && s < end && *s == ']')
		s = cp_skip_space(s + 1, end);
	    else
		space_index = -1;
	}
	if (s < end && *s == '-')
	    minus = s;
	v = var.substring(namebegin, nameend);
    }

    bool found;
    const String &val = value(v, found);

    if (found) {
	String value = cp_expand(val, *this, false, depth + 1);
	if (space_index >= 0) {
	    String word;
	    for (; space_index >= 0 && (value || word); --space_index)
		word = cp_shift_spacevec(value);
	    expansion = word;
	} else
	    expansion = value;
	return true;
    } else if (minus) {
	expansion = cp_expand(var.substring(minus + 1, var.end()), *this, false, depth + 1);
	return true;
    } else
	return false;
}

#if 0
void
VariableEnvironment::print() const
{
    for (int i = 0; i < _names.size(); i++)
	fprintf(stderr, "%s.%d=%s ", _names[i].c_str(), _depths[i], _values[i].c_str());
    fprintf(stderr, "\n");
}
#endif

CLICK_ENDDECLS
