// -*- c-basic-offset: 4; related-file-name: "../../lib/driver.cc" -*-
#ifndef CLICK_DRIVER_HH
#define CLICK_DRIVER_HH
#include <click/package.hh>

#define CLICK_DEFAULT_PROVIDES	/* nada */

#ifdef CLICK_USERLEVEL
CLICK_DECLS

void click_static_initialize();
void click_static_cleanup();

Router *click_read_router(String filename, bool is_expr, ErrorHandler * = 0, bool initialize = true);

CLICK_ENDDECLS
#endif

#endif
