// -*- c-basic-offset: 4 -*-
/*
 * config.cc -- parsing and installing configurations
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000-2002 Mazu Networks, Inc.
 * Copyright (c) 2001-2002 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
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
#include "modulepriv.hh"

#include <click/straccum.hh>
#include <click/router.hh>
#include <click/lexer.hh>

static String *current_config = 0;
uint32_t click_config_generation;


/*************************** Parsing configurations **************************/

static Lexer *lexer = 0;

extern "C" int
click_add_element_type(const char *name, Element *(*factory)(uintptr_t), uintptr_t thunk, struct module *module)
{
    return lexer->add_element_type(name, factory, thunk, module);
}

extern "C" int
click_add_element_type_stable(const char *name, Element *(*factory)(uintptr_t), uintptr_t thunk, struct module *module)
{
    return lexer->add_element_type(String::make_stable(name), factory, thunk, module);
}

extern "C" void
click_remove_element_type(int i)
{
    lexer->remove_element_type(i);
}

static String
read_classes(Element *, void *)
{
    Vector<String> v;
    lexer->element_type_names(v);
    StringAccum sa;
    for (int i = 0; i < v.size(); i++)
	sa << v[i] << "\n";
    return sa.take_string();
}


class LinuxModuleLexerExtra : public LexerExtra { public:
    LinuxModuleLexerExtra() { }
    void require(String type, String value, ErrorHandler *errh);
};

void
LinuxModuleLexerExtra::require(String type, String value, ErrorHandler *errh)
{
    if (type.equals("package", 7) && !click_has_provision(value.c_str()))
	errh->error("unsatisfied requirement %<%s%>", value.c_str());
}

static Router *
parse_router(String s)
{
    LinuxModuleLexerExtra lextra;
    int cookie = lexer->begin_parse(s, "line ", &lextra, click_logged_errh);
    while (!lexer->ydone())
	lexer->ystep();
    Router *r = lexer->create_router(click_master);
    lexer->end_parse(cookie);
    return r;
}


/*********************** Installing and killing routers **********************/

static void
install_router(const String &config, Router *r)
{
    click_config_generation++;
    click_router = r;
    if (click_router)
	click_router->use();
    *current_config = config;
}

static void
kill_router()
{
    if (click_router) {
	click_config_generation++;
	click_router->unuse();
	click_router = 0;
    }
}


/******************************* Handlers ************************************/
/*         Must be called with the click configuration write locked!         */

static int
swap_config(const String &s)
{
    kill_router();
    if (Router *router = parse_router(s)) {
	if (router->initialize(click_logged_errh) >= 0)
	    router->activate(click_logged_errh);
	install_router(s, router);
	return (router->initialized() ? 0 : -EINVAL);
    } else {
	install_router(s, 0);
	return -EINVAL;
    }
}

static int
hotswap_config(const String &s)
{
    int before_errors = click_logged_errh->nerrors();
    Router *router = parse_router(s);
    if (!router)
	return -EINVAL;

    // register hotswap router on new router
    if (click_router && click_router->initialized())
	router->set_hotswap_router(click_router);

    if (click_logged_errh->nerrors() == before_errors
	&& router->initialize(click_logged_errh) >= 0) {
	click_config_generation++;
	router->activate(click_logged_errh);
	kill_router();
	install_router(s, router);
    } else
	delete router;

    return 0;
}

static int
write_config(const String &s, Element *, void *thunk, ErrorHandler *)
{
    click_clear_error_log();
    int retval = (thunk ? hotswap_config(s) : swap_config(s));
    return retval;
}


/********************** Initialization and cleanup ***************************/

extern void click_export_elements();
extern void click_unexport_elements();

void
click_init_config()
{
    lexer = new Lexer;

    Router::add_read_handler(0, "classes", read_classes, 0);
    Router::add_write_handler(0, "config", write_config, 0);
    Router::add_write_handler(0, "hotconfig", write_config, (void *)1);
    Router::set_handler_flags(0, "config", HANDLER_WRITE_UNLIMITED | Handler::h_raw);
    Router::set_handler_flags(0, "hotconfig", HANDLER_WRITE_UNLIMITED | Handler::h_raw);

    click_config_generation = 1;
    current_config = new String;

    click_export_elements();
}

void
click_cleanup_config()
{
    kill_router();
    click_unexport_elements();
    delete current_config;
    delete lexer;
    current_config = 0;
    lexer = 0;
}
