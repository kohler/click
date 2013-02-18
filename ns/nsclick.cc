/*
 * click.cc -- user-level Click main program
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>

#include <click/lexer.hh>
#include <click/routerthread.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <click/clp.h>
#include <click/archive.hh>
#include <click/glue.hh>
#include <click/driver.hh>
#include <click/userutils.hh>
#include <click/confparse.hh>
#include <click/master.hh>
#include <click/simclick.h>
#include <click/handlercall.hh>
#include "elements/standard/quitwatcher.hh"
#include "elements/userlevel/controlsocket.hh"

CLICK_USING_DECLS

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define QUIT_OPT		304
#define OUTPUT_OPT		305
#define HANDLER_OPT		306
#define TIME_OPT		307
#define STOP_OPT		308
#define PORT_OPT		309
#define UNIX_SOCKET_OPT		310
#define NO_WARNINGS_OPT		311
#define WARNINGS_OPT		312
#define EXPRESSION_OPT		313


static simclick_node_t *cursimnode = NULL;

//
// XXX
// OK, this bit of code here should work fine as long as your simulator
// isn't multithreaded. If it is, there could be multiple threads stomping
// on each other and potentially causing subtle or unsubtle problems.
//
static void setsimstate(simclick_node_t *newstate) {
    cursimnode = newstate;
}


CLICK_DECLS

uint32_t click_random() {
    static bool sim_rng_support = simclick_sim_command(NULL, SIMCLICK_SUPPORTS, SIMCLICK_GET_RANDOM_INT);
    if (sim_rng_support && cursimnode) {
        uint32_t x;
        simclick_sim_command(cursimnode, SIMCLICK_GET_RANDOM_INT, &x, (uint32_t) CLICK_RAND_MAX);
        return x;
    }
#if HAVE_RANDOM && CLICK_RAND_MAX == RAND_MAX
    // See also click_random() in include/click/glue.hh
    return random();
#else
    return rand();
#endif
}

CLICK_ENDDECLS


// functions for packages

extern "C" {

int simclick_click_create(simclick_node_t *simnode, const char* router_file) {
    static bool didinit = false;

    setsimstate(simnode);

    if (!didinit) {
	click_static_initialize();
	didinit = true;
    }

    bool warnings = true;

    // lex
    ErrorHandler *errh = ErrorHandler::default_handler();
    int before = errh->nerrors();

    // Get the defines from the simulator, if supported.
    if (simclick_sim_command(simnode, SIMCLICK_SUPPORTS, SIMCLICK_GET_DEFINES)) {
        size_t defines_size = 512;
        char *defines = (char *) malloc(defines_size);
        if ((simclick_sim_command(simnode, SIMCLICK_GET_DEFINES, defines, &defines_size) == -1)) {
            // Our buffer was too small, resize and try again.
            defines = (char *) realloc(defines, defines_size);
            simclick_sim_command(simnode, SIMCLICK_GET_DEFINES, defines, &defines_size);
        }

        // Process defines for click file parsing.
        size_t defines_offset = 0;
        while (defines_offset < defines_size) {
            char *key = defines + defines_offset;
            char *value = key + strlen(key) + 1;
            defines_offset += (size_t) (value + strlen(value) + 1 - defines);
            if (!click_lexer()->global_scope().define(key, value, false)) {
                errh->error("parameter %s multiply defined", key);
            }
        }
        free(defines);
    }

    Router *r = click_read_router(router_file, false, errh, false);
    click_lexer()->global_scope().clear();
    simnode->clickinfo = r;
    if (!r)
	return errh->fatal("%s: not a valid router", router_file);
    r->master()->initialize_ns(simnode);
    if (r->nelements() == 0 && warnings)
	errh->warning("%s: configuration has no elements", router_file);
    if (errh->nerrors() != before || r->initialize(errh) < 0)
	return errh->fatal("%s: errors prevent router from initializing", router_file);

    r->activate(errh);
    return 0;
}

/*
 * XXX Need to actually implement this a little more intelligenetly...
 */
void simclick_click_run(simclick_node_t *simnode) {
  setsimstate(simnode);
  //fprintf(stderr,"Hey! Need to implement simclick_click_run!\n");
  // not right - mostly smoke testing for now...
  Router* r = (Router *) simnode->clickinfo;
  if (r) {
    r->master()->thread(0)->driver();
  } else {
    click_chatter("simclick_click_run: call with null router");
  }
}

void simclick_click_kill(simclick_node_t *simnode) {
  //fprintf(stderr,"Hey! Need to implement simclick_click_kill!\n");
  setsimstate(simnode);
  Router *r = (Router *) simnode->clickinfo;
  if (r) {
    delete r;
    simnode->clickinfo = 0;
  } else {
    click_chatter("simclick_click_kill: call with null router");
  }
}

int simclick_gettimeofday(struct timeval* tv) {
    if (cursimnode) {
	*tv = cursimnode->curtime;
	return 0;
    } else {
	tv->tv_sec = 0;
	tv->tv_usec = 0;
	fprintf(stderr,"Hey! Called simclick_gettimeofday without simstate set!\n");
	return -1;
  }
}

int simclick_click_send(simclick_node_t *simnode,
			int ifid,int type,const unsigned char* data,int len,
			simclick_simpacketinfo* pinfo) {
  setsimstate(simnode);
  int result = 0;
  Router* r = (Router *) simnode->clickinfo;
  if (r) {
    r->sim_incoming_packet(ifid,type,data,len,pinfo);
    r->master()->thread(0)->driver();
  }
  else {
    click_chatter("simclick_click_send: called with null router");
    result = -1;
  }
  return result;
}

char* simclick_click_read_handler(simclick_node_t *simnode,
				  const char* elementname,
				  const char* handlername,
				  SIMCLICK_MEM_ALLOC memalloc,
				  void* memparam) {
    Router *r = (Router *) simnode->clickinfo;
    if (!r) {
      click_chatter("simclick_click_read_handler: call with null router");
      return 0;
    }
    setsimstate(simnode);
    String hdesc = String(elementname) + "." + String(handlername);
    ErrorHandler *errh = ErrorHandler::default_handler();
    int before = errh->nerrors();
    String result = HandlerCall::call_read(hdesc, r->root_element(), errh);
    if (!result && errh->nerrors() != before)
	return 0;
    char *rstr;
    if (memalloc)
	rstr = (char *) memalloc(result.length() + 1, memparam);
    else
	rstr = (char *) malloc(result.length() + 1);
    if (rstr) {
	memcpy(rstr, result.data(), result.length());
	rstr[result.length()] = 0;
    }
    return rstr;
}

int simclick_click_write_handler(simclick_node_t *simnode,
				 const char* elementname,
				 const char* handlername,
				 const char* writestring) {
    Router *r = (Router *) simnode->clickinfo;
    if (!r) {
      click_chatter("simclick_click_write_handler: call with null router");
      return -3;
    }
    setsimstate(simnode);
    String hdesc = String(elementname) + "." + String(handlername);
    return HandlerCall::call_write(hdesc, String(writestring), r->root_element(), ErrorHandler::default_handler());
}

int simclick_click_command(simclick_node_t *, int cmd, ...)
{
    va_list val;
    va_start(val, cmd);
    int r;

    if (cmd == SIMCLICK_VERSION)
	r = 0;
    else if (cmd == SIMCLICK_SUPPORTS) {
	int othercmd = va_arg(val, int);
	r = othercmd >= SIMCLICK_VERSION && othercmd <= SIMCLICK_SUPPORTS;
    } else
	r = 1;

    va_end(val);
    return r;
}

}
