/*
 * proc_config.cc -- support /proc/click/config
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000-2002 Mazu Networks, Inc.
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
#include "modulepriv.hh"

#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>

static String *current_config = 0;
atomic_t click_config_generation;


static void
set_current_config(const String &s)
{
  *current_config = s;
  atomic_inc(&click_config_generation);
}

static int
hotswap_config(const String &s)
{
  int before_errors = kernel_errh->nerrors();
  Router *r = parse_router(s);
  if (!r)
    return -EINVAL;

  // XXX should we lock the kernel?
  
  if (kernel_errh->nerrors() == before_errors
      && r->initialize(kernel_errh) >= 0) {
    // perform hotswap
    if (current_router && current_router->initialized()) {
      // turn off all threads on current router before you take_state
      if (kill_current_router_threads() >= 0) {
	printk("<1>click: performing hotswap\n");
	r->take_state(current_router, kernel_errh);
      }
    }
    // install
    kill_current_router();
    install_current_router(r);
    set_current_config(s);
  } else
    delete r;
  
  return 0;
}

static int
swap_config(const String &s)
{
  set_current_config(s);
  kill_current_router();
  int before_errors = kernel_errh->nerrors();
  Router *router = parse_router(s);
  if (router) {
    router->preinitialize();
    router->initialize(kernel_errh);
    install_current_router(router);
    return router->initialized() ? 0 : -EINVAL;
  } else
    return -EINVAL;
}

static String
read_config(Element *, void *)
{
  return (current_config ? *current_config : String());
}

static int
write_config(const String &s, Element *, void *thunk, ErrorHandler *)
{
  reset_proc_click_errors();
  int retval = (thunk ? hotswap_config(s) : swap_config(s));
  return retval;
}


void
init_proc_click_config()
{
  Router::add_global_read_handler("config", read_config, 0);
  Router::add_global_write_handler("config", write_config, 0);
  Router::add_global_write_handler("hotconfig", write_config, (void *)1);
  Router::change_handler_flags(0, -1, "config", 0, HANDLER_REREAD | HANDLER_WRITE_UNLIMITED);
  Router::change_handler_flags(0, -1, "hotconfig", 0, HANDLER_WRITE_UNLIMITED);
  
  atomic_set(&click_config_generation, 1);
  current_config = new String;
}

void
cleanup_proc_click_config()
{
  delete current_config;
}
