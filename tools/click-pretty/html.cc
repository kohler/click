// -*- c-basic-offset: 4 -*-
/*
 * html.cc -- HTML handling code for click-pretty
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
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
#include <click/pathvars.h>

#include "html.hh"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static HashTable<String, String> html_entities;


// quoting

String
html_quote_attr(const String &what)
{
    StringAccum sa;
    int pos = 0;
    while (1) {
	int npos = pos;
	while (npos < what.length() && what[npos] != '&'
	       && what[npos] != '\'' && what[npos] != '\"')
	    npos++;
	if (npos >= what.length())
	    break;
	sa << what.substring(pos, npos - pos);
	if (what[npos] == '&')
	    sa << "&#38;";
	else if (what[npos] == '\'')
	    sa << "&#39;";
	else /* what[npos] == '\"' */
	    sa << "&#34;";
	pos = npos + 1;
    }
    if (pos == 0)
	return what;
    else {
	sa << what.substring(pos);
	return sa.take_string();
    }
}

String
html_quote_text(const String &what)
{
    StringAccum sa;
    int pos = 0;
    while (1) {
	int npos = pos;
	while (npos < what.length() && what[npos] != '&'
	       && what[npos] != '<' && what[npos] != '>')
	    npos++;
	if (npos >= what.length())
	    break;
	sa << what.substring(pos, npos - pos);
	if (what[npos] == '&')
	    sa << "&amp;";
	else if (what[npos] == '<')
	    sa << "&lt;";
	else /* what[npos] == '>' */
	    sa << "&gt;";
	pos = npos + 1;
    }
    if (pos == 0)
	return what;
    else {
	sa << what.substring(pos);
	return sa.take_string();
    }
}

String
html_unquote(const char *x, const char *end)
{
    if (!html_entities.get("&amp")) {
	html_entities.set("&amp", "&");
	html_entities.set("&quot", "\"");
	html_entities.set("&lt", "<");
	html_entities.set("&gt", ">");
    }

    StringAccum sa;
    while (x < end) {
	if (*x == '&') {
	    if (x < end - 2 && x[1] == '#') {
		int val = 0;
		for (x += 2; x < end && isdigit((unsigned char) *x); x++)
		    val = (val * 10) + *x - '0';
		sa << (char)val;
		if (x < end && *x == ';')
		    x++;
	    } else {
		const char *start = x;
		for (x++; x < end && isalnum((unsigned char) *x); x++)
		    /* nada */;
		String entity_name = String(start, x - start);
		String entity_value = html_entities.get(entity_name);
		sa << (entity_value ? entity_value : entity_name);
		if (x < end && *x == ';' && entity_value)
		    x++;
	    }
	} else
	    sa << *x++;
    }
    return sa.take_string();
}


// tag processing

const char *
process_tag(const char *x, String &tag, HashTable<String, String> &attrs,
	    bool &ended, bool unquote_value)
{
    // process tag
    while (isspace((unsigned char) *x))
	x++;
    const char *tag_start = x;
    while (*x && *x != '>' && !isspace((unsigned char) *x))
	x++;
    tag = String(tag_start, x - tag_start).lower();
    ended = false;

    // process attributes
    while (1) {
	while (isspace((unsigned char) *x))
	    x++;
	if (*x == 0)
	    return x;
	else if (*x == '>')
	    return x + 1;
	else if (x[0] == '-' && x[1] == '-' && x[2] == '>')
	    return x + 3;
	else if (*x == '/') {
	    ended = true;
	    x++;
	    continue;
	}

	// calculate attribute start
	const char *attr_start = x;
	while (*x && *x != '>' && !isspace((unsigned char) *x) && *x != '=')
	    x++;
	String attr_name = html_unquote(attr_start, x).lower();

	// look for '=' if any
	while (isspace((unsigned char) *x))
	    x++;
	if (*x != '=') {
	    attrs.set(attr_name, attr_name);
	    continue;
	}

	// attribute value
	for (x++; isspace((unsigned char) *x); x++)
	    /* nada */;
	const char *value_start;
	bool bump;

	if (*x == '\'') {
	    value_start = x + 1;
	    for (x++; *x && *x != '\''; x++)
		/* nada */;
	    bump = true;
	} else if (*x == '\"') {
	    value_start = x + 1;
	    for (x++; *x && *x != '\"'; x++)
		/* nada */;
	    bump = true;
	} else {
	    value_start = x;
	    for (; *x && !isspace((unsigned char) *x) && *x != '>'; x++)
		/* nada */;
	    bump = false;
	}

	if (unquote_value)
	    attrs.set(attr_name, html_unquote(value_start, x));
	else
	    attrs.set(attr_name, String(value_start, x - value_start));

	if (bump && *x)
	    x++;
    }
}

const char *
output_template_until_tag(const char *templ, StringAccum &sa,
			  String &tag, HashTable<String, String> &attrs,
			  bool unquote, String *sep)
{
    // skip to next directive
    tag = String();
    attrs.clear();
    bool ended;

    const char *x = templ;
    while (*x) {
	if (x[0] == '<' && x[1] == '~') {
	    if (sep && x > templ) {
		sa << *sep;
		*sep = String();
	    }
	    sa.append(templ, x - templ);
	    return process_tag(x + 2, tag, attrs, ended, unquote);
	} else
	    x++;
    }

    if (sep && x > templ) {
	sa << *sep;
	*sep = String();
    }
    sa.append(templ, x - templ);
    return 0;
}

const char *
output_template_until_tag(const char *templ, FILE *outf,
			  String &tag, HashTable<String, String> &attrs,
			  bool unquote, String *sep)
{
    StringAccum sa;
    templ = output_template_until_tag(templ, sa, tag, attrs, unquote, sep);
    fputs(sa.c_str(), outf);
    return templ;
}
