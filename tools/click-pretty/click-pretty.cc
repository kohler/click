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
#include "html.hh"
#include <click/error.hh>
#include <click/driver.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include "processingt.hh"
#include "elementmap.hh"

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define OUTPUT_OPT		304
#define CLASS_URLS_OPT		305
#define TEMPLATE_OPT		306
#define WRITE_TEMPLATE_OPT	307
#define DEFINE_OPT		308

#define FIRST_DRIVER_OPT	1000
#define LINUXMODULE_OPT		(1000 + Driver::LINUXMODULE)
#define USERLEVEL_OPT		(1000 + Driver::USERLEVEL)
#define BSDMODULE_OPT		(1000 + Driver::BSDMODULE)

static Clp_Option options[] = {
    { "bsdmodule", 'b', BSDMODULE_OPT, 0, 0 },
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
    { "class-urls", 'u', CLASS_URLS_OPT, Clp_ArgString, 0 },
    { "define", 'd', DEFINE_OPT, Clp_ArgString, 0 },
    { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
    { "help", 0, HELP_OPT, 0, 0 },
    { "kernel", 'k', LINUXMODULE_OPT, 0, 0 },
    { "linuxmodule", 'l', LINUXMODULE_OPT, 0, 0 },
    { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
    { "template", 't', TEMPLATE_OPT, Clp_ArgString, 0 },
    { "userlevel", 0, USERLEVEL_OPT, 0, 0 },
    { "version", 'v', VERSION_OPT, 0, 0 },
    { "write-template", 0, WRITE_TEMPLATE_OPT, 0, Clp_Negate },
};

static String::Initializer string_initializer;
static const char *program_name;
static HashMap<String, String> definitions;
static int specified_driver = -1;
static const ElementMap *element_map;

static const char *default_template = "\
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
TABLE.conntable {\n\
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
  inputentry='<tr valign=\"top\"><td><~processing>&nbsp;input&nbsp;&nbsp;</td><td align=\"right\"><~port></td><td>&nbsp;&lt;-&nbsp;</td><td><~inputconnections sep=\", \"></td></tr>'\n\
  noinputentry='<tr valign=\"top\"><td colspan=\"4\">no inputs</td></tr>'\n\
  outputentry='<tr valign=\"top\"><td><~processing>&nbsp;output&nbsp;</td><td align=\"right\"><~port></td><td>&nbsp;-&gt;&nbsp;</td><td><~outputconnections sep=\", \"></td></tr>'\n\
  nooutputentry='<tr valign=\"top\"><td colspan=\"4\">no outputs</td></tr>'\n\
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
  inputentry='<tr valign=\"top\"><td><~processing>&nbsp;input&nbsp;&nbsp;</td><td align=\"right\"><~port></td><td>&nbsp;&lt;-&nbsp;</td><td><~inputconnections sep=\", \"></td></tr>'\n\
  noinputentry='<tr valign=\"top\"><td colspan=\"4\">no inputs</td></tr>'\n\
  outputentry='<tr valign=\"top\"><td><~processing>&nbsp;output&nbsp;</td><td align=\"right\"><~port></td><td>&nbsp;-&gt;&nbsp;</td><td><~outputconnections sep=\", \"></td></tr>'\n\
  nooutputentry='<tr valign=\"top\"><td colspan=\"4\">no outputs</td></tr>'\n\
  inputconnection='<a href=\"#et-<~name>\"><~name></a>&nbsp;[<~port>]'\n\
  outputconnection='[<~port>]&nbsp;<a href=\"#et-<~name>\"><~name></a>'\n\
  noinputconnection='not connected'\n\
  nooutputconnection='not connected'\n\
  column='2/2'\n\
  /-->\n\
</body>\n\
</html>\n";


// list of classes

static Vector<ElementClassT *> classes;
static HashMap<int, String> class_hrefs;
static String default_class_href;

static void
notify_class(ElementClassT *c)
{
    int uid = c->unique_id();
    if (uid >= classes.size())
	classes.resize(uid + 1, 0);
    classes[uid] = c;
}

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
    else if (String href = ec->documentation_url()) {
	add_class_href(ec->unique_id(), href);
	return href;
    } else {
	String href = percent_substitute(default_class_href, 's', ec->name_cc(), 0);
	add_class_href(ec->unique_id(), href);
	return href;
    }
}

static String
class_href(int unique_id)
{
    ElementClassT *ec = classes[unique_id];
    assert(ec);
    return class_href(ec);
}


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
	if (s->text[0] == '{') {
	    int uid;
	    cp_integer(s->text.substring(1), &uid);
	    if (String href = class_href(uid)) {
		s->text = "<a href='" + href + "'>";
		e->text = "</a>";
	    } else
		s->text = "";
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
link_class_decl(ElementClassT *ec)
{
    return "decl" + String(ec->unique_id());
}

static String
link_element_decl(ElementT *e)
{
    if (ElementClassT *enclose = e->enclosing_type())
	return "e" + String(enclose->unique_id()) + "-" + e->name();
    else
	return "e-" + e->name();
}

class PrettyLexerTInfo : public LexerTInfo { public:

    PrettyLexerTInfo()				{ }
  
    void notify_comment(int pos1, int pos2) {
	add_item(pos1, "<span class='c-cmt'>", pos2, "</span>");
    }
    void notify_error(const String &what, int pos1, int pos2) {
	add_item(pos1, "<span class='c-err' title='" + html_quote_attr(what) + "'>", pos2, "</span>");
    }
    void notify_keyword(const String &, int pos1, int pos2) {
	add_item(pos1, "<span class='c-kw'>", pos2, "</span>");
    }
    void notify_config_string(int pos1, int pos2) {
	add_item(pos1, "<span class='c-cfg'>", pos2, "</span>");
    }
    void notify_class_declaration(ElementClassT *ec, bool anonymous, int decl_pos1, int name_pos1, int) {
	if (!anonymous)
	    add_item(name_pos1, "<a name='" + link_class_decl(ec) + "'><span class='c-cd'>", name_pos1 + ec->name().length(), "</span></a>");
	else
	    add_item(decl_pos1, "<a name='" + link_class_decl(ec) + "'>", decl_pos1 + 1, "</a>");
	add_class_href(ec->unique_id(), "#" + link_class_decl(ec));
    }
    void notify_class_extension(ElementClassT *ec, int pos1, int pos2) {
	notify_class(ec);
	add_item(pos1, "{" + String(ec->unique_id()), pos2, "");
    }
    void notify_class_reference(ElementClassT *ec, int pos1, int pos2) {
	notify_class(ec);
	add_item(pos1, "{" + String(ec->unique_id()), pos2, "");
    }
    void notify_element_declaration(ElementT *e, int pos1, int pos2, int decl_pos2) {
	add_item(pos1, "<a name='" + link_element_decl(e) + "'>", pos2, "</a>");
	add_item(pos1, "<span class='c-ed'>", decl_pos2, "</span>");
	notify_element_reference(e, pos1, decl_pos2);
    }
    void notify_element_reference(ElementT *e, int pos1, int pos2) {
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
    end_items.clear();
    items_prepared = false;
  
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
    if (!items_prepared)
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
sort_connections(RouterT *r, Vector<int> &conn, bool from)
{
    conn_compar_connvec = &r->connections();
    conn_compar_from = from;
    if (conn.size())
	qsort(&conn[0], conn.size(), sizeof(int), conn_compar);
}


//
// elements
//

static String type_landmark = "$fake_type$";

class ElementsOutput { public:

    ElementsOutput(RouterT *, const ProcessingT &, const HashMap<String, String> &);
    ~ElementsOutput();

    void run(ElementT *, FILE *);
    void run(FILE *);
    
  private:

    RouterT *_router;
    const ProcessingT &_processing;
    const HashMap<String, String> &_main_attrs;
    Vector<ElementT *> _entries;
    Vector<ElementT *> _elements;

    StringAccum _sa;
    String _sep;

    void run_template(String, ElementT *, int, bool);
    String expand(const String &, ElementT *, int, bool);
    
};


ElementsOutput::ElementsOutput(RouterT *r, const ProcessingT &processing, const HashMap<String, String> &main_attrs)
    : _router(r), _processing(processing), _main_attrs(main_attrs)
{
    bool do_elements = main_attrs["entry"];
    bool do_types = main_attrs["typeentry"];

    // get list of elements and/or types
    HashMap<int, int> done_types(-1);
    for (RouterT::iterator x = r->first_element(); x; x++) {
	_elements.push_back(x);
	if (do_elements)
	    _entries.push_back(x);
	if (do_types && done_types[x->type_uid()] < 0) {
	    ElementT *fake = new ElementT(x->type_name(), x->type(), "", type_landmark);
	    _entries.push_back(fake);
	    done_types.insert(x->type_uid(), 1);
	}
    }

    // sort by name
    if (_elements.size())
	qsort(&_elements[0], _elements.size(), sizeof(ElementT *), element_name_compar);
    if (_entries.size())
	qsort(&_entries[0], _entries.size(), sizeof(ElementT *), element_name_compar);
}

ElementsOutput::~ElementsOutput()
{
    for (int i = 0; i < _entries.size(); i++)
	if (_entries[i]->landmark() == type_landmark)
	    delete _entries[i];
}

void
ElementsOutput::run_template(String templ_str, ElementT *e, int port, bool is_output)
{
    ElementClassT *t = e->type();
    bool is_type = (e->landmark() == type_landmark);
    
    String tag;
    HashMap<String, String> attrs;
    const char *templ = templ_str.cc();

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
	} else if (tag == "type") {
	    String href = (attrs["link"] ? class_href(t) : String());
	    if (href)
		_sa << _sep << "<a href='" << href << "'>" << t->name() << "</a>";
	    else
		_sa << _sep << t->name();
	} else if (tag == "config" && !is_type) {
	    int limit = 0;
	    if (attrs["limit"])
		cp_integer(html_unquote(attrs["limit"]), &limit);
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
	    Vector<int> conn;
	    _router->find_connections_to(PortT(e, port), conn);
	    if (conn.size() == 0) {
		String text = attrs["noentry"];
		if (!text)
		    text = _main_attrs["noinputconnection"];
		run_template(text, e, port, false);
	    } else {
		sort_connections(_router, conn, true);
		String subsep = attrs["sep"];
		String text = attrs["entry"];
		if (!text)
		    text = _main_attrs["inputconnection"];
		for (int i = 0; i < conn.size(); i++) {
		    const ConnectionT &c = _router->connection(conn[i]);
		    run_template(text, c.from_elt(), c.from_port(), true);
		    _sep = subsep;
		}
	    }
	} else if (tag == "outputconnections" && port >= 0 && is_output) {
	    Vector<int> conn;
	    _router->find_connections_from(PortT(e, port), conn);
	    if (conn.size() == 0) {
		String text = attrs["noentry"];
		if (!text)
		    text = _main_attrs["nooutputconnection"];
		run_template(text, e, port, true);
	    } else {
		sort_connections(_router, conn, false);
		String subsep = attrs["sep"];
		String text = attrs["entry"];
		if (!text)
		    text = _main_attrs["outputconnection"];
		for (int i = 0; i < conn.size(); i++) {
		    const ConnectionT &c = _router->connection(conn[i]);
		    run_template(text, c.to_elt(), c.to_port(), false);
		    _sep = subsep;
		}
	    }
	} else if (tag == "port" && port >= 0) {
	    _sa << _sep << port;
	} else if (tag == "processing" && port >= 0) {
	    int p = (is_output ? _processing.output_processing(PortT(e, port))
		     : _processing.input_processing(PortT(e, port)));
	    if (p == ProcessingT::VAGNOSTIC)
		_sa << _sep << "agnostic";
	    else if (p == ProcessingT::VPUSH)
		_sa << _sep << "push";
	    else if (p == ProcessingT::VPULL)
		_sa << _sep << "pull";
	    else
		_sa << _sep << "??";
	} else if (tag == "if") {
	    String s = expand(attrs["test"], e, port, is_output);
	    bool result;
	    if (attrs["eq"])
		result = (expand(attrs["eq"], e, port, is_output) == s);
	    else if (attrs["ne"])
		result = (expand(attrs["ne"], e, port, is_output) != s);
	    else if (attrs["gt"])
		result = (click_strcmp(s, expand(attrs["gt"], e, port, is_output)) > 0);
	    else if (attrs["lt"])
		result = (click_strcmp(s, expand(attrs["lt"], e, port, is_output)) < 0);
	    else if (attrs["ge"])
		result = (click_strcmp(s, expand(attrs["ge"], e, port, is_output)) >= 0);
	    else if (attrs["le"])
		result = (click_strcmp(s, expand(attrs["le"], e, port, is_output)) <= 0);
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

void
ElementsOutput::run(ElementT *e, FILE *f)
{
    bool is_type = e->landmark() == type_landmark;
    String templ = _main_attrs[is_type ? "typeentry" : "entry"];
    run_template(templ, e, -1, false);
    fputs(_sa.cc(), f);
    _sa.clear();
    _sep = _main_attrs["sep"];
}

void
ElementsOutput::run(FILE *f)
{
    // divide into columns
    int which_col, ncol;
    parse_columns(_main_attrs["column"], which_col, ncol);
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

static void
run_template(const char *templ, RouterT *r, const String &r_config,
	     const ElementMap &emap, const ProcessingT &processing, FILE *outf)
{
    String tag;
    HashMap<String, String> attrs;

    while (templ) {
	templ = output_template_until_tag(templ, outf, tag, attrs, false);

	if (tag == "config")
	    output_config(r_config, outf);
	else if (tag == "elements") {
	    ElementsOutput eo(r, processing, attrs);
	    eo.run(outf);
	} else if (definitions[tag]) {
	    String text = definitions[tag];
	    run_template(text, r, r_config, emap, processing, outf);
	}
    }
}

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

static void
pretty_process(const char *infile, const char *outfile,
	       const char *templ, ErrorHandler *errh)
{
    String r_config;
    RouterT *r = pretty_read_router(infile, errh, r_config);
    if (!r)
	return;

    // open output file
    FILE *outf = open_output_file(outfile, errh);
    if (!outf) {
	delete r;
	return;
    }

    // get element map and processing
    ElementMap emap;
    emap.parse_all_files(r, CLICK_SHAREDIR, errh);

    int driver = specified_driver;
    if (driver < 0) {
	int driver_mask = 0;
	for (int d = 0; d < Driver::COUNT; d++)
	    if (emap.driver_compatible(r, d))
		driver_mask |= 1 << d;
	if (driver_mask == 0)
	    errh->warning("configuration not compatible with any driver");
	else {
	    for (int d = Driver::COUNT - 1; d >= 0; d--)
		if (driver_mask & (1 << d))
		    driver = d;
	    if (!emap.driver_indifferent(r, driver_mask, errh))
		errh->warning("configuration not indifferent to driver; arbitrarily picking %s", Driver::name(driver));
	}
    } else if (!emap.driver_compatible(r, driver))
	errh->warning("configuration not compatible with %s driver", Driver::name(driver));

    emap.set_driver(driver);
    ProcessingT processing(r, &emap, errh);

    element_map = &emap;
    ElementMap::push_default(&emap);
    
    // process template
    run_template(templ, r, r_config, emap, processing, outf);

    ElementMap::pop_default();
    
    // close files, return
    if (outf != stdout)
	fclose(outf);
    delete r;
}

void
usage()
{
    printf("\
`Click-pretty' reads a Click router configuration and outputs an HTML file,\n\
based on a template, showing that configuration with syntax highlighting.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE           Read router configuration from FILE.\n\
  -o, --output FILE         Write HTML output to FILE.\n\
  -t, --template FILE       Use FILE as the template instead of default.\n\
  -d, --define NAME=TEXT    Define a new tag, NAME, that expands to TEXT.\n\
  -u, --class-urls URL      Link primitive element classes to URL.\n\
      --write-template      Write template as is, without including router.\n\
  -C, --clickpath PATH      Use PATH for CLICKPATH.\n\
      --help                Print this message and exit.\n\
  -v, --version             Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
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
    bool write_template = false;

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

	  case DEFINE_OPT: {
	      String s = clp->arg;
	      int equals = s.find_left('=');
	      if (equals >= 0)
		  definitions.insert(s.substring(0, equals), s.substring(equals + 1));
	      else
		  definitions.insert(s, "");
	      break;
	  }

	  case WRITE_TEMPLATE_OPT:
	    write_template = !clp->negated;
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

	  case USERLEVEL_OPT:
	  case LINUXMODULE_OPT:
	  case BSDMODULE_OPT:
	    if (specified_driver >= 0) {
		p_errh->error("driver specified twice");
		goto bad_option;
	    }
	    specified_driver = opt - FIRST_DRIVER_OPT;
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
    if (write_template) {
	if (FILE *f = open_output_file(output_file, errh)) {
	    fputs(html_template, f);
	    fclose(f);
	}
    } else
	pretty_process(router_file, output_file, html_template, errh);
	
    exit(errh->nerrors() > 0 ? 1 : 0);
}

#include <click/vector.cc>
#include <click/hashmap.cc>
