// -*- c-basic-offset: 4 -*-
/*
 * click2xml.cc -- translate Click configurations into and out of XML
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

#include <expat.h>
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
#include "elementmap.hh"

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define OUTPUT_OPT		305

#define FIRST_DRIVER_OPT	1000
#define USERLEVEL_OPT		(1000 + Driver::USERLEVEL)
#define LINUXMODULE_OPT		(1000 + Driver::LINUXMODULE)
#define BSDMODULE_OPT		(1000 + Driver::BSDMODULE)

static Clp_Option options[] = {
    { "bsdmodule", 'b', BSDMODULE_OPT, 0, 0 },
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
    { "expression", 'e', EXPRESSION_OPT, Clp_ArgString, 0 },
    { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
    { "help", 0, HELP_OPT, 0, 0 },
    { "kernel", 'k', LINUXMODULE_OPT, 0, 0 },
    { "linuxmodule", 'l', LINUXMODULE_OPT, 0, 0 },
    { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
    { "userlevel", 0, USERLEVEL_OPT, 0, 0 },
    { "version", 'v', VERSION_OPT, 0, 0 },
};

static String::Initializer string_initializer;
static const char *program_name;
static int specified_driver = -1;


//
// main loop
//

static HashMap<int, int> generated_types(0);

static void
space_until(String &s, int last_col, int want_col)
{
    while (last_col < want_col)
	if (((last_col + 8) & ~7) <= want_col) {
	    s += '\t';
	    last_col = (last_col + 8) & ~7;
	} else {
	    s += ' ';
	    last_col++;
	}
}

static String
add_indent(const String &indent, int spaces)
{
    String new_indent;
    const char *s = indent.data();
    int last_col = 0, col = 0, len = indent.length();
    for (int pos = 0; pos < len; pos++)
	if (s[pos] == '\t')
	    col = (col + 8) & ~7;
	else if (s[pos] == ' ')
	    col++;
	else {
	    space_until(new_indent, last_col, col);
	    new_indent += s[pos];
	    col++;
	    last_col = col;
	}
    space_until(new_indent, last_col, col + spaces);
    return new_indent;
}

static void
print_class_reference(FILE *f, ElementClassT *c, const char *prefix)
{
    if (c->simple())
	fprintf(f, "%sclassname=\"%s\"", prefix, String(c->name()).cc());
    else if (c->name())
	fprintf(f, "%sclassname=\"%s\" %sclassid=\"c%d\"", prefix, String(c->name()).cc(), prefix, c->uid());
    else
	fprintf(f, "%sclassid=\"c%d\"", prefix, c->uid());
}

static void
print_landmark_attributes(FILE *f, const String &landmark)
{
    int colon = landmark.find_left(':');
    if (colon >= 0 && landmark.substring(colon + 1) != "0")
	fprintf(f, " file=\"%s\" line=\"%s\"", landmark.substring(0, colon).cc(), landmark.substring(colon + 1).cc());
}

static void generate_router(RouterT *, FILE *, String, bool, ErrorHandler *);

static void
generate_type(ElementClassT *c, FILE *f, String indent, ErrorHandler *errh)
{
    if (!c || c->simple() || generated_types[c->uid()])
	return;
    generated_types.insert(c->uid(), 1);

    Vector<ElementClassT *> prerequisites;
    c->collect_prerequisites(prerequisites);
    for (int i = 0; i < prerequisites.size(); i++)
	generate_type(prerequisites[i], f, indent, errh);

    fprintf(f, "%s<elementclass ", indent.cc());
    print_class_reference(f, c, "");
    
    if (SynonymElementClassT *synonym = c->cast_synonym()) {
	fprintf(f, ">\n%s  <synonym ", indent.cc());
	print_class_reference(f, synonym->synonym_of(), "");
	fprintf(f, " />\n");
    } else if (CompoundElementClassT *compound = c->cast_compound()) {
	print_landmark_attributes(f, compound->landmark());
	fprintf(f, ">\n%s  <compound", indent.cc());
	if (ElementClassT *prev = compound->previous()) {
	    fprintf(f, " ");
	    print_class_reference(f, prev, "prev");
	}
	fprintf(f, " ninputs=\"%d\" noutputs=\"%d\" nformals=\"%d\">\n",
		compound->ninputs(), compound->noutputs(), compound->nformals());

	String new_indent = add_indent(indent, 4);
	for (int i = 0; i < compound->nformals(); i++)
	    fprintf(f, "%s<formal number=\"%d\" name=\"%s\" />\n",
		    new_indent.cc(), i, compound->formals()[i].substring(1).cc());
	generate_router(compound->cast_router(), f, new_indent, false, errh);
	
	fprintf(f, "%s  </compound>\n", indent.cc());
    }
    
    fprintf(f, "%s</elementclass>\n", indent.cc());
}

static void
generate_router(RouterT *r, FILE *f, String indent, bool top, ErrorHandler *errh)
{
    ProcessingT processing(r, ElementMap::default_map(), errh);
    if (top)
	processing.resolve_agnostics();
    
    for (int i = 0; i < r->ntypes(); i++)
	generate_type(r->eclass(i), f, indent, errh);
    
    for (RouterT::iterator e = r->begin_elements(); e; e++)
	if (!e->tunnel()) {
	    fprintf(f, "%s<element name=\"%s\" ", indent.cc(), e->name_cc());
	    print_class_reference(f, e->type(), "");
	    print_landmark_attributes(f, e->landmark());
	    fprintf(f, " ninputs=\"%d\" noutputs=\"%d\"",
		    e->ninputs(), e->noutputs());
	    if (e->ninputs() || e->noutputs())
		fprintf(f, " processing=\"%s\"", processing.processing_code(e).cc());
	    fprintf(f, " />\n");
	}

    // print connections
    const Vector<ConnectionT> &conn = r->connections();
    for (int i = 0; i < conn.size(); i++) {
	int p = processing.output_processing(conn[i].from());
	fprintf(f, "%s<connection from=\"%s\" fromport=\"%d\" to=\"%s\" toport=\"%d\" processing=\"%c\" />\n",
		indent.cc(),
		conn[i].from_elt()->name_cc(), conn[i].from_port(),
		conn[i].to_elt()->name_cc(), conn[i].to_port(),
		ProcessingT::processing_letters[p]);
    }
}

static ErrorHandler *xml_errh;
static String xml_file;
static RouterT *router;

static inline String
xml_landmark(XML_Parser parser)
{
    return xml_file + ":" + String(XML_GetCurrentLineNumber(parser));
}

static int
xml_error(XML_Parser parser, const char *format, ...)
{
    va_list val;
    va_start(val, format);
    xml_errh->verror(ErrorHandler::ERR_ERROR, xml_landmark(parser), format, val);
    va_end(val);
    return -EINVAL;
}


struct CxElement {
    String name;
    String class_name;
    String class_id;
    String config;
    int ninputs;
    int noutputs;
    String landmark;
    String xml_landmark;
    CxElement()			: ninputs(-1), noutputs(-1) { }
};

struct CxConnection {
    String from;
    int fromport;
    String to;
    int toport;
    String xml_landmark;
    CxConnection()		: fromport(0), toport(0) { }
};

struct CxState {
    Vector<CxElement> elements;
    Vector<CxConnection> connections;
    CxState *prev;
};

static CxState *xstate;


static RouterT *
complete(CxState *cx, ErrorHandler *errh)
{
    RouterT *r = new RouterT;

    for (int i = 0; i < cx->elements.size(); i++) {
	CxElement &e = cx->elements[i];
	if (ElementT *old_e = r->elt(e.name)) {
	    int which = (int)(old_e->user_data());
	    ElementT::redeclaration_error(errh, "element", e.name, e.xml_landmark, cx->elements[which].xml_landmark);
	} else {
	    ElementClassT *eclass = r->get_type(e.class_name);
	    ElementT *ne = r->get_element(e.name, eclass, e.config, (e.landmark ? e.landmark : e.xml_landmark));
	    ne->set_user_data((void *)i);
	}
    }

    for (int i = 0; i < cx->connections.size(); i++) {
	CxConnection &c = cx->connections[i];
	ElementT *frome = r->elt(c.from);
	if (!frome) {
	    errh->lerror(c.xml_landmark, "undeclared element '%s' (first use this block)", c.from.cc());
	    frome = r->get_element(c.from, ElementClassT::default_class("Error"), String(), c.xml_landmark);
	}
	ElementT *toe = r->elt(c.to);
	if (!toe) {
	    errh->lerror(c.xml_landmark, "undeclared element '%s' (first use this block)", c.to.cc());
	    toe = r->get_element(c.to, ElementClassT::default_class("Error"), String(), c.xml_landmark);
	}
	r->add_connection(frome, c.fromport, toe, c.toport, c.xml_landmark);
    }

    return r;
}


extern "C" {
    
static void
start_element_handler(void *v, const XML_Char *name, const XML_Char **attrs)
{
    XML_Parser parser = (XML_Parser)v;
    String landmark = xml_landmark(parser);
    
    if (strcmp(name, "configuration") == 0) {
	CxState *xs = new CxState;
	xs->prev = xstate;
	xstate = xs;
	
    } else if (strcmp(name, "element") == 0) {
	if (!xstate) {
	    xml_errh->lerror(landmark, "<element> tag meaningless outside of <configuration>");
	    return;
	}
	
	CxElement e;
	e.xml_landmark = landmark;
	bool ok = true;
	
	String file, line;
	for (const XML_Char **a = attrs; *a; a += 2)
	    if (strcmp(a[0], "name") == 0) {
		if (!cp_is_click_id(a[1]))
		    xml_errh->lerror(landmark, "'name' attribute is not a valid Click identifier");
		e.name = a[1];
	    } else if (strcmp(a[0], "classname") == 0) {
		if (!cp_is_click_id(a[1]))
		    xml_errh->lerror(landmark, "'classname' attribute is not a valid Click identifier");
		e.class_name = a[1];
	    } else if (strcmp(a[0], "classid") == 0)
		e.class_id = a[1];
	    else if (strcmp(a[0], "config") == 0)
		e.config = a[1];
	    else if (strcmp(a[0], "file") == 0)
		file = a[1];
	    else if (strcmp(a[0], "line") == 0)
		line = a[1];
	    else if (strcmp(a[0], "ninputs") == 0) {
		if (!cp_integer(a[1], &e.ninputs))
		    xml_errh->lerror(landmark, "'ninputs' should be an integer number of ports");
	    } else if (strcmp(a[0], "noutputs") == 0) {
		if (!cp_integer(a[1], &e.ninputs))
		    xml_errh->lerror(landmark, "'noutputs' should be an integer number of ports");
	    }
	
	if (file && line)
	    e.landmark = file + ":" + line;
	else if (file)
	    e.landmark = file;
	else if (line)
	    e.landmark = "line " + line;

	if (e.class_name && e.class_id) {
	    xml_errh->lerror(landmark, "cannot specify both 'classname' and 'classid'");
	    e.class_name = String();
	} else if (!e.class_name && !e.class_id) {
	    xml_errh->lerror(landmark, "element declared without a class");
	    ok = false;
	}
	if (!e.name) {
	    xml_errh->lerror(landmark, "element declared without a name");
	    ok = false;
	}

	if (ok)
	    xstate->elements.push_back(e);
	
    } else if (strcmp(name, "connection") == 0) {
	if (!xstate) {
	    xml_errh->lerror(landmark, "<connection> tag meaningless outside of <configuration>");
	    return;
	}
	
	CxConnection e;
	e.xml_landmark = landmark;
	bool ok = true;
	
	for (const XML_Char **a = attrs; *a; a += 2)
	    if (strcmp(a[0], "from") == 0)
		e.from = a[1];
	    else if (strcmp(a[0], "to") == 0)
		e.to = a[1];
	    else if (strcmp(a[0], "fromport") == 0) {
		if (!cp_integer(a[1], &e.fromport) && e.fromport >= 0)
		    xml_errh->lerror(landmark, "bad 'fromport' attribute; expected port number");
	    } else if (strcmp(a[0], "toport") == 0) {
		if (!cp_integer(a[1], &e.toport) && e.toport >= 0)
		    xml_errh->lerror(landmark, "bad 'fromport' attribute; expected port number");
	    }

	if (!e.from || !e.to) {
	    xml_errh->lerror(landmark, "connection lacks 'from' or 'to' attribute");
	    ok = false;
	}
	
	if (ok)
	    xstate->connections.push_back(e);
    }
}

static void
end_element_handler(void *, const XML_Char *name)
{
    if (strcmp(name, "configuration") == 0) {
	// XXX
	CxState *xs = xstate;
	xstate = xs->prev;
	router = complete(xs, xml_errh);
	delete xs;
    }
}

}

static void
process(const char *infile, bool file_is_expr, const char *outfile,
	ErrorHandler *errh)
{
    int before = errh->nerrors();
    
    String contents;
    if (file_is_expr)
	contents = infile;
    else {
	contents = file_string(infile, errh);
	if (!contents && errh->nerrors() != before)
	    return;
    }
    
    XML_Parser parser = XML_ParserCreateNS(0, '|');
    XML_SetElementHandler(parser, start_element_handler, end_element_handler);
    XML_UseParserAsHandlerArg(parser);
    xml_errh = errh;
    xml_file = filename_landmark(infile, file_is_expr);

    if (XML_Parse(parser, contents.data(), contents.length(), 1) == 0) {
	xml_error(parser, "XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
	return;
    }

    if (!router && errh->nerrors() == before)
	errh->lerror(xml_file, "no configuration section");
    
    // open output file
    if (errh->nerrors() == before && router)
	write_router_file(router, outfile, errh);
    
    delete router;
}

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
`Xml2click' reads an XML description of a Click router configuration and\n\
outputs a Click-language file corresponding to that configuration.\n\
\n\
Usage: %s [OPTION]... [XMLFILE]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -e, --expression EXPR       Use EXPR as XML router configuration.\n\
  -o, --output FILE           Write output to FILE.\n\
  -C, --clickpath PATH        Use PATH for CLICKPATH.\n\
      --help                  Print this message and exit.\n\
  -v, --version               Print version number and exit.\n\
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
    bool file_is_expr = false;
    const char *output_file = 0;

    while (1) {
	int opt = Clp_Next(clp);
	switch (opt) {

	  case HELP_OPT:
	    usage();
	    exit(0);
	    break;

	  case VERSION_OPT:
	    printf("xml2click (Click) %s\n", CLICK_VERSION);
	    printf("Copyright (c) 2002 International Computer Science Institute\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
	    exit(0);
	    break;

	  case CLICKPATH_OPT:
	    set_clickpath(clp->arg);
	    break;

	  case ROUTER_OPT:
	  case EXPRESSION_OPT:
	  case Clp_NotOption:
	    if (router_file) {
		p_errh->error("router configuration specified twice");
		goto bad_option;
	    }
	    router_file = clp->arg;
	    file_is_expr = (opt == EXPRESSION_OPT);
	    break;

	  case OUTPUT_OPT:
	    if (output_file) {
		p_errh->error("output file specified twice");
		goto bad_option;
	    }
	    output_file = clp->arg;
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
    process(router_file, file_is_expr, output_file, errh);
	
    exit(errh->nerrors() > 0 ? 1 : 0);
}


#include <click/vector.cc>
