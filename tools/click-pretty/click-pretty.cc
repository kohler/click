// -*- c-basic-offset: 4 -*-
/*
 * click-pretty.cc -- pretty-print Click configurations
 * Eddie Kohler
 *
 * Copyright (c) 2001-2002 International Computer Science Institute
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

#include "routert.hh"
#include "lexert.hh"
#include "lexertinfo.hh"
#include <click/error.hh>
#include <click/driver.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include "processingt.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define OUTPUT_OPT		304
#define CLASS_URLS_OPT		305
#define TEMPLATE_OPT		306

static Clp_Option options[] = {
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
    { "class-urls", 'u', CLASS_URLS_OPT, Clp_ArgString, 0 },
    { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
    { "help", 0, HELP_OPT, 0, 0 },
    { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
    { "template", 't', TEMPLATE_OPT, Clp_ArgString, 0 },
    { "version", 'v', VERSION_OPT, 0, 0 },
};

static String::Initializer string_initializer;
static const char *program_name;

static const char *default_template = "\
<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n\
<html><head>\n\
<meta http-equiv='Content-Type' content='text/html; charset=ISO-8859-1'>\n\
<meta http-equiv='Content-Style-Type' content='text/css'>\n\
<style type='text/css'><!--\n\
SPAN.c-kw {\n\
  font-weight: bold;\n\
}\n\
SPAN.c-cdecl {\n\
  font-style: italic;\n\
}\n\
SPAN.c-cstr {\n\
  color: green;\n\
}\n\
SPAN.c-cmt {\n\
  color: gray;\n\
}\n\
SPAN.c-err {\n\
  color: red;\n\
  font-weight: bold;\n\
}\n\
SPAN.c-err:hover {\n\
  color: black;\n\
  font-weight: bold;\n\
  background-color: #000;\n\
}\n\
SPAN.c-et-decl {\n\
  font-weight: bold;\n\
  background: yellow;\n\
  foreground: black;\n\
}\n\
SPAN.c-et-port {\n\
  font-style: italic;\n\
  font-size: small;\n\
}\n\
SPAN.c-et-conn {\n\
  font-size: small;\n\
}\n\
P.c-ei, P.c-eit {\n\
  margin-top: 0px;\n\
  margin-bottom: 0px;\n\
  text-indent: -2em;\n\
  margin-left: 2em;\n\
}\n\
P.c-eite {\n\
  margin-top: 0px;\n\
  margin-bottom: 0px;\n\
  margin-left: 3em;\n\
}\n\
SPAN.c-eih {\n\
  font-weight: bold;\n\
}\n\
SPAN.c-ei-class {\n\
}\n\
--></style>\n\
<body>\n\
<h1>Configuration</h1>\n\
<!-- click-pretty:config /-->\n\
<h1>Element index</h1>\n\
<table cellspacing='0' cellpadding='0' border='0'>\n\
<tr valign='top'>\n\
<td><!-- click-pretty:eindex types configlink=' (<i>config</i>, ' etablelink='<i>table</i>)' column='1/2' /--></td>\n\
<td width='15'>&nbsp;</td>\n\
<td><!-- click-pretty:eindex types configlink=' (<i>config</i>, ' etablelink='<i>table</i>)' column='2/2' /--></td>\n\
</tr>\n\
</table>\n\
<h1>Element tables</h1>\n\
<!-- click-pretty:etables configlink=' (<i>config</i>)' /-->\n\
</body>\n\
</html>\n";


// handle output items

struct OutputItem {
    int pos;
    String text;
    int _other;
    bool active : 1;
    bool _end_item : 1;
    OutputItem() : pos(-1), _other(-1), active(0), _end_item(0) { }
    OutputItem(int p, const String &t, bool ei) : pos(p), text(t), _other(-1), active(0), _end_item(ei) { }
    bool end_item() const		{ return _end_item; }
    OutputItem *other() const;
    int other_index() const		{ return _other; }
    int item_index() const;
    int end_item_index() const;
    void activate(bool a);
};

static Vector<OutputItem> items;
static Vector<OutputItem> end_items;

inline OutputItem *
OutputItem::other() const
{
    if (_other < 0)
	return 0;
    else if (_end_item)
	return &items[_other];
    else
	return &end_items[_other];
}

inline int
OutputItem::item_index() const
{
    if (_end_item)
	return other_index();
    else
	return other()->other_index();
}

inline int
OutputItem::end_item_index() const
{
    if (_end_item)
	return other()->other_index();
    else
	return other_index();
}

inline void
OutputItem::activate(bool a)
{
    active = other()->active = a;
}

static void
add_item(int p1, const String &t1, int p2, const String &t2)
{
    items.push_back(OutputItem(p1, t1, false));
    end_items.push_back(OutputItem(p2, t2, true));
    items.back()._other = end_items.back()._other = items.size() - 1;
}

extern "C" {
static OutputItem *compar_items;
static int
item_compar(const void *v1, const void *v2)
{
    const OutputItem &oi1 = compar_items[*((const int *)v1)];
    const OutputItem &oi2 = compar_items[*((const int *)v2)];
    int diff = oi1.pos - oi2.pos;
    if (diff != 0)
	return diff;
    else if (oi1.end_item())
	// Sort end items in reverse order from corresponding start items.
	return oi2.other_index() - oi1.other_index();
    else
	return oi2.other()->pos - oi1.other()->pos;
}
}

static void
prepare_items(int last_pos)
{
    add_item(last_pos + 1, "", last_pos + 1, "");
    assert(items.size() == end_items.size());

    // sort items
    for (int which = 0; which < 2; which++) {
	compar_items = (which == 0 ? &items[0] : &end_items[0]);

	Vector<int> permute;
	for (int i = 0; i < items.size(); i++)
	    permute.push_back(i);
	qsort(&permute[0], items.size(), sizeof(int), item_compar);

	Vector<int> rev_permute(items.size(), -1);
	for (int i = 0; i < items.size(); i++)
	    rev_permute[permute[i]] = i;
	
	OutputItem *other_items = (which == 0 ? &end_items[0] : &items[0]);
	for (int i = 0; i < items.size(); i++)
	    other_items[i]._other = rev_permute[other_items[i]._other];

	Vector<OutputItem> new_items(which == 0 ? items : end_items);
	for (int i = 0; i < items.size(); i++)
	    compar_items[i] = new_items[permute[i]];
    }

    // process items, combine those that need to be combined
    for (int i = 0; i < items.size() - 1; i++) {
	OutputItem *s1 = &items[i], *e1 = s1->other();
	OutputItem *s2 = &items[i+1], *e2 = s2->other();
	if (s1->pos != s2->pos || e1->pos != e2->pos)
	    continue;
	if (s1->text == s2->text && e1->text == e2->text)
	    s2->text = e2->text = "";
	else if (s1->text.substring(0, 3) == "<a "
		 && s2->text.substring(0, 3) == "<a ") {
	    s1->text = s1->text.substring(0, s1->text.length() - 1)
		+ " " + s2->text.substring(3);
	    s2->text = e2->text = "";
	}
    }
}


static HashMap<int, String> class_hrefs;
static String default_class_href;

static void
add_class_href(int class_uid, const String &href)
{
    class_hrefs.insert(class_uid, href);
}

static String
class_href(ElementClassT *ec)
{
    String *sp = class_hrefs.findp(ec->unique_id());
    if (sp)
	return *sp;
    else {
	String href = percent_substitute(default_class_href, 's', ec->name_cc(), 0);
	add_class_href(ec->unique_id(), href);
	return href;
    }
}


static String
url_attr_quote(const String &what)
{
    StringAccum sa;
    int pos = 0;
    while (1) {
	int npos = pos;
	while (npos < what.length() && what[npos] != '&' && what[npos] != '\'')
	    npos++;
	if (npos >= what.length())
	    break;
	sa << what.substring(pos, npos - pos);
	if (what[npos] == '&')
	    sa << "&#38;";
	else /* what[npos] == '\'' */
	    sa << "&#39;";
	pos = npos + 1;
    }
    if (pos == 0)
	return what;
    else {
	sa << what.substring(pos);
	return sa.take_string();
    }
}

static String
link_class_decl(ElementClassT *ec)
{
    return "decl" + String(ec->unique_id());
}

static String
link_element_decl(ElementT *e, ElementClassT *enclose)
{
    if (enclose)
	return "e" + String(enclose->unique_id()) + "-" + e->name();
    else
	return "e-" + e->name();
}

static String
link_element_table(ElementT *e, ElementClassT *enclose)
{
    if (enclose)
	return "et" + String(enclose->unique_id()) + "-" + e->name();
    else
	return "et-" + e->name();
}

class PrettyLexerTInfo : public LexerTInfo { public:

    PrettyLexerTInfo()				{ }
  
    void notify_comment(int pos1, int pos2) {
	add_item(pos1, "<span class='c-cmt'>", pos2, "</span>");
    }
    void notify_error(const String &what, int pos1, int pos2) {
	add_item(pos1, "<span class='c-err' title='" + url_attr_quote(what) + "'>", pos2, "</span>");
    }
    void notify_keyword(const String &, int pos1, int pos2) {
	add_item(pos1, "<span class='c-kw'>", pos2, "</span>");
    }
    void notify_config_string(int pos1, int pos2) {
	add_item(pos1, "<span class='c-cstr'>", pos2, "</span>");
    }
    void notify_class_declaration(ElementClassT *ec, bool anonymous, int decl_pos1, int name_pos1, int) {
	if (!anonymous)
	    add_item(name_pos1, "<a name='" + link_class_decl(ec) + "'><span class='c-cdecl'>", name_pos1 + ec->name().length(), "</span></a>");
	else
	    add_item(decl_pos1, "<a name='" + link_class_decl(ec) + "'>", decl_pos1 + 1, "</a>");
	add_class_href(ec->unique_id(), "#" + link_class_decl(ec));
    }
    void notify_class_extension(ElementClassT *ec, int pos1, int pos2) {
	String href = class_href(ec);
	if (href)
	    add_item(pos1, "<a href='" + href + "'>", pos2, "</a>");
    }
    void notify_class_reference(ElementClassT *ec, int pos1, int pos2) {
	String href = class_href(ec);
	if (href)
	    add_item(pos1, "<a href='" + href + "'>", pos2, "</a>");
    }
    void notify_element_declaration(ElementT *e, ElementClassT *enclose, int pos1, int pos2, int decl_pos2) {
	add_item(pos1, "<a name='" + link_element_decl(e, enclose) + "'>", pos2, "</a>");
	notify_element_reference(e, enclose, pos1, decl_pos2);
    }
    void notify_element_reference(ElementT *e, ElementClassT *, int pos1, int pos2) {
	add_item(pos1, "<span title='" + e->name() + " :: " + e->type_name() + "'>", pos2, "</span>");
    }

};


void
short_usage()
{
    fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try `%s --help' for more information.\n",
	    program_name, program_name);
}

void
usage()
{
    printf("\
`C-pretty' checks a Click router configuration for correctness and reports\n\
any error messages to standard error.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE           Read router configuration from FILE.\n\
  -o, --output FILE         If valid, write configuration to FILE.\n\
  -C, --clickpath PATH      Use PATH for CLICKPATH.\n\
      --help                Print this message and exit.\n\
  -v, --version             Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

static RouterT *
pretty_read_router(const char *filename, ErrorHandler *errh, String &config)
{
    // This function is a paraphrase of read_router_file.
  
    // read file string
    int before_nerrors = errh->nerrors();
  
    config = file_string(filename, errh);
    if (!config && errh->nerrors() != before_nerrors)
	return 0;

    // set readable filename
    if (!filename || strcmp(filename, "-") == 0)
	filename = "<stdin>";

    // check for archive
    Vector<ArchiveElement> archive;
    if (config.length() && config[0] == '!') {
	separate_ar_string(config, archive, errh);
	int found = -1;
	for (int i = 0; i < archive.size(); i++)
	    if (archive[i].name == "config")
		found = i;
	if (found >= 0)
	    config = archive[found].data;
	else {
	    errh->error("%s: archive has no `config' section", filename);
	    config = String();
	}
    }

    // clear list of items
    items.clear();
  
    // read router
    if (!config.length())
	errh->warning("%s: empty configuration", filename);
    LexerT lexer(ErrorHandler::silent_handler());
    PrettyLexerTInfo pinfo;
    lexer.reset(config, filename);
    lexer.set_lexinfo(&pinfo);

    // add archive bits first
    if (lexer.router() && archive.size()) {
	for (int i = 0; i < archive.size(); i++)
	    if (archive[i].live() && archive[i].name != "config")
		lexer.router()->add_archive(archive[i]);
    }

    // read statements
    while (lexer.ystatement())
	/* nada */;

    // done
    return lexer.take_router();
}


static void
activate(OutputItem &item, int &first_active)
{
    item.activate(true);
    int iitem = item.item_index();
    if (iitem < first_active)
	first_active = iitem;
}

static void
deactivate(OutputItem &item, int &first_active, int ipos)
{
    item.activate(false);
    int iitem = item.item_index();
    if (iitem == first_active) {
	for (first_active++;
	     first_active < ipos && !items[first_active].active;
	     first_active++)
	    /* nada */;
	if (first_active >= ipos)
	    first_active = items.size();
    }
}

static void
output_config(String r_config, FILE *outf)
{
    // create two sorted lists of objects
    // add sentinel item, sort item lists
    prepare_items(r_config.length());

    // loop over characters
    const char *data = r_config.cc();
    int len = r_config.length();
    int ipos = 0, eipos = 0;
    int first_active = items.size();

    fputs("<pre>", outf);
    for (int pos = 0; pos < len; pos++) {
	while (items[ipos].pos <= pos || end_items[eipos].pos <= pos)
	    if (end_items[eipos].pos <= items[ipos].pos) {
		if (end_items[eipos].active)
		    fputs(end_items[eipos].text.cc(), outf);
		deactivate(end_items[eipos], first_active, ipos);
		eipos++;
	    } else {
		fputs(items[ipos].text.cc(), outf);
		activate(items[ipos], first_active);
		ipos++;
	    }

	switch (data[pos]) {

	  case '\n': case '\r':
	    for (int i = ipos - 1; i >= first_active; i--)
		if (items[i].active)
		    fputs(items[i].other()->text.cc(), outf);
	    fputc('\n', outf);
	    if (data[pos] == '\r' && pos < len - 1 && data[pos+1] == '\n')
		pos++;
	    for (int i = first_active; i < ipos; i++)
		if (items[i].active) {
		    if (items[i].other()->pos <= pos + 1)
			items[i].activate(false);
		    else
			fputs(items[i].text.cc(), outf);
		}
	    break;

	  case '<':
	    fputs("&lt;", outf);
	    break;

	  case '>':
	    fputs("&gt;", outf);
	    break;

	  case '&':
	    fputs("&amp;", outf);
	    break;

	  default:
	    fputc(data[pos], outf);
	    break;

	}
    }
    fputs("</pre>\n", outf);
}


static bool
parse_columns(const String &s, int &which, int &count)
{
    which = count = 1; 
    int slash = s.find_left('/');
    if (slash < 0)
	return false;
    if (!cp_integer(s.substring(0, slash), &which)
	|| !cp_integer(s.substring(slash + 1), &count)
	|| which <= 0 || which > count) {
	which = count = 1;
	return false;
    } else
	return true;
}


extern "C" {
static const Vector<ConnectionT> *conn_compar_connvec;
static bool conn_compar_from;
static int
conn_compar(const void *v1, const void *v2)
{
    const int *i1 = (const int *)v1, *i2 = (const int *)v2;
    const ConnectionT &c1 = (*conn_compar_connvec)[*i1];
    const ConnectionT &c2 = (*conn_compar_connvec)[*i2];
    const PortT &p1 = (conn_compar_from ? c1.from() : c1.to());
    const PortT &p2 = (conn_compar_from ? c2.from() : c2.to());
    if (p1.elt == p2.elt)
	return p1.port - p2.port;
    else
	return click_strcmp(p1.elt->name(), p2.elt->name());
}

static int
element_name_compar(const void *v1, const void *v2)
{
    const ElementT **e1 = (const ElementT **)v1, **e2 = (const ElementT **)v2;
    return click_strcmp((*e1)->name(), (*e2)->name());
}
}


static void
link_around(StringAccum &sa, const String &href, const String &text)
{
    if (!text)
	return;
    
    int offset = 0;
    if (isspace(text[0]) || text[0] == ',' || text[0] == '(') {
	offset = 1;
	while (offset < text.length() && (isspace(text[offset]) || text[offset] == '('))
	    offset++;
	if (offset >= text.length())
	    offset = 0;
    } else if (text.substring(0, 6) == "&nbsp;" && text.length() > 6)
	offset = 6;

    int back_offset = text.length();
    while (back_offset > offset + 1
	   && (text[back_offset-1] == ')' || text[back_offset-1] == ','
	       || isspace(text[back_offset-1])))
	back_offset--;

    sa << text.substring(0, offset) << "<a href='" << href << "'>"
       << text.substring(offset, back_offset - offset) << "</a>"
       << text.substring(back_offset);
}

static void
output_element_table(ElementT *e, RouterT *r, FILE *outf,
		     const String &config_link)
{
    StringAccum sa;

    // +---+-------------------+
    // | name :: declaration   |
    // +---+-------------------+
    // |   |+----+-+--+-------+|
    // |sp.||port|#|->| stuff ||
    // |   |+....+.+..+.......+|
    // +---+-------------------+
    
    // table boilerplate
    sa << "<table cellspacing='0' cellpadding='1' border='0'>\n";
    
    // output declaration and link
    sa << "<tr valign='top'><td colspan='2' align='left'><span class='c-et-decl'>"
       << "<a name='" + link_element_table(e, 0 /* XXX */) + "'>"
       << e->name() << "</a> :: ";
    String h = class_href(e->type());
    if (h)
	sa << "<a href='" << h << "'>" << e->type_name() << "</a>";
    else
	sa << e->type_name();
    sa << "</span>";
    if (config_link)
	link_around(sa, "#" + link_element_decl(e, 0 /* XXX */), config_link);
    sa << "<br></td></tr>\n"
       << "<tr valign='top'><td width='15'>&nbsp;</td>\n"
       << "<td>";

    // output inputs
    Vector<int> conn;
    conn_compar_connvec = &r->connections();
    conn_compar_from = true;
    sa << "<table cellspacing='0' cellpadding='0' border='0'>\n";
    for (int i = 0; i < e->ninputs(); i++) {
	r->find_connections_to(PortT(e, i), conn);
	sa << "<tr valign='top'>"
	   << "<td align='left'><span class='c-et-port'>input&nbsp;&nbsp;</span></td>\n"
	   << "<td align='right'><span class='c-et-port'>" << i << "</span></td>\n";
	if (conn.size()) {
	    sa << "<td align='center'><span class='c-et-conn'>&nbsp;&lt;-&nbsp;</span></td>\n<td align='left'><span class='c-et-conn'>";
	    qsort(&conn[0], conn.size(), sizeof(int), conn_compar);
	} else
	    sa << "<td align='left' colspan='2'><span class='c-et-conn'>&nbsp;not connected";
	for (int j = 0; j < conn.size(); j++) {
	    const ConnectionT &c = r->connection(conn[j]);
	    if (j)
		sa << ", ";
	    sa << "<a href='#" << link_element_table(c.from_elt(), 0)
	       << "'>" << c.from_elt()->name() << "</a>["
	       << c.from_port() << "]";
	}
	sa << "<br></span></td></tr>\n";
    }
    if (e->ninputs() == 0)
	sa << "<tr valign='top'>"
	   << "<td colspan='4'><span class='c-et-port'>no inputs<br></span></td></tr>\n";

    // outputs
    conn_compar_from = false;
    for (int i = 0; i < e->noutputs(); i++) {
	r->find_connections_from(PortT(e, i), conn);
	sa << "<tr valign='top'>"
	   << "<td align='left'><span class='c-et-port'>output&nbsp;</span></td>\n"
	   << "<td align='right'><span class='c-et-port'>" << i << "</span></td>\n";
	if (conn.size()) {
	    sa << "<td align='center'><span class='c-et-conn'>&nbsp;-&gt;&nbsp;</span></td>\n<td align='left'><span class='c-et-conn'>";
	    qsort(&conn[0], conn.size(), sizeof(int), conn_compar);
	} else
	    sa << "<td align='left' colspan='2'><span class='c-et-conn'>&nbsp;not connected";
	for (int j = 0; j < conn.size(); j++) {
	    const ConnectionT &c = r->connection(conn[j]);
	    if (j)
		sa << ", ";
	    sa << "[" << c.to_port() << "]<a href='#"
	       << link_element_table(c.to_elt(), 0) << "'>"
	       << c.to_elt()->name() << "</a>";
	}
	sa << "<br></span></td></tr>\n";
    }
    if (e->noutputs() == 0)
	sa << "<tr valign='top'>"
	   << "<td colspan='3'><span class='c-et-port'>no outputs<br></span></td></tr>\n";

    sa << "</table></td></tr></table>\n";
    fputs(sa.cc(), outf);
}

static void
output_etables(const Vector<ElementT *> &elements, RouterT *r, FILE *outf,
	       const HashMap<String, String> &attrs)
{
    String config_link = attrs["configlink"];
    for (int i = 0; i < elements.size(); i++)
	output_element_table(elements[i], r, outf, config_link);
}


static String
eindex_string(ElementT *e, const String &config_link, const String &etable_link)
{
    StringAccum sa;
    sa << "<p class='c-ei'><span class='c-eih'><span class='c-ei-elt'>" << e->name() << "</span></span>"
       << "<span class='c-ei-class'> :: " << e->type_name() << "</span>";
    if (config_link)
	link_around(sa, "#" + link_element_decl(e, 0 /*XXX*/), config_link);
    if (etable_link)
	link_around(sa, "#" + link_element_table(e, 0 /*XXX*/), etable_link);
    sa << "</p>\n";
    return sa.take_string();
}

static String
eindex_type_string(ElementClassT *type, const Vector<ElementT *> &elements, const String &fake_landmark, const String &, const String &)
{
    StringAccum sa;
    sa << "<p class='c-eit'><span class='c-eih'><span class='c-ei-class'>" << type->name() << "</span></span> (type):</p>";
    sa << "<p class='c-eite'><i>see</i> ";
    const char *sep = "";
    for (int i = 0; i < elements.size(); i++)
	if (elements[i]->type() == type && elements[i]->landmark() != fake_landmark) {
	    ElementT *e = elements[i];
	    sa << sep << "<span class='c-eite-elt'>" << e->name() << "</span>";
	    //if (config_link)
	    //link_around(sa, "#" + link_element_decl(e, 0 /*XXX*/), config_link);
	    //if (etable_link)
	    //link_around(sa, "#" + link_element_table(e, 0 /*XXX*/), etable_link);
	    sep = ", ";
	}
    sa << "</p>\n";
    return sa.take_string();
}

static void
output_eindex(RouterT *r, FILE *outf,
	      const HashMap<String, String> &attrs)
{
    String config_link = attrs["configlink"];
    String etable_link = attrs["etablelink"];
    int which_col, ncol;
    parse_columns(attrs["column"], which_col, ncol);
    bool do_types = (attrs["types"] && attrs["types"] != "0");

    // get list of elements
    Vector<ElementT *> elements, free_elements;
    HashMap<int, int> done_types(-1);
    String fake_landmark = "<<fake>>";
    
    for (RouterT::iterator x = r->first_element(); x; x++) {
	elements.push_back(x);
	if (do_types && done_types[x->type_uid()] < 0) {
	    ElementT *fake = new ElementT(x->type_name(), x->type(), "", fake_landmark);
	    elements.push_back(fake);
	    free_elements.push_back(fake);
	    done_types.insert(x->type_uid(), 1);
	}
    }
    
    if (elements.size())
	qsort(&elements[0], elements.size(), sizeof(ElementT *), element_name_compar);

    // divide into columns
    int per_col = ((elements.size() - 1) / ncol) + 1;
    int first = (which_col - 1) * per_col;
    int last = (which_col == ncol ? elements.size() : which_col * per_col);
    
    for (int i = first; i < last; i++) {
	String s;
	if (elements[i]->landmark() == fake_landmark)
	    s = eindex_type_string(elements[i]->type(), elements, fake_landmark, config_link, etable_link);
	else
	    s = eindex_string(elements[i], config_link, etable_link);
	fputs(s, outf);
    }

    // clean up
    for (int i = 0; i < free_elements.size(); i++)
	delete free_elements[i];
}



static const char *
skip_comment(const char *x)
{
    while (1) {
	if (!*x)
	    return x;
	else if (*x == '>')
	    return x + 1;
	else if (x[0] == '-' && x[1] == '-') {
	    for (x += 2; *x && (x[0] != '-' || x[1] != '-'); x++)
		/* nada */;
	    if (*x)		// skip whitespace after comment
		for (x += 2; isspace(*x); x++)
		    /* nada */;
	} else {		// should not happen
	    for (x++; *x && (x[0] != '-' || x[1] != '-') && *x != '>'; x++)
		/* nada */;
	}
    }
}

static HashMap<String, String> html_entities;

static String
html_unquote(const char *x, const char *end)
{
    if (!html_entities["&amp"]) {
	html_entities.insert("&amp", "&");
	html_entities.insert("&quot", "\"");
	html_entities.insert("&lt", "<");
	html_entities.insert("&gt", ">");
    }
    
    StringAccum sa;
    while (x < end) {
	if (*x == '&') {
	    if (x < end - 2 && x[1] == '#') {
		int val = 0;
		for (x += 2; x < end && isdigit(*x); x++)
		    val = (val * 10) + *x - '0';
		sa << (char)val;
		if (x < end && *x == ';')
		    x++;
	    } else {
		const char *start = x;
		for (x++; x < end && isalnum(*x); x++)
		    /* nada */;
		String entity_name = String(start, x - start);
		String entity_value = html_entities[entity_name];
		sa << (entity_value ? entity_value : entity_name);
		if (x < end && *x == ';' && entity_value)
		    x++;
	    }
	} else
	    sa << *x++;
    }
    return sa.take_string();
}

static const char *
process_tag(const char *x, String &tag, HashMap<String, String> &attrs,
	    bool &ended)
{
    // process tag
    while (isspace(*x))
	x++;
    const char *tag_start = x;
    while (*x && *x != '>' && !isspace(*x))
	x++;
    tag = String(tag_start, x - tag_start).lower();
    ended = false;
    
    // process attributes
    while (1) {
	while (isspace(*x))
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
	while (*x && *x != '>' && !isspace(*x) && *x != '=')
	    x++;
	String attr_name = html_unquote(attr_start, x).lower();

	// look for '=' if any
	while (isspace(*x))
	    x++;
	if (*x != '=') {
	    attrs.insert(attr_name, attr_name);
	    continue;
	}

	// attribute value
	String attr_value;
	for (x++; isspace(*x); x++)
	    /* nada */;
	if (*x == '\'') {
	    const char *value_start = x + 1;
	    for (x++; *x && *x != '\''; x++)
		/* nada */;
	    attr_value = html_unquote(value_start, x);
	    if (*x)
		x++;
	} else if (*x == '\"') {
	    const char *value_start = x + 1;
	    for (x++; *x && *x != '\"'; x++)
		/* nada */;
	    attr_value = html_unquote(value_start, x);
	    if (*x)
		x++;
	} else {
	    const char *value_start = x;
	    for (; *x && !isspace(*x) && *x != '>'; x++)
		/* nada */;
	    attr_value = html_unquote(value_start, x);
	}
	attrs.insert(attr_name, attr_value);
    }
}

static const char *
output_template_until_tag(const char *templ, FILE *outf,
			  String &tag, HashMap<String, String> &attrs,
			  bool &ended)
{
    // skip to next directive
    const char *x = templ;
    int q;
    while (*x) {
	if (x[0] == '<' && x[1] == '!') {
	    if (memcmp(x + 2, "-- click-pretty:", 16) == 0) {
		fwrite(templ, 1, x - templ, outf);
		return process_tag(x + 18, tag, attrs, ended);
	    } else
		x = skip_comment(x + 2);
	} else if (x[0] == '<') {
	    for (x++; *x && *x != '>'; x++)
		if (*x == '\'' || *x == '\"') {
		    for (x++, q = *x; *x && *x != q; x++)
			/* nada */;
		    x--;
		}
	} else
	    x++;
    }

    fwrite(templ, 1, x - templ, outf);
    tag = String();
    return 0;
}

static void
pretty_process(const char *infile, const char *outfile,
	       const char *templ, ErrorHandler *errh)
{
    String r_config;
    RouterT *r = pretty_read_router(infile, errh, r_config);
    if (!r)
	return;

    // open output file
    FILE *outf = stdout;
    if (outfile && strcmp(outfile, "-") != 0) {
	outf = fopen(outfile, "w");
	if (!outf) {
	    errh->error("%s: %s", outfile, strerror(errno));
	    delete r;
	    return;
	}
    }

    // get list of elements
    Vector<ElementT *> elements;
    for (RouterT::iterator x = r->first_element(); x; x++)
	elements.push_back(x);
    if (elements.size())
	qsort(&elements[0], elements.size(), sizeof(ElementT *), element_name_compar);

    // process template
    while (templ) {
	String tag;
	HashMap<String, String> attrs;
	bool ended;
	templ = output_template_until_tag(templ, outf, tag, attrs, ended);

	if (tag == "config")
	    output_config(r_config, outf);
	else if (tag == "etables")
	    output_etables(elements, r, outf, attrs);
	else if (tag == "eindex")
	    output_eindex(r, outf, attrs);
    }
    
    // close files, return
    if (outf != stdout)
	fclose(outf);
    delete r;
}

int
main(int argc, char **argv)
{
    String::static_initialize();
    ErrorHandler::static_initialize(new FileErrorHandler(stderr));
    ErrorHandler *errh = ErrorHandler::default_handler();
    ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-pretty: ");
    CLICK_DEFAULT_PROVIDES;

    // read command line arguments
    Clp_Parser *clp =
	Clp_NewParser(argc, argv, sizeof(options)/sizeof(options[0]), options);
    Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
    program_name = Clp_ProgramName(clp);

    const char *router_file = 0;
    const char *output_file = 0;
    String html_template = default_template;
    bool output = false;

    while (1) {
	int opt = Clp_Next(clp);
	switch (opt) {

	  case HELP_OPT:
	    usage();
	    exit(0);
	    break;

	  case VERSION_OPT:
	    printf("click-pretty (Click) %s\n", CLICK_VERSION);
	    printf("Copyright (c) 2001-2002 International Computer Science Institute\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
	    exit(0);
	    break;

	  case CLICKPATH_OPT:
	    set_clickpath(clp->arg);
	    break;

	  case CLASS_URLS_OPT:
	    default_class_href = clp->arg;
	    break;

	  case TEMPLATE_OPT:
	    html_template = file_string(clp->arg, p_errh);
	    break;

	  case ROUTER_OPT:
	  case Clp_NotOption:
	    if (router_file) {
		p_errh->error("router file specified twice");
		goto bad_option;
	    }
	    router_file = clp->arg;
	    break;

	  case OUTPUT_OPT:
	    if (output_file) {
		p_errh->error("output file specified twice");
		goto bad_option;
	    }
	    output_file = clp->arg;
	    output = true;
	    break;

	  bad_option:
	  case Clp_BadOption:
	    short_usage();
	    exit(1);
	    break;

	  case Clp_Done:
	    goto done;

	}
    }

  done:
    pretty_process(router_file, output_file, html_template, errh);
    exit(errh->nerrors() > 0 ? 1 : 0);
}

#include <click/vector.cc>
#include <click/hashmap.cc>
