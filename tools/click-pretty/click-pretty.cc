// -*- c-basic-offset: 4 -*-
/*
 * click-pretty.cc -- pretty-print Click configurations
 * Eddie Kohler
 *
 * Copyright (c) 2001-2002 International Computer Science Institute
 * Copyright (c) 2007 Regents of the University of California
 * Copyright (c) 2009 Intel Corporation
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
#include "html.hh"
#include <click/error.hh>
#include <click/driver.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include "processingt.hh"
#include "elementmap.hh"

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define OUTPUT_OPT		305
#define CLASS_URLS_OPT		306
#define TEMPLATE_OPT		307
#define WRITE_TEMPLATE_OPT	308
#define DEFINE_OPT		309
#define PACKAGE_URLS_OPT	310
#define DOT_OPT			311
#define GML_OPT			312
#define GRAPHML_OPT		313
#define TEMPLATE_TEXT_OPT	314

#define FIRST_DRIVER_OPT	1000
#define USERLEVEL_OPT		(1000 + Driver::USERLEVEL)
#define LINUXMODULE_OPT		(1000 + Driver::LINUXMODULE)
#define BSDMODULE_OPT		(1000 + Driver::BSDMODULE)

static const Clp_Option options[] = {
    { "bsdmodule", 'b', BSDMODULE_OPT, 0, 0 },
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ValString, 0 },
    { "class-docs", 'u', CLASS_URLS_OPT, Clp_ValString, 0 },
    { "define", 'd', DEFINE_OPT, Clp_ValString, 0 },
    { "dot", 0, DOT_OPT, 0, 0 },
    { "expression", 'e', EXPRESSION_OPT, Clp_ValString, 0 },
    { "file", 'f', ROUTER_OPT, Clp_ValString, 0 },
    { "gml", 0, GML_OPT, 0, 0 },
    { "graphml", 0, GRAPHML_OPT, 0, 0 },
    { "help", 0, HELP_OPT, 0, 0 },
    { "kernel", 'k', LINUXMODULE_OPT, 0, 0 }, // DEPRECATED
    { "linuxmodule", 'l', LINUXMODULE_OPT, 0, 0 },
    { "output", 'o', OUTPUT_OPT, Clp_ValString, 0 },
    { "package-docs", 0, PACKAGE_URLS_OPT, Clp_ValString, 0 },
    { "template", 't', TEMPLATE_OPT, Clp_ValString, Clp_PreferredMatch },
    { "template-text", 'T', TEMPLATE_TEXT_OPT, Clp_ValString, 0 },
    { "userlevel", 0, USERLEVEL_OPT, 0, 0 },
    { "version", 'v', VERSION_OPT, 0, 0 },
    { "write-template", 0, WRITE_TEMPLATE_OPT, 0, 0 },
};

static const char *program_name;
static HashTable<String, String> definitions;
static int specified_driver = -1;

static const char *default_html_template = "\
<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n\
<html><head>\n\
<meta http-equiv='Content-Type' content='text/html; charset=ISO-8859-1'>\n\
<meta http-equiv='Content-Style-Type' content='text/css'>\n\
<style type='text/css'><!--\n\
SPAN.c-kw {\n\
  font-weight: bold;\n\
}\n\
SPAN.c-cd {\n\
  font-style: italic;\n\
}\n\
SPAN.c-cfg {\n\
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
P.ei, P.eit {\n\
  margin-top: 0px;\n\
  margin-bottom: 0px;\n\
  text-indent: -2em;\n\
  margin-left: 2em;\n\
  font-family: sans-serif;\n\
}\n\
P.et {\n\
  font-family: sans-serif;\n\
}\n\
TABLE.conntable TR TD {\n\
  font-family: sans-serif;\n\
  font-size: small;\n\
}\n\
--></style>\n\
<body>\n\
<h1>Configuration</h1>\n\
<~config>\n\
<h1><a name='index'>Element index</a></h1>\n\
<table cellspacing='0' cellpadding='0' border='0'>\n\
<tr valign='top'>\n\
<td><~elements\n\
  entry='<p class=\"ei\"><b><~name></b> :: <~type link> - <~configlink sep=\", \"><a href=\"#et-<~name>\"><i>table</i></a></p>'\n\
  typeentry='<p class=\"eit\"><b><~type link></b> (type)<br><i>see</i> <~typerefs sep=\", \"></p>'\n\
  typeref='<~name>'\n\
  configlink='<i>config</i>'\n\
  column='1/2' /--></td>\n\
<td width='15'>&nbsp;</td>\n\
<td><~elements\n\
  entry='<p class=\"ei\"><b><~name></b> :: <~type link> - <~configlink sep=\", \"><a href=\"#et-<~name>\"><i>table</i></a></p>'\n\
  typeentry='<p class=\"eit\"><b><~type link></b> (type)<br><i>see</i> <~typerefs sep=\", \"></p>'\n\
  typeref='<~name>'\n\
  configlink='<i>config</i>'\n\
  column='2/2' /--></td>\n\
</tr>\n\
</table>\n\
<h1><a name='tables'>Element tables</a></h1>\n\
<table cellspacing='0' cellpadding='0' border='0'>\n\
<tr valign='top'>\n\
<td><~elements\n\
  entry='<table cellspacing=\"0\" cellpadding=\"2\" border=\"0\">\n\
    <tr valign=\"top\"><td colspan=\"2\"><p class=\"et\"><a name=\"et-<~name>\"><b><~name></b></a> :: <~type link> <~configlink></p></td></tr>\n\
    <tr valign=\"top\"><td width=\"20\">&nbsp;</td>\n\
       <td><table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" class=\"conntable\">\n\
         <~inputs><~outputs>\n\
       </table></td></tr>\n\
    </table>'\n\
  configlink='<small>(<i>config</i>)</small>'\n\
  inputentry='<tr valign=\"top\"><td>input&nbsp;&nbsp;</td><td align=\"right\"><~port></td><td align=\"center\">&nbsp;(<~processing>)</td><td>&nbsp;&lt;-&nbsp;</td><td><~inputconnections sep=\", \"></td></tr>'\n\
  noinputentry='<tr valign=\"top\"><td colspan=\"5\">no inputs</td></tr>'\n\
  outputentry='<tr valign=\"top\"><td>output&nbsp;</td><td align=\"right\"><~port></td><td align=\"center\">&nbsp;(<~processing>)</td><td>&nbsp;-&gt;&nbsp;</td><td><~outputconnections sep=\", \"></td></tr>'\n\
  nooutputentry='<tr valign=\"top\"><td colspan=\"5\">no outputs</td></tr>'\n\
  inputconnection='<a href=\"#et-<~name>\"><~name></a>&nbsp;[<~port>]'\n\
  outputconnection='[<~port>]&nbsp;<a href=\"#et-<~name>\"><~name></a>'\n\
  noinputconnection='not connected'\n\
  nooutputconnection='not connected'\n\
  column='1/2'\n\
  /-->\n\
</td><td width='20'>&nbsp;</td>\n\
<td><~elements\n\
  entry='<table cellspacing=\"0\" cellpadding=\"2\" border=\"0\">\n\
    <tr valign=\"top\"><td colspan=\"2\"><p class=\"et\"><a name=\"et-<~name>\"><b><~name></b></a> :: <~type link> <~configlink></p></td></tr>\n\
    <tr valign=\"top\"><td width=\"20\">&nbsp;</td>\n\
       <td><table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" class=\"conntable\">\n\
         <~inputs><~outputs>\n\
       </table></td></tr>\n\
    </table>'\n\
  configlink='<small>(<i>config</i>)</small>'\n\
  inputentry='<tr valign=\"top\"><td>input&nbsp;&nbsp;</td><td align=\"right\"><~port></td><td align=\"center\">&nbsp;(<~processing>)</td><td>&nbsp;&lt;-&nbsp;</td><td><~inputconnections sep=\", \"></td></tr>'\n\
  noinputentry='<tr valign=\"top\"><td colspan=\"5\">no inputs</td></tr>'\n\
  outputentry='<tr valign=\"top\"><td>output&nbsp;</td><td align=\"right\"><~port></td><td align=\"center\">&nbsp;(<~processing>)</td><td>&nbsp;-&gt;&nbsp;</td><td><~outputconnections sep=\", \"></td></tr>'\n\
  nooutputentry='<tr valign=\"top\"><td colspan=\"5\">no outputs</td></tr>'\n\
  inputconnection='<a href=\"#et-<~name>\"><~name></a>&nbsp;[<~port>]'\n\
  outputconnection='[<~port>]&nbsp;<a href=\"#et-<~name>\"><~name></a>'\n\
  noinputconnection='not connected'\n\
  nooutputconnection='not connected'\n\
  column='2/2'\n\
  /-->\n\
</td></tr>\n\
</table>\n\
</body>\n\
</html>\n";

static const char *default_graph_template = "\
<~if test=\"<~anonymous>\" then=\"<~type>\" else=\"<~name> :: <~type>\">";


// list of classes

static HashTable<ElementClassT *, int> class2cid(-1);
static Vector<ElementClassT *> cid2class;
static Vector<String> cid_hrefs;
static HashTable<String, String> package_hrefs;

static int
cid(ElementClassT *type)
{
    HashTable<ElementClassT *, int>::iterator it = class2cid.find_insert(type, -1);
    if (it.value() < 0) {
	it.value() = cid2class.size();
	cid2class.push_back(type);
	cid_hrefs.push_back(String());
    }
    return it.value();
}

static void
add_class_href(ElementClassT *type, const String &href)
{
    cid_hrefs[cid(type)] = href;
}

static String
class_href(ElementClassT *type)
{
    String &sp = cid_hrefs[cid(type)];
    if (sp)
	return sp;
    else if (String href = type->documentation_url()) {
	add_class_href(type, href);
	return href;
    } else if (String doc_name = type->documentation_name()) {
	String package_href = package_hrefs.get("x" + type->package());
	if (!package_href)
	    package_href = package_hrefs["x"];
	String href = percent_substitute(package_href, 's', doc_name.c_str(), 0);
	add_class_href(type, href);
	return href;
    } else {
	add_class_href(type, String());
	return String();
    }
}


// handle output items

struct OutputItem {
    int pos;
    String text;
    int _other;
    union {
	ElementT *element;
	ElementClassT *eclass;
    } _u;
    bool active : 1;
    bool _end_item : 1;
    int _type;
    enum { OI_NORMAL, OI_ELEMENT_REF, OI_ECLASS_REF };

    OutputItem() : pos(-1), _other(-1), active(0), _end_item(0), _type(OI_NORMAL) { _u.element = 0; }
    OutputItem(int p, const String &t, bool ei) : pos(p), text(t), _other(-1), active(0), _end_item(ei), _type(OI_NORMAL) { _u.element = 0; }
    OutputItem(int p, ElementT *e, bool ei) : pos(p), _other(-1), active(0), _end_item(ei), _type(OI_ELEMENT_REF) { _u.element = e; }
    OutputItem(int p, ElementClassT *ec, bool ei) : pos(p), _other(-1), active(0), _end_item(ei), _type(OI_ECLASS_REF) { _u.eclass = ec; }

    bool end_item() const		{ return _end_item; }
    OutputItem *other() const;
    int other_index() const		{ return _other; }
    int item_index() const;
    int end_item_index() const;
    void activate(bool a);
};

static Vector<OutputItem> items;
static Vector<OutputItem> end_items;
static bool items_prepared;

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

static void
add_item(int p1, ElementT *e1, int p2, const String &t2)
{
    items.push_back(OutputItem(p1, e1, false));
    end_items.push_back(OutputItem(p2, t2, true));
    items.back()._other = end_items.back()._other = items.size() - 1;
}

static void
add_item(int p1, ElementClassT *e1, int p2, const String &t2)
{
    items.push_back(OutputItem(p1, e1, false));
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
    if (items_prepared)
	return;
    items_prepared = true;

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

    // update class references
    for (int i = 0; i < items.size(); i++) {
	OutputItem *s = &items[i], *e = s->other();
	if (s->_type == OutputItem::OI_ECLASS_REF) {
	    if (String href = class_href(s->_u.eclass)) {
		s->text = "<a href='" + href + "'>";
		e->text = "</a>";
	    } else
		s->text = "";
	} else if (s->_type == OutputItem::OI_ELEMENT_REF) {
	    s->text = "<span title='" + s->_u.element->name() + " :: " + s->_u.element->type_name() + "'>";
	}
    }

    // combine items that need to be combined (<a href> and <a name>)
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

static String
link_class_decl(ElementClassT *type)
{
    return "decl" + String(cid(type));
}

static String
link_element_decl(ElementT *e)
{
    if (e->router()->declaration_scope())
	return "e" + String(cid(e->router())) + "-" + e->name();
    else
	return "e-" + e->name();
}

class PrettyLexerTInfo : public LexerTInfo { public:

    PrettyLexerTInfo(const String &config)	: _config(config) { }

    void add_item(const char *pos1, const String &s1, const char *pos2, const String &s2) {
	::add_item(pos1 - _config.begin(), s1, pos2 - _config.begin(), s2);
    }
    void add_item(const char *pos1, ElementT *e1, const char *pos2, const String &s2) {
	::add_item(pos1 - _config.begin(), e1, pos2 - _config.begin(), s2);
    }
    void add_item(const char *pos1, ElementClassT *e1, const char *pos2, const String &s2) {
	::add_item(pos1 - _config.begin(), e1, pos2 - _config.begin(), s2);
    }
    void notify_comment(const char *pos1, const char *pos2) {
	add_item(pos1, "<span class='c-cmt'>", pos2, "</span>");
    }
    void notify_error(const String &what, const char *pos1, const char *pos2) {
	add_item(pos1, "<span class='c-err' title='" + html_quote_attr(what) + "'>", pos2, "</span>");
    }
    void notify_keyword(const String &, const char *pos1, const char *pos2) {
	add_item(pos1, "<span class='c-kw'>", pos2, "</span>");
    }
    void notify_config_string(const char *pos1, const char *pos2) {
	add_item(pos1, "<span class='c-cfg'>", pos2, "</span>");
    }
    void notify_class_declaration(ElementClassT *ec, bool anonymous, const char *decl_pos1, const char *name_pos1, const char *) {
	if (!anonymous)
	    add_item(name_pos1, "<a name='" + link_class_decl(ec) + "'><span class='c-cd'>", name_pos1 + ec->name().length(), "</span></a>");
	else
	    add_item(decl_pos1, "<a name='" + link_class_decl(ec) + "'>", decl_pos1 + 1, "</a>");
	add_class_href(ec, "#" + link_class_decl(ec));
    }
    void notify_class_extension(ElementClassT *ec, const char *pos1, const char *pos2) {
	add_item(pos1, ec, pos2, "");
    }
    void notify_class_reference(ElementClassT *ec, const char *pos1, const char *pos2) {
	add_item(pos1, ec, pos2, "");
    }
    void notify_element_declaration(ElementT *e, const char *pos1, const char *pos2, const char *decl_pos2) {
	add_item(pos1, "<a name='" + link_element_decl(e) + "'>", pos2, "</a>");
	add_item(pos1, "<span class='c-ed'>", decl_pos2, "</span>");
	notify_element_reference(e, pos1, decl_pos2);
    }
    void notify_element_reference(ElementT *e, const char *pos1, const char *pos2) {
	add_item(pos1, e, pos2, "</span>");
    }

    String _config;

};


void
short_usage()
{
    fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try '%s --help' for more information.\n",
	    program_name, program_name);
}

static RouterT *
pretty_read_router(const char *filename, bool file_is_expr,
		   ErrorHandler *errh, String &config)
{
    // This function is a paraphrase of read_router_file.

    // read file string
    int before_nerrors = errh->nerrors();

    if (file_is_expr)
	config = filename;
    else
	config = file_string(filename, errh);
    if (!config && errh->nerrors() != before_nerrors)
	return 0;

    // set readable filename
    if (file_is_expr)
	filename = "config";
    else if (!filename || strcmp(filename, "-") == 0)
	filename = "<stdin>";

    // check for archive
    Vector<ArchiveElement> archive;
    if (config.length() && config[0] == '!') {
	ArchiveElement::parse(config, archive, errh);
	if (ArchiveElement *ae = ArchiveElement::find(archive, "config"))
	    config = ae->data;
	else {
	    errh->error("%s: archive has no %<config%> section", filename);
	    config = String();
	}
    }

    // clear list of items
    items.clear();
    end_items.clear();
    items_prepared = false;

    // read router
    if (!config.length())
	errh->warning("%s: empty configuration", filename);
    LexerT lexer(ErrorHandler::silent_handler(), false);
    PrettyLexerTInfo pinfo(config);
    lexer.reset(config, archive, filename);
    lexer.set_lexinfo(&pinfo);

    // read statements
    while (lexer.ystatement())
	/* nada */;

    // done
    return lexer.finish(global_scope);
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
    if (!items_prepared)
	prepare_items(r_config.length());

    // loop over characters
    const char *data = r_config.c_str();
    int len = r_config.length();
    int ipos = 0, eipos = 0;
    int first_active = items.size();

    fputs("<pre>", outf);
    for (int pos = 0; pos < len; pos++) {
	while (items[ipos].pos <= pos || end_items[eipos].pos <= pos)
	    if (end_items[eipos].pos <= items[ipos].pos) {
		if (end_items[eipos].active)
		    fputs(end_items[eipos].text.c_str(), outf);
		deactivate(end_items[eipos], first_active, ipos);
		eipos++;
	    } else {
		fputs(items[ipos].text.c_str(), outf);
		activate(items[ipos], first_active);
		ipos++;
	    }

	switch (data[pos]) {

	  case '\n': case '\r':
	    for (int i = ipos - 1; i >= first_active; i--)
		if (items[i].active)
		    fputs(items[i].other()->text.c_str(), outf);
	    fputc('\n', outf);
	    if (data[pos] == '\r' && pos < len - 1 && data[pos+1] == '\n')
		pos++;
	    for (int i = first_active; i < ipos; i++)
		if (items[i].active) {
		    if (items[i].other()->pos <= pos + 1)
			items[i].activate(false);
		    else
			fputs(items[i].text.c_str(), outf);
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
    const char *slash = find(s, '/');
    if (!IntArg().parse(s.substring(s.begin(), slash), which)
	|| !IntArg().parse(s.substring(slash + 1, s.end()), count)
	|| which <= 0 || which > count) {
	which = count = 1;
	return false;
    } else
	return true;
}

extern "C" {
static bool conn_compar_from;
static int
conn_compar(const void *v1, const void *v2)
{
    const ConnectionT *c1 = (const ConnectionT *)v1;
    const ConnectionT *c2 = (const ConnectionT *)v2;
    const PortT &p1 = (conn_compar_from ? c1->from() : c1->to());
    const PortT &p2 = (conn_compar_from ? c2->from() : c2->to());
    if (p1.element == p2.element)
	return p1.port - p2.port;
    else
	return click_strcmp(p1.element->name(), p2.element->name());
}

static int
element_name_compar(const void *v1, const void *v2)
{
    const ElementT **e1 = (const ElementT **)v1, **e2 = (const ElementT **)v2;
    return click_strcmp((*e1)->name(), (*e2)->name());
}
}

static void
sort_connections(Vector<ConnectionT> &conn, bool from)
{
    if (conn.size()) {
	conn_compar_from = from;
	qsort(&conn[0], conn.size(), sizeof(ConnectionT), conn_compar);
    }
}


//
// elements
//

static String type_landmark = "$fake_type$";

class ElementsOutput { public:

    ElementsOutput(RouterT *r, const ProcessingT &processing,
		   const HashTable<String, String> &main_attrs);
    ~ElementsOutput();

    String run(const String &templ, ElementT *e);
    void run(ElementT *e, FILE *outf);
    void run(FILE *outf);

  private:

    RouterT *_router;
    const ProcessingT &_processing;
    const HashTable<String, String> &_main_attrs;
    Vector<ElementT *> _entries;
    Vector<ElementT *> _elements;
    bool _have_entries;
    bool _have_elements;

    StringAccum _sa;
    String _sep;

    void run_template(String, ElementT *, int, bool);
    String expand(const String &, ElementT *, int, bool);
    void create_entries();
    void create_elements();

};


ElementsOutput::ElementsOutput(RouterT *r, const ProcessingT &processing, const HashTable<String, String> &main_attrs)
    : _router(r), _processing(processing), _main_attrs(main_attrs),
      _have_entries(false), _have_elements(false)
{
}

ElementsOutput::~ElementsOutput()
{
    for (int i = 0; i < _entries.size(); i++)
	if (_entries[i]->landmark() == type_landmark)
	    delete _entries[i];
}

void
ElementsOutput::create_elements()
{
    if (!_have_elements) {
	for (RouterT::iterator x = _router->begin_elements(); x; ++x)
	    _elements.push_back(x.get());
	if (_elements.size())	// sort by name
	    qsort(&_elements[0], _elements.size(), sizeof(ElementT *), element_name_compar);
	_have_elements = true;
    }
}

void
ElementsOutput::create_entries()
{
    if (!_have_entries) {
	bool do_elements = _main_attrs["entry"];
	bool do_types = _main_attrs["typeentry"];
	HashTable<ElementClassT *, int> done_types(-1);
	for (RouterT::iterator x = _router->begin_elements(); x; ++x) {
	    if (do_elements)
		_entries.push_back(x.get());
	    if (do_types && done_types[x->type()] < 0) {
		ElementT *fake = new ElementT(x->type_name(), x->type(), "", LandmarkT(type_landmark));
		_entries.push_back(fake);
		done_types.set(x->type(), 1);
	    }
	}
	if (_entries.size())	// sort by name
	    qsort(&_entries[0], _entries.size(), sizeof(ElementT *), element_name_compar);
	_have_entries = true;
    }
}

void
ElementsOutput::run_template(String templ_str, ElementT *e, int port, bool is_output)
{
    ElementClassT *t = e->type();
    bool is_type = (e->landmark() == type_landmark);

    String tag;
    HashTable<String, String> attrs;
    const char *templ = templ_str.c_str();

    while (templ) {
	templ = output_template_until_tag(templ, _sa, tag, attrs, true, &_sep);

	String next_sep;
	int pre_expansion_pos = _sa.length();

	if (tag == "name") {
	    String href, link = attrs["link"].lower();
	    if (link == "type" || (is_type && link))
		href = class_href(t);
	    else if (link)
		href = link_element_decl(e);
	    if (href)
		_sa << _sep << "<a href='" << href << "'>" << e->name() << "</a>";
	    else
		_sa << _sep << e->name();
	} else if (tag == "anonymous") {
	    if (e->was_anonymous())
		_sa << _sep << "yes";
	    else
		_sa << _sep;
	} else if (tag == "type") {
	    String href = (attrs["link"] ? class_href(t) : String());
	    if (href)
		_sa << _sep << "<a href='" << href << "'>" << t->name() << "</a>";
	    else
		_sa << _sep << t->name();
	} else if (tag == "config" && !is_type) {
	    int limit = 0;
	    if (attrs["limit"])
		IntArg().parse(html_unquote(attrs["limit"]), limit);
	    String config = e->configuration();
	    if (limit && config.length() > limit)
		config = config.substring(0, limit) + "...";
	    if (config && attrs["parens"])
		_sa << _sep << '(' << html_quote_text(config) << ')';
	    else if (config)
		_sa << _sep << html_quote_text(config);
	} else if (tag == "typerefs") {
	    String subsep = attrs["sep"];
	    String text = attrs["entry"];
	    if (!text)
		text = _main_attrs["typeref"];
	    create_elements();
	    for (int i = 0; i < _elements.size(); i++)
		if (_elements[i]->type() == t) {
		    run_template(text, _elements[i], -1, false);
		    _sep = subsep;
		}
	} else if (tag == "configlink" && !is_type) {
	    String text = attrs["text"];
	    if (!text)
		text = _main_attrs["configlink"];
	    if (text) {
		text = "<a href='#" + link_element_decl(e) + "'>" + text + "</a>";
		run_template(text, e, port, is_output);
		next_sep = attrs["sep"];
	    }
	} else if (tag == "ninputs" && !is_type) {
	    _sa << _sep << e->ninputs();
	    if (attrs["english"])
		_sa << (e->ninputs() == 1 ? " input" : " inputs");
	} else if (tag == "noutputs" && !is_type) {
	    _sa << _sep << e->noutputs();
	    if (attrs["english"])
		_sa << (e->noutputs() == 1 ? " output" : " outputs");
	} else if (tag == "inputs" && !is_type) {
	    if (e->ninputs() == 0) {
		String text = attrs["noentry"];
		if (!text)
		    text = _main_attrs["noinputentry"];
		run_template(text, e, -1, false);
	    } else {
		String subsep = attrs["sep"];
		String text = attrs["entry"];
		if (!text)
		    text = _main_attrs["inputentry"];
		for (int i = 0; i < e->ninputs(); i++) {
		    run_template(text, e, i, false);
		    _sep = subsep;
		}
	    }
	} else if (tag == "outputs" && !is_type) {
	    if (e->noutputs() == 0) {
		String text = attrs["noentry"];
		if (!text)
		    text = _main_attrs["nooutputentry"];
		run_template(text, e, -1, true);
	    } else {
		String subsep = attrs["sep"];
		String text = attrs["entry"];
		if (!text)
		    text = _main_attrs["outputentry"];
		for (int i = 0; i < e->noutputs(); i++) {
		    run_template(text, e, i, true);
		    _sep = subsep;
		}
	    }
	} else if (tag == "inputconnections" && port >= 0 && !is_output) {
	    Vector<ConnectionT> conn;
	    for (RouterT::conn_iterator it = _router->find_connections_to(PortT(e, port));
		 it != _router->end_connections(); ++it)
		conn.push_back(*it);
	    if (conn.empty()) {
		String text = attrs["noentry"];
		if (!text)
		    text = _main_attrs["noinputconnection"];
		run_template(text, e, port, false);
	    } else {
		sort_connections(conn, true);
		String subsep = attrs["sep"];
		String text = attrs["entry"];
		if (!text)
		    text = _main_attrs["inputconnection"];
		for (Vector<ConnectionT>::iterator it = conn.begin();
		     it != conn.end(); ++it) {
		    run_template(text, it->from_element(), it->from_port(), true);
		    _sep = subsep;
		}
	    }
	} else if (tag == "outputconnections" && port >= 0 && is_output) {
	    Vector<ConnectionT> conn;
	    for (RouterT::conn_iterator it = _router->find_connections_from(PortT(e, port));
		 it != _router->end_connections(); ++it)
		conn.push_back(*it);
	    if (conn.empty()) {
		String text = attrs["noentry"];
		if (!text)
		    text = _main_attrs["nooutputconnection"];
		run_template(text, e, port, true);
	    } else {
		sort_connections(conn, false);
		String subsep = attrs["sep"];
		String text = attrs["entry"];
		if (!text)
		    text = _main_attrs["outputconnection"];
		for (Vector<ConnectionT>::iterator it = conn.begin();
		     it != conn.end(); ++it) {
		    run_template(text, it->to_element(), it->to_port(), false);
		    _sep = subsep;
		}
	    }
	} else if (tag == "port" && port >= 0) {
	    _sa << _sep << port;
	} else if (tag == "processing" && port >= 0) {
	    int p = (is_output ? _processing.output_processing(PortT(e, port))
		     : _processing.input_processing(PortT(e, port)));
	    if (p == ProcessingT::pagnostic)
		_sa << _sep << "agnostic";
	    else if (p & ProcessingT::ppush)
		_sa << _sep << "push";
	    else if (p & ProcessingT::ppull)
		_sa << _sep << "pull";
	    else
		_sa << _sep << "??";
	} else if (tag == "processingcode") {
	    _sa << _sep << t->processing_code();
	} else if (tag == "flowcode") {
	    _sa << _sep << e->flow_code();
	} else if (tag == "if") {
	    String s = expand(attrs["test"], e, port, is_output);
	    bool result;
	    if (String v = attrs.get("eq"))
		result = (expand(v, e, port, is_output) == s);
	    else if (String v = attrs.get("ne"))
		result = (expand(v, e, port, is_output) != s);
	    else if (String v = attrs.get("gt"))
		result = (click_strcmp(s, expand(v, e, port, is_output)) > 0);
	    else if (String v = attrs.get("lt"))
		result = (click_strcmp(s, expand(v, e, port, is_output)) < 0);
	    else if (String v = attrs.get("ge"))
		result = (click_strcmp(s, expand(v, e, port, is_output)) >= 0);
	    else if (String v = attrs.get("le"))
		result = (click_strcmp(s, expand(v, e, port, is_output)) <= 0);
	    else
		result = (s.length() > 0);

	    if (result)
		run_template(attrs["then"], e, port, is_output);
	    else
		run_template(attrs["else"], e, port, is_output);
	} else if (_main_attrs[tag]) {
	    String text = attrs[tag];
	    run_template(text, e, port, is_output);
	} else if (definitions[tag]) {
	    String text = definitions[tag];
	    run_template(text, e, port, is_output);
	}

	if (_sa.length() != pre_expansion_pos)
	    _sep = next_sep;
    }
}

String
ElementsOutput::expand(const String &s, ElementT *e, int port, bool is_output)
{
    int pos = _sa.length();
    run_template(s, e, port, is_output);
    String result(_sa.data() + pos, _sa.length() - pos);
    _sa.pop_back(_sa.length() - pos);
    return result;
}

String
ElementsOutput::run(const String &templ, ElementT *e)
{
    run_template(templ, e, -1, false);
    String s = _sa.take_string();
    _sep = _main_attrs["sep"];
    return s;
}

void
ElementsOutput::run(ElementT *e, FILE *f)
{
    bool is_type = e->landmark() == type_landmark;
    String templ = _main_attrs[is_type ? "typeentry" : "entry"];
    run_template(templ, e, -1, false);
    fputs(_sa.c_str(), f);
    _sa.clear();
    _sep = _main_attrs["sep"];
}

void
ElementsOutput::run(FILE *f)
{
    // divide into columns
    int which_col = 0, ncol = 0;
    parse_columns(_main_attrs["column"], which_col, ncol);
    create_entries();
    int per_col = ((_entries.size() - 1) / ncol) + 1;
    int first = (which_col - 1) * per_col;
    int last = which_col * per_col;
    if (which_col == ncol || last > _entries.size())
	last = _entries.size();

    // actually do output
    for (int i = first; i < last; i++)
	run(_entries[i], f);
}


//
// main loop
//

static FILE *
open_output_file(const char *outfile, ErrorHandler *errh)
{
    FILE *outf = stdout;
    if (outfile && strcmp(outfile, "-") != 0) {
	outf = fopen(outfile, "w");
	if (!outf)
	    errh->error("%s: %s", outfile, strerror(errno));
    }
    return outf;
}

struct PrettyRouter {
    String r_config;
    RouterT *r;
    ElementMap emap;
    ProcessingT *processing;
    FILE *outf;

    PrettyRouter(const char *infile, bool file_is_expr, const char *outfile,
		 ErrorHandler *errh)
	: processing(0), outf(0) {
	r = pretty_read_router(infile, file_is_expr, errh, r_config);
	if (!r)
	    return;

	emap.parse_all_files(r, CLICK_DATADIR, errh);
	emap.set_driver(emap.pick_driver(specified_driver, r, errh));

	processing = new ProcessingT(r, &emap, errh);
	processing->check_types(errh);

	ElementMap::push_default(&emap);

	outf = open_output_file(outfile, errh);
    }
    ~PrettyRouter() {
	ElementMap::pop_default(&emap);
	delete processing;
	delete r;
	if (outf && outf != stdout)
	    fclose(outf);
    }
    bool ok() const {
	return outf != 0;
    }
};

static void
run_template(const char *templ, PrettyRouter &pr)
{
    String tag;
    HashTable<String, String> attrs;

    while (templ) {
	templ = output_template_until_tag(templ, pr.outf, tag, attrs, false);

	if (tag == "config")
	    output_config(pr.r_config, pr.outf);
	else if (tag == "elements") {
	    ElementsOutput eo(pr.r, *pr.processing, attrs);
	    eo.run(pr.outf);
	} else if (String text = definitions.get(tag))
	    run_template(text.c_str(), pr);
    }
}

static void
pretty_process(const char *infile, bool file_is_expr, const char *outfile,
	       const char *templ, ErrorHandler *errh)
{
    PrettyRouter pr(infile, file_is_expr, outfile, errh);
    if (pr.ok())
	run_template(templ, pr);
}


// This algorithm based on the original click-viz script,
// donated by Jose Vasconcellos <jvasco@bellatlantic.net>
static void
pretty_process_dot(const char *infile, bool file_is_expr, const char *outfile,
		   const String &the_template, ErrorHandler *errh)
{
    PrettyRouter pr(infile, file_is_expr, outfile, errh);
    if (!pr.ok())
	return;
    HashTable<String, String> attrs;
    ElementsOutput eo(pr.r, *pr.processing, attrs);

    // write dot configuration
    fprintf(pr.outf, "digraph clickrouter {\n\
  node [shape=record,height=.1]\n\
  edge [arrowhead=normal,arrowtail=none,tailclip=false]\n");

    // print all nodes
    for (RouterT::iterator n = pr.r->begin_elements();
	 n != pr.r->end_elements();
	 ++n) {
	String label_text = eo.run(the_template, n.operator->());
#if 1
	fprintf(pr.outf, "  \"%s\" [label=\"", n->name_c_str());
	if (n->ninputs() || n->noutputs())
	    fprintf(pr.outf, "{");
	if (n->ninputs()) {
	    fprintf(pr.outf, "{");
	    for (int i = 0; i < n->ninputs(); i++)
		fprintf(pr.outf, (i ? "|<i%d>" : "<i%d>"), i);
	    fprintf(pr.outf, "}|");
	}
	fputs(label_text.c_str(), pr.outf);
	if (n->noutputs()) {
	    fprintf(pr.outf, "|{");
	    for (int i = 0; i < n->noutputs(); i++)
		fprintf(pr.outf, (i ? "|<o%d>" : "<o%d>"), i);
	    fprintf(pr.outf, "}");
	}
	if (n->ninputs() || n->noutputs())
	    fprintf(pr.outf, "}");
	fprintf(pr.outf, "\"];\n");
#else
	if (!n->ninputs() && !n->noutputs())
	    fprintf(pr.outf, "  \"%s\" [label=\"%s\"];\n",
		    n->name_c_str(), label_text.c_str());
	else {
	    fprintf(pr.outf, "  \"%s\" [label=< <TABLE BORDER=\"0\">", n->name_c_str());
	    if (n->ninputs() > 0) {
		fprintf(pr.outf, "<TR><TD><TABLE BORDER=\"0\"><TR>");
		for (int i = 0; i < n->ninputs(); i++)
		    fprintf(pr.outf, "<TD PORT=\"i%d\">X</TD>", i);
		fprintf(pr.outf, "</TR></TABLE></TD></TR>");
	    }
	    fprintf(pr.outf, "<TR><TD>%s</TD></TR>", label_text.c_str());
	    if (n->noutputs() > 0) {
		fprintf(pr.outf, "<TR><TD><TABLE BORDER=\"0\"><TR>");
		for (int i = 0; i < n->noutputs(); i++)
		    fprintf(pr.outf, "<TD PORT=\"o%d\">X</TD>", i);
		fprintf(pr.outf, "</TR></TABLE></TD></TR>");
	    }
	    fprintf(pr.outf, "</TABLE> >];\n");
	}
#endif
    }

    // print all connections
    for (RouterT::conn_iterator it = pr.r->begin_connections();
	 it != pr.r->end_connections(); ++it)
	fprintf(pr.outf, "  \"%s\":o%d -> \"%s\":i%d;\n",
		it->from_element()->name_c_str(), it->from_port(),
		it->to_element()->name_c_str(), it->to_port());

    fprintf(pr.outf, "}\n");
}

static void
pretty_process_gml(const char *infile, bool file_is_expr, const char *outfile,
		   const String &the_template, ErrorHandler *errh)
{
    PrettyRouter pr(infile, file_is_expr, outfile, errh);
    if (!pr.ok())
	return;
    HashTable<String, String> attrs;
    ElementsOutput eo(pr.r, *pr.processing, attrs);

    // write dot configuration
    fprintf(pr.outf, "Creator \"click-pretty\"\n\
graph\n[ hierarchic 1\n\
  directed 1\n");

    // print all nodes
    for (RouterT::iterator n = pr.r->begin_elements();
	 n != pr.r->end_elements();
	 ++n) {
	String label_text = eo.run(the_template, n.operator->());
	fprintf(pr.outf, "  node\n  [ id %d\n    label \"%s\"\n  ]\n", n->eindex(), label_text.c_str());
    }

    // print all connections
    for (RouterT::conn_iterator it = pr.r->begin_connections();
	 it != pr.r->end_connections(); ++it) {
	fprintf(pr.outf, "  edge\n  [ source %d\n    target %d\n", it->from_eindex(), it->to_eindex());

	double amt_from = 1. / it->from_element()->noutputs();
	double first_from = -(it->from_element()->noutputs() - 1.) * amt_from;
	double amt_to = 1. / it->to_element()->ninputs();
	double first_to = -(it->to_element()->ninputs() - 1.) * amt_to;
	fprintf(pr.outf, "    edgeAnchor\n    [ xSource %f\n      xTarget %f\n      ySource 1\n      yTarget -1\n    ]\n", first_from + it->from_port() * amt_from, first_to + it->to_port() * amt_to);
	fprintf(pr.outf, "  ]\n");
    }

    fprintf(pr.outf, "]\n");
}

static void
pretty_process_graphml(const char *infile, bool file_is_expr, const char *outfile,
		       const String &the_template, ErrorHandler *errh)
{
    PrettyRouter pr(infile, file_is_expr, outfile, errh);
    if (!pr.ok())
	return;
    HashTable<String, String> attrs;
    ElementsOutput eo(pr.r, *pr.processing, attrs);

    // write dot configuration
    fprintf(pr.outf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd\">\n\
<key id=\"kn\" for=\"node\" attr.name=\"name\" attr.type=\"string\" />\n\
<key id=\"kc\" for=\"node\" attr.name=\"class\" attr.type=\"string\" />\n\
<key id=\"kp\" for=\"port\" attr.name=\"processing\" attr.type=\"string\">\n\
  <default>a</default>\n\
</key>\n\
<graph id=\"G\" edgedefault=\"directed\">\n");

    // print all nodes
    int nodeid = 0;
    for (RouterT::iterator n = pr.r->begin_elements();
	 n != pr.r->end_elements();
	 ++n) {
	String label_text = eo.run(the_template, n.operator->());
	fprintf(pr.outf, "  <node id=\"n%d\" parse.indegree=\"%d\" parse.outdegree=\"%d\">\n\
    <data key=\"kn\">%s</data> <data key=\"kc\">%s</data>\n",
		nodeid++, n->ninputs(), n->noutputs(), n->name().c_str(),
		label_text.c_str());
	for (int i = 0; i < n->ninputs(); i++)
	    fprintf(pr.outf, "    <port name=\"i%d\"> <data key=\"kp\">%c</data> </port>\n", i, pr.processing->decorated_input_processing_letter(PortT(n.operator->(), i)));
	for (int i = 0; i < n->noutputs(); i++)
	    fprintf(pr.outf, "    <port name=\"o%d\"> <data key=\"kp\">%c</data> </port>\n", i, pr.processing->decorated_output_processing_letter(PortT(n.operator->(), i)));
	fprintf(pr.outf, "  </node>\n");
    }

    // print all connections
    int edgeid = 0;
    for (RouterT::conn_iterator it = pr.r->begin_connections();
	 it != pr.r->end_connections(); ++it)
	fprintf(pr.outf, "  <edge id=\"e%d\" source=\"n%d\" target=\"n%d\" sourceport=\"o%d\" targetport=\"i%d\" />\n",
		edgeid++, it->from_eindex(), it->to_eindex(), it->from_port(), it->to_port());

    fprintf(pr.outf, "</graph>\n</graphml>\n");
}

void
usage()
{
    printf("\
'Click-pretty' reads a Click router configuration and outputs an HTML file,\n\
based on a template, showing that configuration with syntax highlighting.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -e, --expression EXPR       Use EXPR as router configuration.\n\
  -o, --output FILE           Write HTML output to FILE.\n\
  -t, --template FILE         Use FILE as the template instead of default.\n\
  -d, --define NAME=TEXT      Define a new tag, NAME, that expands to TEXT.\n\
  -u, --class-docs URL        Link primitive element classes to URL.\n\
      --package-docs PKG=URL  Link element classes in package PKG to URL.\n\
  -l, --linuxmodule           Prefer Linux kernel module elements.\n\
  -b, --bsdmodule             Prefer FreeBSD kernel module elements.\n\
      --userlevel             Prefer user-level driver elements.\n\
      --write-template        Write template as is, without including router.\n\
      --dot                   Output a 'dot' graph definition.\n\
      --gml                   Output a GML graph definition.\n\
      --graphml               Output a GraphML XML graph definition.\n\
  -C, --clickpath PATH        Use PATH for CLICKPATH.\n\
      --help                  Print this message and exit.\n\
  -v, --version               Print version number and exit.\n\
\n\
Report bugs to <click@librelist.com>.\n", program_name);
}

int
main(int argc, char **argv)
{
    click_static_initialize();
    CLICK_DEFAULT_PROVIDES;
    ErrorHandler *errh = ErrorHandler::default_handler();
    ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-pretty: ");

    // read command line arguments
    Clp_Parser *clp =
	Clp_NewParser(argc, argv, sizeof(options)/sizeof(options[0]), options);
    Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
    program_name = Clp_ProgramName(clp);

    const char *router_file = 0;
    bool file_is_expr = false;
    const char *output_file = 0;
    bool explicit_template = false;
    String the_template;
    int action = 0;

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
Copyright (c) 2007 Regents of the University of California\n\
Copyright (c) 2009 Intel Corporation\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
	    exit(0);
	    break;

	  case CLICKPATH_OPT:
	    set_clickpath(clp->vstr);
	    break;

	  case CLASS_URLS_OPT:
	    package_hrefs.set("x", clp->vstr);
	    break;

	  case PACKAGE_URLS_OPT: {
	      String s = clp->vstr;
	      const char *equals = find(s, '=');
	      if (equals == s.end()) {
		  p_errh->error("'--package-urls' option must contain an equals sign");
		  goto bad_option;
	      }
	      package_hrefs.set("x" + s.substring(s.begin(), equals), s.substring(equals + 1, s.end()));
	      break;
	  }

	case TEMPLATE_OPT:
	    explicit_template = true;
	    the_template = file_string(clp->vstr, p_errh);
	    break;

	case TEMPLATE_TEXT_OPT:
	    explicit_template = true;
	    the_template = clp->vstr;
	    break;

	  case DEFINE_OPT: {
	      String s = clp->vstr;
	      const char *equals = find(s, '=');
	      if (equals < s.end())
		  definitions.set(s.substring(s.begin(), equals), s.substring(equals + 1, s.end()));
	      else
		  definitions.set(s, "");
	      break;
	  }

	  case ROUTER_OPT:
	  case EXPRESSION_OPT:
	  router_file:
	    if (router_file) {
		p_errh->error("router configuration specified twice");
		goto bad_option;
	    }
	    router_file = clp->vstr;
	    file_is_expr = (opt == EXPRESSION_OPT);
	    break;

	  case Clp_NotOption:
	    if (!click_maybe_define(clp->vstr, p_errh))
		goto router_file;
	    break;

	  case OUTPUT_OPT:
	    if (output_file) {
		p_errh->error("output file specified twice");
		goto bad_option;
	    }
	    output_file = clp->vstr;
	    break;

	  case USERLEVEL_OPT:
	  case LINUXMODULE_OPT:
	  case BSDMODULE_OPT:
	    if (specified_driver >= 0) {
		p_errh->error("driver specified twice");
		goto bad_option;
	    }
	    specified_driver = opt - FIRST_DRIVER_OPT;
	    break;

	  case WRITE_TEMPLATE_OPT:
	  case DOT_OPT:
	  case GML_OPT:
	  case GRAPHML_OPT:
	    if (action) {
		p_errh->error("action specified twice");
		goto bad_option;
	    }
	    action = opt;
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
    if (!explicit_template) {
	if (action == DOT_OPT || action == GML_OPT || action == GRAPHML_OPT)
	    the_template = default_graph_template;
	else
	    the_template = default_html_template;
    }

    if (action == WRITE_TEMPLATE_OPT) {
	if (FILE *f = open_output_file(output_file, errh)) {
	    fputs(the_template.c_str(), f);
	    fclose(f);
	}
    } else if (action == DOT_OPT)
	pretty_process_dot(router_file, file_is_expr, output_file, the_template, errh);
    else if (action == GML_OPT)
	pretty_process_gml(router_file, file_is_expr, output_file, the_template, errh);
    else if (action == GRAPHML_OPT)
	pretty_process_graphml(router_file, file_is_expr, output_file, the_template, errh);
    else
	pretty_process(router_file, file_is_expr, output_file, the_template.c_str(), errh);

    exit(errh->nerrors() > 0 ? 1 : 0);
}
