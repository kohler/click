// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PRETTY_HTML_HH
#define CLICK_PRETTY_HTML_HH
#include <click/string.hh>
#include <click/straccum.hh>
#include <click/hashtable.hh>
#include <stdio.h>

String html_quote_attr(const String &);
String html_quote_text(const String &);
String html_unquote(const String &);
String html_unquote(const char *x, const char *end);

const char *process_tag(const char *x, String &tag,
	HashTable<String, String> &attr, bool &ended, bool unquote_value = true);

const char *output_template_until_tag
	(const char *templ, FILE *outf, String &tag,
	 HashTable<String, String> &attrs, bool unquote = true, String *sep = 0);
const char *output_template_until_tag
	(const char *templ, StringAccum &sa, String &tag,
	 HashTable<String, String> &attrs, bool unquote = true, String *sep = 0);


inline String
html_unquote(const String &s)
{
    return html_unquote(s.data(), s.data() + s.length());
}

#endif
