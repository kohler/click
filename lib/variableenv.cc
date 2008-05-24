// -*- c-basic-offset: 4; related-file-name: "../include/click/variableenv.hh" -*-
/*
 * variableenv.{cc,hh} -- scoped configuration variables
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2004-2005 Regents of the University of California
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
cp_expand(const String &config, const VariableExpander &ve, bool expand_quote)
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
	    if (s + 1 >= end || quote == '\'')
		break;

	    const char *beforedollar = s, *cstart;
	    String vname;
	    int vtype;

	    if (s[1] == '{') {
		vtype = '{';
		s += 2;
		for (cstart = s; s < end && *s != '}'; s++)
		    /* nada */;
		if (s == end)
		    goto done;
		vname = config.substring(cstart, s++);
		
	    } else if (s[1] == '(') {
		int level = 1, nquote = 0, anydollar = 0;
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
			    anydollar = 1;
			break;
		    }

		if (s == cstart || s[-1] != ')')
		    goto done;
		if (anydollar)
		    // XXX recursive call: potential stack overflow
		    vname = cp_expand(config.substring(cstart, s - 1), ve);
		else
		    vname = config.substring(cstart, s - 1);
		
	    } else if (isalnum((unsigned char) s[1]) || s[1] == '_') {
		vtype = 'a';
		s++;
		for (cstart = s; s < end && (isalnum((unsigned char) *s) || *s == '_'); s++)
		    /* nada */;
		vname = config.substring(cstart, s);

	    } else if (s[1] == '?' || s[1] == '#') {
		vtype = 'a';
		s++;
		vname = config.substring(s, s + 1);
		s++;
		
	    } else
		break;

	    output << config.substring(uninterpolated, beforedollar);

	    bool result;
	    if (expand_quote && quote == 0) {
		output << '\"';
		result = ve.expand(vname, vtype, '\"', output);
		output << '\"';
	    } else
		result = ve.expand(vname, vtype, quote, output);

	    uninterpolated = (result ? s : beforedollar);
	    s--;
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

String
cp_expand_in_quotes(const String &s, int quote)
{
    if (quote == '\"') {
	String ss = cp_quote(s);
	if (ss[0] == '\"')
	    ss = ss.substring(1, ss.length() - 2);
	return ss;
    } else
	return s;
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
    return String::empty_string();
}

bool
VariableEnvironment::expand(const String &var, int vartype, int quote,
			    StringAccum &output) const
{
    String v(var);
    const char *minus = 0;
    if (vartype == '{' && (minus = find(var, '-')) != var.end())
	v = var.substring(var.begin(), minus);
    bool found;
    const String &val = value(v, found);
    if (found) {
	output << cp_expand_in_quotes(val, quote);
	return true;
    } else if (minus) {
	output << cp_expand_in_quotes(var.substring(minus + 1, var.end()), quote);
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
