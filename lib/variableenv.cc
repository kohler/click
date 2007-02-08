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
cp_expand(const String &config, VariableExpander &ve, bool expand_quote)
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
		
	    } else if (isalnum(s[1]) || s[1] == '_') {
		vtype = 'a';
		s++;
		for (cstart = s; s < end && (isalnum(*s) || *s == '_'); s++)
		    /* nada */;
		vname = config.substring(cstart, s);

	    } else if (s[1] == '?') {
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

VariableEnvironment::VariableEnvironment(const VariableEnvironment &ve, int depth)
    : VariableExpander()
{
    for (int i = 0; i < ve._formals.size() && ve._depths[i] < depth; i++) {
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

bool
VariableEnvironment::expand(const String &var, int vartype, int quote,
			    StringAccum &output)
{
    String v(var);
    const char *minus = 0;
    if (vartype == '{' && (minus = find(var, '-')) != var.end())
	v = var.substring(var.begin(), minus);
    for (int vnum = _formals.size() - 1; vnum >= 0; vnum--)
	if (v == _formals[vnum]) {
	    output << cp_expand_in_quotes(_values[vnum], quote);
	    return true;
	}
    if (minus) {
	output << cp_expand_in_quotes(var.substring(minus + 1, var.end()), quote);
	return true;
    } else
	return false;
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
