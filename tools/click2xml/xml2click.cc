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

static inline String
xml_landmark(XML_Parser parser)
{
    return xml_file + String(XML_GetCurrentLineNumber(parser));
}

static int
xml_error(XML_Parser parser, const char *format, ...)
{
    va_list val;
    va_start(format, val);
    xml_errh->verror(ErrorHandler::ERR_ERROR, xml_landmark(parser), format, val);
    va_end(val);
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
};

struct CxState {
    Vector<CxElement> elements;
    Vector<CxConnection> connections;
    CxState *prev;
};

static CxState *xstate;


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
	    xml_error("<element> tag meaningless outside of <configuration>");
	    return;
	}
	
	CxElement e;
	e.xml_landmark = landmark;
	
	String file, line;
	for (const XML_Char **a = attrs; *a; a += 2)
	    if (strcmp(a[0], "name") == 0) {
		if (!cp_is_click_id(a[1]))
		    errh->lerror(landmark, "'name' attribute is not a valid Click identifier");
		e.name = a[1];
	    } else if (strcmp(a[0], "classname") == 0) {
		if (!cp_is_click_id(a[1]))
		    errh->lerror(landmark, "'classname' attribute is not a valid Click identifier");
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
		    errh->lerror(landmark, "bad 'ninputs' attribute; expected integer");
	    } else if (strcmp(a[0], "noutputs") == 0) {
		if (!cp_integer(a[1], &e.ninputs))
		    errh->lerror(landmark, "bad 'noutputs' attribute; expected integer");
	    }
	
	if (file && line)
	    e.landmark = file + ":" + line;
	else if (file)
	    e.landmark = file;
	else if (line)
	    e.landmark = "line " + line;

	if (e.class_name && e.class_id) {
	    errh->lerror(landmark, "cannot specify both 'classname' and 'classid'");
	    e.class_name = String();
	}
	
	xstate->elements.push_back(e);
	
    } else if (strcmp(name, "connection") == 0) {
	if (!xstate) {
	    xml_error("<connection> tag meaningless outside of <configuration>");
	    return;
	}
	
	CxConnection e;
	e.xml_landmark = landmark;
	
	for (const XML_Char **a = attrs; *a; a += 2)
	    if (strcmp(a[0], "from") == 0)
		e.from = a[1];
	    else if (strcmp(a[0], "to") == 0)
		e.to = a[1];
	    else if (strcmp(a[0], "fromport") == 0) {
		if (!cp_integer(a[1], &e.fromport) && e.fromport >= 0)
		    errh->lerror(landmark, "bad 'fromport' attribute; expected port number");
	    } else if (strcmp(a[0], "toport") == 0) {
		if (!cp_integer(a[1], &e.toport) && e.toport >= 0)
		    errh->lerror(landmark, "bad 'fromport' attribute; expected port number");
	    }

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
	delete xs;
    }
}

}

static void
process(const char *infile, bool file_is_expr, const char *outfile,
	ErrorHandler *errh)
{
    String contents;
    if (file_is_expr)
	contents = infile;
    else {
	int before = errh->nerrors();
	contents = file_string(infile, errh);
	if (!contents && errh->nerrors() > before)
	    return;
    }
    
    XML_Parser parser = XML_ParserCreateNS(0, '|');
    XML_SetElementHandler(parser, start_element_handler, end_element_handler);
    xml_errh = errh;
    xml_file = filename_landmark(infile, file_is_expr) + String(":");

    if (XML_Parse(parser, contents.data(), contents.length(), 1) == 0) {
	xml_error(parser, "XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
	return;
    }
    
    RouterT *r = read_router(infile, file_is_expr, errh);
    if (!r)
	return;
    //r->flatten(errh);

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
	    // don't complain if only a single driver works
	    if ((driver_mask & (driver_mask - 1)) != 0
		&& !emap.driver_indifferent(r, driver_mask, errh))
		errh->warning("configuration not indifferent to driver, picking %s\n(You might want to specify a driver explicitly.)", Driver::name(driver));
	}
    } else if (!emap.driver_compatible(r, driver))
	errh->warning("configuration not compatible with %s driver", Driver::name(driver));

    emap.set_driver(driver);
    ElementMap::push_default(&emap);

    fprintf(outf, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n\
<!DOCTYPE configuration SYSTEM \"http://www.icir.org/kohler/clickconfig.dtd\">\n\
<configuration>\n");
    generate_router(r, outf, "", true, errh);
    fprintf(outf, "</configuration>\n");
    
    ElementMap::pop_default();
    
    // close files, return
    if (outf != stdout)
	fclose(outf);
    delete r;
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
`Click2xml' reads a Click router configuration and outputs an XML file\n\
corresponding to that configuration.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -e, --expression EXPR       Use EXPR as router configuration.\n\
  -o, --output FILE           Write HTML output to FILE.\n\
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
