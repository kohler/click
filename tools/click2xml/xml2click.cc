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

static Clp_Option options[] = {
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
    { "expression", 'e', EXPRESSION_OPT, Clp_ArgString, 0 },
    { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
    { "help", 0, HELP_OPT, 0, 0 },
    { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
    { "version", 'v', VERSION_OPT, 0, 0 },
};

static String::Initializer string_initializer;
static const char *program_name;

static ErrorHandler *xml_errh;
static String xml_file;

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

struct CxConfig {
    Vector<CxElement> _elements;
    Vector<CxConnection> _connections;
    
    Vector<String> _formals;
    CxConfig *_enclosing;
    int _depth;
    String _name;
    String _id;
    String _prev_class_name;
    String _prev_class_id;
    bool _is_synonym;
    bool _filled;
    String _landmark;
    String _xml_landmark;

    ElementClassT *_type;
    bool _completing;
    RouterT *_router;
    
    CxConfig(CxConfig *enclosing, int depth, const String &xml_landmark)
	: _enclosing(enclosing), _depth(depth), _filled(false),
	  _xml_landmark(xml_landmark), _type(0), _completing(false),
	  _router(0) { }
    ~CxConfig();

    String readable_name() const;
    RouterT *router(ErrorHandler *);
    int complete_elementclass(ErrorHandler *);
    int complete(ErrorHandler *);
};

CxConfig::~CxConfig()
{
    if (_type)
	_type->unuse();
    if (_router)
	_router->unuse();
}

String
CxConfig::readable_name() const
{
    return (_name ? _name : String("<anonymous>"));
}


enum CxState { CX_NONE, CX_CONFIGURATION, CX_ELEMENTCLASS, CX_COMPOUND,
	       CX_IN_EMPTY, CX_ERROR };
static Vector<CxState> xstates;
static Vector<CxConfig *> xstack;
static HashMap<String, int> class_id_map(-1);
static Vector<CxConfig *> classes;


static CxState
do_element(XML_Parser parser, const XML_Char **attrs, ErrorHandler *errh)
{
    String landmark = xml_landmark(parser);
    
    if (xstates.back() != CX_CONFIGURATION && xstates.back() != CX_COMPOUND) {
	if (xstates.back() != CX_ERROR)
	    errh->lerror(landmark, "<element> tag outside of <configuration>");
	return CX_ERROR;
    }
    
    CxElement e;
    e.xml_landmark = landmark;
    bool ok = true;
    
    String file, line;
    for (const XML_Char **a = attrs; *a; a += 2)
	if (strcmp(a[0], "name") == 0) {
	    if (!cp_is_click_id(a[1]))
		errh->lerror(landmark, "'name' attribute not a Click identifier");
	    e.name = a[1];
	} else if (strcmp(a[0], "classname") == 0) {
	    if (!cp_is_click_id(a[1]))
		errh->lerror(landmark, "'classname' attribute not a Click identifier");
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
		errh->lerror(landmark, "'ninputs' attribute not an integer");
	} else if (strcmp(a[0], "noutputs") == 0) {
	    if (!cp_integer(a[1], &e.ninputs))
		errh->lerror(landmark, "'noutputs' attribute not an integer");
	}
    
    if (file && line)
	e.landmark = file + ":" + line;
    else if (file)
	e.landmark = file;
    else if (line)
	e.landmark = "line " + line;

    if (e.class_name && e.class_id) {
	errh->lerror(landmark, "conflicting attributes 'classname' and 'classid'");
	e.class_name = String();
    } else if (!e.class_name && !e.class_id) {
	errh->lerror(landmark, "element declared without a class");
	ok = false;
    }
    if (!e.name) {
	errh->lerror(landmark, "element declared without a name");
	ok = false;
    }

    if (ok)
	xstack.back()->_elements.push_back(e);
    return CX_IN_EMPTY;
}

static CxState
do_connection(XML_Parser parser, const XML_Char **attrs, ErrorHandler *errh)
{
    String landmark = xml_landmark(parser);
    
    if (xstates.back() != CX_CONFIGURATION && xstates.back() != CX_COMPOUND) {
	if (xstates.back() != CX_ERROR)
	    errh->lerror(landmark, "<connection> tag meaningless outside of <configuration>");
	return CX_ERROR;
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
		errh->lerror(landmark, "'fromport' should be port number");
	} else if (strcmp(a[0], "toport") == 0) {
	    if (!cp_integer(a[1], &e.toport) && e.toport >= 0)
		errh->lerror(landmark, "'toport' should be port number");
	}

    if (!e.from || !e.to) {
	errh->lerror(landmark, "connection lacks 'from' or 'to' attribute");
	ok = false;
    }
    
    if (ok)
	xstack.back()->_connections.push_back(e);
    return CX_IN_EMPTY;
}

static CxState
do_start_elementclass(XML_Parser parser, const XML_Char **attrs, ErrorHandler *errh)
{
    String landmark = xml_landmark(parser);

    if (xstates.back() != CX_CONFIGURATION && xstates.back() != CX_COMPOUND) {
	if (xstates.back() != CX_ERROR)
	    errh->lerror(landmark, "<elementclass> tag outside of <configuration>");
	return CX_ERROR;
    }

    CxConfig *nc = new CxConfig(xstack.back(), xstack.size(), landmark);
    
    String file, line;
    for (const XML_Char **a = attrs; *a; a += 2)
	if (strcmp(a[0], "classname") == 0) {
	    if (!cp_is_click_id(a[1]))
		errh->lerror(landmark, "'classname' attribute not a valid Click identifier");
	    nc->_name = a[1];
	} else if (strcmp(a[0], "classid") == 0)
	    nc->_id = a[1];
	else if (strcmp(a[0], "file") == 0)
	    file = a[1];
	else if (strcmp(a[0], "line") == 0)
	    line = a[1];

    if (file && line)
	nc->_landmark = file + ":" + line;
    else if (file)
	nc->_landmark = file;
    else if (line)
	nc->_landmark = "line " + line;

    if (!nc->_id)
	errh->lerror(landmark, "element class declared without an ID");
    else
	class_id_map.insert(nc->_id, classes.size());
    classes.push_back(nc);
    
    xstack.push_back(nc);
    return CX_ELEMENTCLASS;
}

static CxState
do_synonym(XML_Parser parser, const XML_Char **attrs, ErrorHandler *errh)
{
    String landmark = xml_landmark(parser);
    
    if (xstates.back() != CX_ELEMENTCLASS) {
	if (xstates.back() != CX_ERROR)
	    errh->lerror(landmark, "<synonym> tag outside of <elementclass>");
	return CX_ERROR;
    } else if (xstack.back()->_filled) {
	errh->lerror(landmark, "element class already defined");
	return CX_ERROR;
    }

    CxConfig *cx = xstack.back();
    for (const XML_Char **a = attrs; *a; a += 2)
	if (strcmp(a[0], "classname") == 0) {
	    if (!cp_is_click_id(a[1]))
		errh->lerror(landmark, "'classname' attribute not a Click identifier");
	    cx->_prev_class_name = a[1];
	} else if (strcmp(a[0], "classid") == 0)
	    cx->_prev_class_id = a[1];

    if (cx->_prev_class_name && cx->_prev_class_id) {
	errh->lerror(landmark, "conflicting attributes 'classname' and 'classid'");
	cx->_prev_class_name = String();
    } else if (!cx->_prev_class_name && !cx->_prev_class_id)
	errh->lerror(landmark, "synonym refers to no other class");
    
    cx->_filled = true;
    return CX_IN_EMPTY;
}

static CxState
do_start_compound(XML_Parser parser, const XML_Char **attrs, ErrorHandler *errh)
{
    String landmark = xml_landmark(parser);
    
    if (xstates.back() != CX_ELEMENTCLASS) {
	if (xstates.back() != CX_ERROR)
	    errh->lerror(landmark, "<compound> tag outside of <elementclass>");
	return CX_ERROR;
    } else if (xstack.back()->_filled) {
	errh->lerror(landmark, "element class already defined");
	return CX_ERROR;
    }

    CxConfig *cx = xstack.back();
    for (const XML_Char **a = attrs; *a; a += 2)
	if (strcmp(a[0], "prevclassname") == 0) {
	    if (!cp_is_click_id(a[1]))
		errh->lerror(landmark, "'prevclassname' attribute not a valid Click identifier");
	    cx->_prev_class_name = a[1];
	} else if (strcmp(a[0], "prevclassid") == 0)
	    cx->_prev_class_id = a[1];
    // XXX nformals etc.

    if (cx->_prev_class_name && cx->_prev_class_id) {
	errh->lerror(landmark, "conflicting attributes 'classname' and 'classid'");
	cx->_prev_class_name = String();
    }

    cx->_filled = true;
    return CX_COMPOUND;
}

static CxState
do_formal(XML_Parser parser, const XML_Char **attrs, ErrorHandler *errh)
{
    String landmark = xml_landmark(parser);
    
    if (xstates.back() != CX_COMPOUND) {
	if (xstates.back() != CX_ERROR)
	    errh->lerror(landmark, "<formal> tag meaningless outside of <compound>");
	return CX_ERROR;
    }

    int number = -1;
    String name;
    for (const XML_Char **a = attrs; *a; a += 2)
	if (strcmp(a[0], "name") == 0) {
	    if (!cp_is_word(name))
		errh->lerror(landmark, "'name' should be formal name");
	    name = a[1];
	} else if (strcmp(a[0], "number") == 0) {
	    if (!cp_integer(a[1], &number) || number < 0)
		errh->lerror(landmark, "'number' should be formal argument position");
	}

    while (xstack.back()->_formals.size() < number)
	xstack.back()->_formals.push_back(String());
    if (xstack.back()->_formals[number])
	errh->lerror(landmark, "formal parameter %d already defined as '$%s'", number, xstack.back()->_formals[number].cc());
    else
	xstack.back()->_formals[number] = name;
    return CX_IN_EMPTY;
}


extern "C" {

static void
start_element_handler(void *v, const XML_Char *name, const XML_Char **attrs)
{
    XML_Parser parser = (XML_Parser)v;
    CxState next_state = CX_ERROR;
    
    if (strcmp(name, "configuration") == 0) {
	String landmark = xml_landmark(parser);
	if (xstack.size())
	    xml_errh->lerror(landmark, "additional configuration section ignored");
	else {
	    xstack.push_back(new CxConfig(0, 0, landmark));
	    next_state = CX_CONFIGURATION;
	}
	
    } else if (strcmp(name, "element") == 0)
	next_state = do_element(parser, attrs, xml_errh);
	
    else if (strcmp(name, "connection") == 0)
	next_state = do_connection(parser, attrs, xml_errh);
	
    else if (strcmp(name, "elementclass") == 0)
	next_state = do_start_elementclass(parser, attrs, xml_errh);
	
    else if (strcmp(name, "synonym") == 0)
	next_state = do_synonym(parser, attrs, xml_errh);
	
    else if (strcmp(name, "compound") == 0)
	next_state = do_start_compound(parser, attrs, xml_errh);

    else if (strcmp(name, "formal") == 0)
	next_state = do_formal(parser, attrs, xml_errh);
	
    else
	next_state = xstates.back();

    xstates.push_back(next_state);
}

static void
end_element_handler(void *v, const XML_Char *name)
{
    XML_Parser parser = (XML_Parser)v;
    
    if (strcmp(name, "elementclass") == 0) {
	if (xstates.back() == CX_ELEMENTCLASS) {
	    if (!xstack.back()->_filled)
		xml_errh->lerror(xml_landmark(parser), "elementclass tag not filled");
	    xstack.pop_back();
	}
    }

    xstates.pop_back();
}

}


static ElementClassT *
complete_elementclass(const String &id, const String &xml_landmark, ErrorHandler *errh)
{
    assert(id);
    int which = class_id_map[id];
    if (which < 0) {
	errh->lerror(xml_landmark, "no such element class `%s'", String(id).cc());
	return 0;
    } else {
	classes[which]->complete_elementclass(errh);
	return classes[which]->_type;
    }
}

int
CxConfig::complete_elementclass(ErrorHandler *errh)
{
    if (_type)			// already complete
	return 0;
    if (_completing)
	return errh->lerror(_xml_landmark, "circular definition of elementclass `%s'", readable_name().cc());

    _completing = true;

    // get referred-to class
    ElementClassT *prev_class;
    if (_prev_class_id)
	prev_class = ::complete_elementclass(_prev_class_id, _xml_landmark, errh);
    else if (_prev_class_name)
	prev_class = ElementClassT::default_class(_prev_class_name);
    else
	prev_class = 0;

    // check for synonym or empty
    if (!_filled)		// error already reported
	return 0;
    if (_is_synonym && prev_class) {
	_type = new SynonymElementClassT(_name, prev_class);
	return 0;
    }

    // otherwise, compound
    assert(_enclosing);
    RouterT *enclosing_scope = _enclosing->router(errh);
    CompoundElementClassT *c = new CompoundElementClassT(_name, prev_class, _depth, enclosing_scope, (_landmark ? _landmark : _xml_landmark));
    _type = c;
    _router = c->cast_router();

    // handle formals
    HashMap<String, int> formal_map(-1);
    for (int i = 0; i < _formals.size(); i++)
	if (!_formals.size())
	    errh->lerror(_xml_landmark, "definition missing for formal %d", i);
	else if (formal_map[_formals[i]] >= 0)
	    errh->lerror(_xml_landmark, "redeclaration of formal `$%s'", _formals[i].cc());
	else
	    c->add_formal(_formals[i]);

    // add to enclosing scope
    enclosing_scope->get_type(c);
    
    // handle elements
    return complete(errh);
}

RouterT *
CxConfig::router(ErrorHandler *errh)
{
    if (!_router) {
	if (_enclosing) {
	    assert(_filled && !_is_synonym);
	    complete_elementclass(errh);
	    if (!_router)
		return _enclosing->router(errh);
	} else
	    _router = new RouterT;
    }
    return _router;
}

int
CxConfig::complete(ErrorHandler *errh)
{
    RouterT *r = router(errh);
    if (!r)
	return -1;

    for (int i = 0; i < _elements.size(); i++) {
	CxElement &e = _elements[i];
	if (ElementT *old_e = r->elt(e.name)) {
	    int which = (int)(old_e->user_data());
	    ElementT::redeclaration_error(errh, "element", e.name, e.xml_landmark, _elements[which].xml_landmark);
	} else {
	    ElementClassT *eclass = 0;
	    if (e.class_id)
		eclass = ::complete_elementclass(e.class_id, e.xml_landmark, errh);
	    else if (e.class_name)
		eclass = r->get_type(e.class_name);
	    ElementT *ne = r->get_element(e.name, (eclass ? eclass : ElementClassT::default_class("Error")), e.config, (e.landmark ? e.landmark : e.xml_landmark));
	    ne->set_user_data((void *)i);
	}
    }

    for (int i = 0; i < _connections.size(); i++) {
	CxConnection &c = _connections[i];
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

    return 0;
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

    xstates.clear();
    xstates.push_back(CX_NONE);
    
    if (XML_Parse(parser, contents.data(), contents.length(), 1) == 0) {
	xml_error(parser, "XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
	return;
    }

    if (xstack.size() == 0 && errh->nerrors() == before)
	errh->lerror(xml_file, "no configuration section");

    // if no errors, resolve router
    if (errh->nerrors() == before) {
	for (int i = 0; i < classes.size(); i++)
	    classes[i]->complete_elementclass(errh);
	xstack.back()->complete(errh);
    }
    
    // if no errors, write output
    if (errh->nerrors() == before)
	write_router_file(xstack.back()->router(errh), outfile, errh);

    // delete state
    for (int i = 0; i < classes.size(); i++)
	delete classes[i];
    classes.clear();
    if (xstack.size())
	delete xstack[0];
    xstack.clear();
    xstates.clear();
    class_id_map.clear();
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
