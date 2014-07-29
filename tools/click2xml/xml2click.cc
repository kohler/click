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
#include <click/args.hh>
#include <click/clp.h>
#include "toolutils.hh"
#include "processingt.hh"
#include "elementmap.hh"
#include "xml2click.hh"

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define OUTPUT_OPT		305
#define FLATTEN_OPT		306

static const Clp_Option options[] = {
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ValString, 0 },
    { "expression", 'e', EXPRESSION_OPT, Clp_ValString, 0 },
    { "file", 'f', ROUTER_OPT, Clp_ValString, 0 },
    { "flatten", 'F', FLATTEN_OPT, 0, Clp_Negate },
    { "help", 0, HELP_OPT, 0, 0 },
    { "output", 'o', OUTPUT_OPT, Clp_ValString, 0 },
    { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;

static ErrorHandler *xml_errh;
static String xml_file;

static bool flatten = false;

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
    xml_errh->xmessage(xml_landmark(parser), ErrorHandler::e_error, format, val);
    va_end(val);
    return -EINVAL;
}


CxConfig::CxConfig(CxConfig *enclosing, const String &xml_landmark)
    : _enclosing(enclosing), _depth(enclosing ? enclosing->_depth + 1 : 0),
      _filled(false),
      _xml_landmark(xml_landmark),
      _decl_ninputs(-1), _decl_noutputs(-1), _decl_nformals(-1),
      _type(0), _router(0), _completing(false)
{
}

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
static HashTable<String, int> class_id_map(-1);
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
	    if (!IntArg().parse(a[1], e.ninputs))
		errh->lerror(landmark, "'ninputs' attribute must be an integer");
	} else if (strcmp(a[0], "noutputs") == 0) {
	    if (!IntArg().parse(a[1], e.noutputs))
		errh->lerror(landmark, "'noutputs' attribute must be an integer");
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
	    if (!IntArg().parse(a[1], e.fromport) && e.fromport >= 0)
		errh->lerror(landmark, "'fromport' should be port number");
	} else if (strcmp(a[0], "toport") == 0) {
	    if (!IntArg().parse(a[1], e.toport) && e.toport >= 0)
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

    CxConfig *nc = new CxConfig(xstack.back(), landmark);

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
	class_id_map.set(nc->_id, classes.size());
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
    cx->_is_synonym = true;
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
	if (strcmp(a[0], "overloadclassname") == 0) {
	    if (!cp_is_click_id(a[1]))
		errh->lerror(landmark, "'overloadclassname' attribute not a valid Click identifier");
	    cx->_prev_class_name = a[1];
	} else if (strcmp(a[0], "overloadclassid") == 0)
	    cx->_prev_class_id = a[1];
	else if (strcmp(a[0], "ninputs") == 0) {
	    if (!IntArg().parse(a[1], cx->_decl_ninputs))
		errh->lerror(landmark, "'ninputs' attribute must be an integer");
	} else if (strcmp(a[0], "noutputs") == 0) {
	    if (!IntArg().parse(a[1], cx->_decl_noutputs))
		errh->lerror(landmark, "'noutputs' attribute must be an integer");
	} else if (strcmp(a[0], "nformals") == 0) {
	    if (!IntArg().parse(a[1], cx->_decl_nformals))
		errh->lerror(landmark, "'noutputs' attribute must be an integer");
	}
    // XXX nformals etc.

    if (cx->_prev_class_name && cx->_prev_class_id) {
	errh->lerror(landmark, "conflicting attributes 'classname' and 'classid'");
	cx->_prev_class_name = String();
    }

    cx->_filled = true;
    cx->_is_synonym = false;
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
    String name, key;
    for (const XML_Char **a = attrs; *a; a += 2)
	if (strcmp(a[0], "name") == 0) {
	    name = a[1];
	    if (!cp_is_word(name))
		errh->lerror(landmark, "'name' should be formal name");
	} else if (strcmp(a[0], "number") == 0) {
	    if (!IntArg().parse(a[1], number) || number < 0)
		errh->lerror(landmark, "'number' should be formal argument position");
	} else if (strcmp(a[0], "key") == 0) {
	    key = a[1];
	    if (!cp_is_word(key))
		errh->lerror(landmark, "'key' should be formal keyword");
	}

    while (xstack.back()->_formals.size() <= number) {
	xstack.back()->_formals.push_back(String());
	xstack.back()->_formal_types.push_back(String());
    }
    if (xstack.back()->_formals[number])
	errh->lerror(landmark, "formal parameter %d already defined as '$%s'", number, xstack.back()->_formals[number].c_str());
    else {
	xstack.back()->_formals[number] = name;
	xstack.back()->_formal_types[number] = key;
    }
    return CX_IN_EMPTY;
}


extern "C" {

static void
start_element_handler(void *v, const XML_Char *name, const XML_Char **attrs)
{
    XML_Parser parser = (XML_Parser)v;
    CxState next_state = CX_ERROR;

    // handle XML namespaces
    if (strncmp(name, "http://www.lcdf.org/click/xml/|", 31) == 0)
	name += 31;

    if (strcmp(name, "configuration") == 0) {
	String landmark = xml_landmark(parser);
	if (xstack.size())
	    xml_errh->lerror(landmark, "additional configuration section ignored");
	else {
	    xstack.push_back(new CxConfig(0, landmark));
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
    int which = class_id_map.get(id);
    if (which < 0) {
	errh->lerror(xml_landmark, "no such element class '%s'", id.c_str());
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
	return errh->lerror(_xml_landmark, "circular definition of elementclass '%s'", readable_name().c_str());
    _completing = true;

    ContextErrorHandler cerrh(errh, "In definition of elementclass %<%s%> (id %<%s%>):", _name.c_str(), _id.c_str());
    cerrh.set_context_landmark(_xml_landmark);
    int before_nerrors = cerrh.nerrors();

    // get previous class
    ElementClassT *prev_class;
    if (_prev_class_id)
	prev_class = ::complete_elementclass(_prev_class_id, _xml_landmark, &cerrh);
    else if (_prev_class_name)
	prev_class = ElementClassT::base_type(_prev_class_name);
    else
	prev_class = 0;

    // get enclosing scope
    RouterT *enclosing_type = _enclosing->router(errh);

    // check for synonym or empty
    if (!_filled)		// error already reported
	return 0;
    if (_is_synonym && prev_class) {
	_type = new SynonymElementClassT(_name, prev_class, enclosing_type);
	_type->use();
	enclosing_type->add_declared_type(_type, true);
	return 0;
    }

    // otherwise, compound
    assert(_enclosing);
    _type = _router = new RouterT(_name, LandmarkT(_landmark ? _landmark : _xml_landmark), enclosing_type);
    _type->use();
    _router->use();
    enclosing_type->add_declared_type(_type, true);
    enclosing_type->check();
    _router->set_overload_type(prev_class);

    // handle formals
    HashTable<String, int> formal_map(-1);
    int formal_state = 0;
    for (int i = 0; i < _formals.size(); i++)
	if (!_formals[i])
	    cerrh.lerror(_xml_landmark, "definition missing for formal %d", i);
	else if (_formals[i][0] != '$')
	    cerrh.lerror(_xml_landmark, "formal %d (%<%s%>) does not begin with %<$%>", i, _formals[i].c_str());
	else if (_router->add_formal(_formals[i], _formal_types[i]) < 0)
	    cerrh.lerror(_xml_landmark, "redeclaration of formal %<%s%>", _formals[i].c_str());
	else {
	    if ((!_formal_types[i] && formal_state == 1)
		|| (_formal_types[i] == "__REST__" && i != _formals.size() - 1))
		cerrh.lerror(_xml_landmark, "formals out of order\n(The correct order is %<[positional], [keywords], [__REST__]%>.)");
	    if (_formal_types[i])
		formal_state = 1;
	}
    if (_decl_nformals >= 0 && _formals.size() != _decl_nformals)
	cerrh.lerror(_xml_landmark, "<formal> count and 'nformals' attribute disagree");

    // handle elements
    if (complete(&cerrh) < 0)
	return -1;

    // finally, finish elementclass
    if (_router->finish_type(&cerrh) < 0)
	return -1;
    if (_decl_ninputs >= 0 && _router->ninputs() != _decl_ninputs)
	cerrh.lerror(_xml_landmark, "input port count and 'ninputs' attribute disagree");
    if (_decl_noutputs >= 0 && _router->noutputs() != _decl_noutputs)
	cerrh.lerror(_xml_landmark, "output port count and 'noutputs' attribute disagree");
    return (cerrh.nerrors() == before_nerrors ? 0 : -1);
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

    // set up elements
    for (CxElement *e = _elements.begin(); e != _elements.end(); e++)
	if (ElementT *old_e = r->element(e->name)) {
	    int which = (intptr_t)(old_e->user_data());
	    ElementT::redeclaration_error(errh, "element", e->name, e->xml_landmark, _elements[which].xml_landmark);
	} else {
	    ElementClassT *eclass = 0;
	    if (e->class_id)
		eclass = ::complete_elementclass(e->class_id, e->xml_landmark, errh);
	    else if (e->class_name)
		eclass = ElementClassT::base_type(e->class_name);
	    ElementT *ne = r->get_element(e->name, (eclass ? eclass : ElementClassT::base_type("Error")), e->config, LandmarkT(e->landmark ? e->landmark : e->xml_landmark));
	    ne->set_user_data(e - _elements.begin());
	}

    // set up connections
    for (CxConnection *c = _connections.begin(); c != _connections.end(); c++) {
	ElementT *frome = r->element(c->from);
	if (!frome) {
	    errh->lerror(c->xml_landmark, "undeclared element '%s' (first use this block)", c->from.c_str());
	    frome = r->get_element(c->from, ElementClassT::base_type("Error"), String(), LandmarkT(c->xml_landmark));
	}
	ElementT *toe = r->element(c->to);
	if (!toe) {
	    errh->lerror(c->xml_landmark, "undeclared element '%s' (first use this block)", c->to.c_str());
	    toe = r->get_element(c->to, ElementClassT::base_type("Error"), String(), LandmarkT(c->xml_landmark));
	}
	r->add_connection(frome, c->fromport, toe, c->toport, LandmarkT(c->xml_landmark));
    }

    // check elements' ninputs and noutputs
    for (CxElement *e = _elements.begin(); e != _elements.end(); e++)
	if (e->ninputs >= 0 || e->noutputs >= 0)
	    if (ElementT *et = r->element(e->name)) {
		if (e->ninputs >= 0 && et->ninputs() != e->ninputs)
		    errh->lerror(et->landmark(), "'%s' input port count and 'ninputs' attribute disagree", e->name.c_str());
		if (e->noutputs >= 0 && et->noutputs() != e->noutputs)
		    errh->lerror(et->landmark(), "'%s' output port count and 'noutputs' attribute disagree", e->name.c_str());
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
	xstack.back()->router(errh)->check();
    }

    // flatten router if appropriate
    if (errh->nerrors() == before && ::flatten)
	xstack.back()->router(errh)->flatten(errh);

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
Try '%s --help' for more information.\n",
	    program_name, program_name);
}

void
usage()
{
    printf("\
'Xml2click' reads an XML description of a Click router configuration and\n\
outputs a Click-language file corresponding to that configuration.\n\
\n\
Usage: %s [OPTION]... [XMLFILE]\n\
\n\
Options:\n\
  -f, --file FILE             Read router configuration from FILE.\n\
  -e, --expression EXPR       Use EXPR as XML router configuration.\n\
  -o, --output FILE           Write output to FILE.\n\
  -F, --flatten               Flatten configuration before output.\n\
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
	    set_clickpath(clp->vstr);
	    break;

	  case ROUTER_OPT:
	  case EXPRESSION_OPT:
	  case Clp_NotOption:
	    if (router_file) {
		p_errh->error("router configuration specified twice");
		goto bad_option;
	    }
	    router_file = clp->vstr;
	    file_is_expr = (opt == EXPRESSION_OPT);
	    break;

	  case OUTPUT_OPT:
	    if (output_file) {
		p_errh->error("output file specified twice");
		goto bad_option;
	    }
	    output_file = clp->vstr;
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
    process(router_file, file_is_expr, output_file, errh);

    exit(errh->nerrors() > 0 ? 1 : 0);
}
