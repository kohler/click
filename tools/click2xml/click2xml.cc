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
#define FLATTEN_OPT		306

#define FIRST_DRIVER_OPT	1000
#define USERLEVEL_OPT		(1000 + Driver::USERLEVEL)
#define LINUXMODULE_OPT		(1000 + Driver::LINUXMODULE)
#define BSDMODULE_OPT		(1000 + Driver::BSDMODULE)

static const Clp_Option options[] = {
    { "bsdmodule", 'b', BSDMODULE_OPT, 0, 0 },
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ValString, 0 },
    { "expression", 'e', EXPRESSION_OPT, Clp_ValString, 0 },
    { "file", 'f', ROUTER_OPT, Clp_ValString, 0 },
    { "flatten", 'F', FLATTEN_OPT, 0, Clp_Negate },
    { "help", 0, HELP_OPT, 0, 0 },
    { "kernel", 'k', LINUXMODULE_OPT, 0, 0 },
    { "linuxmodule", 'l', LINUXMODULE_OPT, 0, 0 },
    { "output", 'o', OUTPUT_OPT, Clp_ValString, 0 },
    { "userlevel", 0, USERLEVEL_OPT, 0, 0 },
    { "version", 'v', VERSION_OPT, 0, 0 },
};

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

static HashTable<ElementClassT *, int> generated_types(0);

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
    if (c->primitive())
	fprintf(f, "%sclassname=\"%s\"", prefix, c->name().c_str());
    else
	fprintf(f, "%sclassid=\"c%p\"", prefix, c);
}

static void
print_landmark_attributes(FILE *f, const String &landmark)
{
    const char *colon = rfind(landmark.begin(), landmark.end(), ':');
    if (colon < landmark.end() && (colon < landmark.end() - 1 || colon[1] != '0'))
	fprintf(f, " file=\"%s\" line=\"%s\"", xml_quote(landmark.substring(landmark.begin(), colon)).c_str(), landmark.substring(colon + 1, landmark.end()).c_str());
}

static void generate_router(RouterT *, FILE *, String, ErrorHandler *);

static void
generate_type(ElementClassT *c, FILE *f, String indent, ErrorHandler *errh)
{
    if (!c || c->primitive() || generated_types.get(c) || c->tunnel())
	return;
    generated_types.set(c, 1);

    if (ElementClassT *older = c->overload_type())
	generate_type(older, f, indent, errh);

    fprintf(f, "%s<elementclass ", indent.c_str());
    if (c->name())
	fprintf(f, "classname=\"%s\" ", c->name().c_str());
    print_class_reference(f, c, "");

    if (SynonymElementClassT *synonym = c->cast_synonym()) {
	fprintf(f, ">\n%s  <synonym ", indent.c_str());
	print_class_reference(f, synonym->synonym_of(), "");
	fprintf(f, " />\n");
    } else if (RouterT *compound = c->cast_router()) {
	print_landmark_attributes(f, compound->landmark());
	fprintf(f, ">\n%s  <compound", indent.c_str());
	if (ElementClassT *prev = compound->overload_type()) {
	    fprintf(f, " ");
	    print_class_reference(f, prev, "overload");
	}
	fprintf(f, " ninputs=\"%d\" noutputs=\"%d\" nformals=\"%d\">\n",
		compound->ninputs(), compound->noutputs(), compound->nformals());

	String new_indent = add_indent(indent, 4);
	for (int i = 0; i < compound->nformals(); i++) {
	    assert(compound->formal_name(i));
	    fprintf(f, "%s<formal number=\"%d\" name=\"%s\" ",
		    new_indent.c_str(), i, compound->formal_name(i).c_str());
	    if (compound->formal_type(i))
		fprintf(f, "key=\"%s\" ", compound->formal_type(i).c_str());
	    fprintf(f, "/>\n");
	}
	generate_router(compound->cast_router(), f, new_indent, errh);

	fprintf(f, "%s  </compound>\n", indent.c_str());
    }

    fprintf(f, "%s</elementclass>\n", indent.c_str());
}

static void
generate_router(RouterT *r, FILE *f, String indent, ErrorHandler *errh)
{
    ProcessingT processing(r, ElementMap::default_map(), errh);

    Vector<ElementClassT *> declared_types;
    r->collect_locally_declared_types(declared_types);
    for (int i = 0; i < declared_types.size(); i++)
	generate_type(declared_types[i], f, indent, errh);

    for (RouterT::iterator e = r->begin_elements(); e; e++)
	if (!e->tunnel()) {
	    fprintf(f, "%s<element name=\"%s\" ", indent.c_str(), e->name_c_str());
	    print_class_reference(f, e->type(), "");
	    print_landmark_attributes(f, e->landmark());
	    fprintf(f, " ninputs=\"%d\" noutputs=\"%d\"",
		    e->ninputs(), e->noutputs());
	    if (e->ninputs() || e->noutputs())
		fprintf(f, " processing=\"%s\"", processing.processing_code(e.get()).c_str());
	    if (e->config())
		fprintf(f, " config=\"%s\"", xml_quote(e->config()).c_str());
	    fprintf(f, " />\n");
	}

    // print connections
    for (RouterT::conn_iterator it = r->begin_connections();
	 it != r->end_connections(); ++it) {
	int p = processing.output_processing(it->from());
	fprintf(f, "%s<connection from=\"%s\" fromport=\"%d\" to=\"%s\" toport=\"%d\" processing=\"%c\" />\n",
		indent.c_str(),
		it->from_element()->name_c_str(), it->from_port(),
		it->to_element()->name_c_str(), it->to_port(),
		ProcessingT::processing_letters[p]);
    }
}

static void
process(const char *infile, bool file_is_expr, bool flatten,
	const char *outfile, ErrorHandler *errh)
{
    RouterT *r = read_router(infile, file_is_expr, errh);
    if (!r)
	return;
    if (flatten)
	r->flatten(errh);

    // open output file
    FILE *outf = open_output_file(outfile, errh);
    if (!outf) {
	delete r;
	return;
    }

    // get element map and processing
    ElementMap emap;
    emap.parse_all_files(r, CLICK_DATADIR, errh);

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
<!DOCTYPE configuration SYSTEM \"http://www.pdos.lcs.mit.edu/click/clickconfig.dtd\">\n\
<configuration xmlns=\"http://www.lcdf.org/click/xml/\">\n");
    generate_router(r, outf, "", errh);
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
Try '%s --help' for more information.\n",
	    program_name, program_name);
}

void
usage()
{
    printf("\
'Click2xml' reads a Click router configuration and outputs an XML file\n\
corresponding to that configuration.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -e, --expression EXPR       Use EXPR as router configuration.\n\
  -o, --output FILE           Write XML output to FILE.\n\
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
    bool flatten = false;

    while (1) {
	int opt = Clp_Next(clp);
	switch (opt) {

	  case HELP_OPT:
	    usage();
	    exit(0);
	    break;

	  case VERSION_OPT:
	    printf("click2xml (Click) %s\n", CLICK_VERSION);
	    printf("Copyright (c) 2002 International Computer Science Institute\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
	    exit(0);
	    break;

	  case CLICKPATH_OPT:
	    set_clickpath(clp->vstr);
	    break;

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

	  case FLATTEN_OPT:
	    flatten = !clp->negated;
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
    process(router_file, file_is_expr, flatten, output_file, errh);

    exit(errh->nerrors() > 0 ? 1 : 0);
}
