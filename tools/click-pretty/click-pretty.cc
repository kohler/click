// -*- c-basic-offset: 4 -*-
/*
 * click-pretty.cc -- pretty-print Click configurations
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#define HTML_BOILERPLATE_OPT	306

static Clp_Option options[] = {
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
    { "class-urls", 'u', CLASS_URLS_OPT, Clp_ArgString, 0 },
    { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
    { "help", 0, HELP_OPT, 0, 0 },
    { "html-boilerplate", 'h', HTML_BOILERPLATE_OPT, 0, Clp_Negate },
    { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
    { "version", 'v', VERSION_OPT, 0, 0 },
};

static String::Initializer string_initializer;
static const char *program_name;
static bool html_boilerplate = false;

static const char *html_header = "\
<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n\
<html><head>\n\
<meta http-equiv='Content-Type' content='text/html; charset=ISO-8859-1'>\n\
<meta http-equiv='Content-Style-Type' content='text/css'>\n\
<style type='text/css'><!--\n\
SPAN.click-kw {\n\
  font-weight: bold;\n\
}\n\
SPAN.click-cdecl {\n\
  font-style: italic;\n\
}\n\
SPAN.click-cstr {\n\
  color: green;\n\
}\n\
SPAN.click-cmt {\n\
  color: gray;\n\
}\n\
SPAN.click-err {\n\
  color: red;\n\
  font-weight: bold;\n\
}\n\
SPAN.click-err:hover {\n\
  color: black;\n\
  font-weight: bold;\n\
  background-color: #000;\n\
}\n\
--></style>\n\
<body>\n";

static const char *html_footer = "\
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
	else
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

class PrettyLexerTInfo : public LexerTInfo { public:

    PrettyLexerTInfo()				{ }
  
    void notify_comment(int pos1, int pos2) {
	add_item(pos1, "<span class='click-cmt'>", pos2, "</span>");
    }
    void notify_error(const String &what, int pos1, int pos2) {
	add_item(pos1, "<span class='click-err' title='" + url_attr_quote(what) + "'>", pos2, "</span>");
    }
    void notify_keyword(const String &, int pos1, int pos2) {
	add_item(pos1, "<span class='click-kw'>", pos2, "</span>");
    }
    void notify_config_string(int pos1, int pos2) {
	add_item(pos1, "<span class='click-cstr'>", pos2, "</span>");
    }
    void notify_class_declaration(ElementClassT *ec, bool anonymous, int decl_pos1, int name_pos1, int) {
	int uid = ec->unique_id();
	if (!anonymous)
	    add_item(name_pos1, "<a name='decl" + String(uid) + "'><span class='click-cdecl'>", name_pos1 + ec->name().length(), "</span></a>");
	else
	    add_item(decl_pos1, "<a name='decl" + String(uid) + "'>", decl_pos1 + 1, "</a>");
	add_class_href(uid, "#decl" + String(uid));
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
    void notify_element_declaration(const String &name, ElementClassT *type, ElementClassT *enclose, int pos1, int pos2, int decl_pos2) {
	if (!enclose)
	    add_item(pos1, "<a name='e-" + name + "'>", pos2, "</a>");
	else
	    add_item(pos1, "<a name='e" + String(enclose->unique_id()) + "-" + name + "'>", pos2, "</a>");
	notify_element_reference(name, type, enclose, pos1, decl_pos2);
    }
    void notify_element_reference(const String &name, ElementClassT *type, ElementClassT *, int pos1, int pos2) {
	add_item(pos1, "<span title='" + name + " :: " + type->name() + "'>", pos2, "</span>");
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
`Click-pretty' checks a Click router configuration for correctness and reports\n\
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

static void
output_element_connections(ElementT *e, RouterT *r, FILE *outf)
{
    
}

static void
pretty_process(const char *infile, const char *outfile, ErrorHandler *errh)
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

    if (html_boilerplate)
	fputs(html_header, outf);

    // output configuration
    output_config(r_config, outf);
    
    if (html_boilerplate)
	fputs(html_footer, outf);
    
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
    ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-check: ");
    CLICK_DEFAULT_PROVIDES;

    // read command line arguments
    Clp_Parser *clp =
	Clp_NewParser(argc, argv, sizeof(options)/sizeof(options[0]), options);
    Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
    program_name = Clp_ProgramName(clp);

    const char *router_file = 0;
    const char *output_file = 0;
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
	    printf("Copyright (c) 2001 International Computer Science Institute\n\
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

	  case HTML_BOILERPLATE_OPT:
	    html_boilerplate = !clp->negated;
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
    pretty_process(router_file, output_file, errh);
    exit(errh->nerrors() > 0 ? 1 : 0);
}

#include <click/vector.cc>
#include <click/hashmap.cc>
