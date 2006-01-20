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


//
// State for each simulated machine
//

class SimState {
public:
  Router *router;

  //String::Initializer crap_initializer;
  //Vector<String> packages;
  //String configuration_string;
  //Vector<String> handlers;

  SimState() {
    router = NULL;
  }

  static SimState* simmain(simclick_sim siminst,const char* router_file);
  static bool didinit_;
};

bool SimState::didinit_ = false;

static simclick_simstate* cursimclickstate = NULL;

//
// XXX
// OK, this bit of code here should work fine as long as your simulator
// isn't multithreaded. If it is, there could be multiple threads stomping
// on each other and potentially causing subtle or unsubtle problems.
//
static void setsimstate(simclick_simstate* newstate) {
  cursimclickstate = newstate;
}

static simclick_simstate* getsimstate() {
  return cursimclickstate;
}

static ErrorHandler *errh;

// functions for packages

static String::Initializer crap_initializer;
static String configuration_string;

#if 0
extern "C" int
click_add_element_type(const char *, Element *(*)(uintptr_t), uintptr_t)
{
  // if (name)
  //   return lexer->add_element_type(name, e);
  // else
  //   return lexer->add_element_type(e);
  fprintf(stderr,"Hey! Need to do click_add_element_type!\n");
  return 0;
}

extern "C" void
click_remove_element_type(int)
{
  //lexer->remove_element_type(which);
  fprintf(stderr,"Hey! Need to do click_remove_element_type!\n");
}

// global handlers for ControlSocket

enum {
  GH_VERSION, GH_LIST, GH_CLASSES, GH_CONFIG,
  GH_FLATCONFIG, GH_PACKAGES, GH_REQUIREMENTS
};

String
read_global_handler(Element *, void *thunk)
{
  StringAccum sa;

  switch (reinterpret_cast<int>(thunk)) {

   case GH_VERSION:
    return String(CLICK_VERSION "\n");

   case GH_LIST:
    return router->element_list_string();
  
   case GH_CLASSES: {
     Vector<String> v;
     lexer->element_type_names(v);
     for (int i = 0; i < v.size(); i++)
       sa << v[i] << "\n";
     return sa.take_string();
   }

   case GH_CONFIG:
    return configuration_string;

   case GH_FLATCONFIG:
    return router->flat_configuration_string();

   case GH_PACKAGES: {
     Vector<String> p;
     click_public_packages(p);
     for (int i = 0; i < p.size(); i++)
       sa << p[i] << "\n";
     return sa.take_string();
   }

   case GH_REQUIREMENTS: {
     const Vector<String> &v = router->requirements();
     for (int i = 0; i < v.size(); i++)
       sa << v[i] << "\n";
     return sa.take_string();
   }

   default:
    return "<error>\n";

  }
}

static int
stop_global_handler(const String &s, Element *, void *, ErrorHandler *)
{
  int n = 1;
  (void) cp_integer(cp_uncomment(s), &n);
  router->adjust_runcount(-n);
  return 0;
}


// report handler results

static int
call_read_handler(Element *e, String handler_name, Router *r,
		  bool print_name, ErrorHandler *errh)
{
  const Handler *rh = Router::handler(e, handler_name);
  String full_name = Handler::unparse_name(e, handler_name);
  if (!rh || !rh->visible())
    return errh->error("no `%s' handler", full_name.c_str());
  else if (!rh->read_visible())
    return errh->error("`%s' is a write handler", full_name.c_str());

  if (print_name)
    fprintf(stdout, "%s:\n", full_name.c_str());
  String result = rh->call_read(e);
  fputs(result.c_str(), stdout);
  if (print_name)
    fputs("\n", stdout);

  return 0;
}

static bool
expand_handler_elements(const String &pattern, const String &handler_name,
			Vector<Element *> &elements, Router *router)
{
  int nelem = router->nelements();
  bool any_elements = false;
  for (int i = 0; i < nelem; i++) {
    const String &id = router->ename(i);
    if (glob_match(id, pattern)) {
      any_elements = true;
      if (const Handler *h = Router::handler(router->element(i), handler_name))
	if (h->read_visible())
	  elements.push_back(router->element(i));
    }
  }
  return any_elements;
}

static int
call_read_handlers(Vector<String> &handlers, ErrorHandler *errh)
{
  Vector<Element *> handler_elements;
  Vector<String> handler_names;
  bool print_names = (handlers.size() > 1);
  int before = errh->nerrors();

  // expand handler names
  for (int i = 0; i < handlers.size(); i++) {
    const char *dot = find(handlers[i], '.');
    if (dot == handlers[i].end()) {
      call_read_handler(0, handlers[i], router, print_names, errh);
      continue;
    }
    
    String element_name = handlers[i].substring(handlers[i].begin(), dot);
    String handler_name = handlers[i].substring(dot + 1, handlers[i].end());

    Vector<Element *> elements;
    if (Element *e = router->find(element_name))
      elements.push_back(e);
    else if (expand_handler_elements(element_name, handler_name, elements, router))
      print_names = true;
    else
      errh->error("no element matching `%s'", element_name.c_str());

    for (int j = 0; j < elements.size(); j++)
      call_read_handler(elements[j], handler_name, router, print_names, errh);
  }

  return (errh->nerrors() == before ? 0 : -1);
}
#endif


// main

SimState*
SimState::simmain(simclick_sim siminst, const char *router_file)
{
  if (!didinit_) {
    click_static_initialize();
    errh = ErrorHandler::default_handler();
    didinit_ = true;
  }

  bool warnings = true;

  SimState* newstate = new SimState();

  // lex
  newstate->router = click_read_router(router_file, false, errh, false);
  if (!newstate->router)
    exit(1);
  
  newstate->router->master()->initialize_ns(siminst, (simclick_click)newstate);

  if (newstate->router->nelements() == 0 && warnings)
    errh->warning("%s: configuration has no elements", router_file);

  if (errh->nerrors() > 0 || newstate->router->initialize(errh) < 0)
    exit(1);

  newstate->router->activate(errh);
  return newstate;
}


simclick_click simclick_click_create(simclick_sim siminst,
				     const char* router_file,
				     simclick_simstate* startstate) {
  setsimstate(startstate);
  return SimState::simmain(siminst,router_file);
}

/*
 * XXX Need to actually implement this a little more intelligenetly...
 */
void simclick_click_run(simclick_click clickinst,simclick_simstate* state) {
  setsimstate(state);
  //fprintf(stderr,"Hey! Need to implement simclick_click_run!\n");
  // not right - mostly smoke testing for now...
  Router* r = ((SimState*)clickinst)->router;
  if (r) {
    r->master()->thread(0)->driver();
  }
  else {
    click_chatter("simclick_click_run: call with null router");
  }
}

void simclick_click_kill(simclick_click clickinst, simclick_simstate* state) {
  //fprintf(stderr,"Hey! Need to implement simclick_click_kill!\n");
  setsimstate(state);
  Router *r = ((SimState*)clickinst)->router;
  if (r) {
    delete r;
    ((SimState*)clickinst)->router = 0;
  }
  else {
    click_chatter("simclick_click_kill: call with null router");
  }
}

int simclick_gettimeofday(struct timeval* tv) {
  simclick_simstate* sstate = getsimstate();
  if (sstate) {
    *tv = sstate->curtime;
  }
  else {
    fprintf(stderr,"Hey! Called simclick_gettimeofday without simstate set!\n");
  }
  return 0;
}

int simclick_click_send(simclick_click clickinst,simclick_simstate* state,
			int ifid,int type,const unsigned char* data,int len,
			simclick_simpacketinfo* pinfo) {
  setsimstate(state);
  int result = 0;
  Router* r = ((SimState*)clickinst)->router;
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

char* simclick_click_read_handler(simclick_click clickinst,
				  const char* elementname,
				  const char* handlername,
				  SIMCLICK_MEM_ALLOC memalloc,
				  void* memparam) {
    Router *r = ((SimState*)clickinst)->router;
    if (!r) {
      click_chatter("simclick_click_read_handler: call with null router");
      return 0;
    }
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

int simclick_click_write_handler(simclick_click clickinst,
				 const char* elementname,
				 const char* handlername,
				 const char* writestring) {
    Router *r = ((SimState*)clickinst)->router;
    if (!r) {
      click_chatter("simclick_click_write_handler: call with null router");
      return -3;
    }
    String hdesc = String(elementname) + "." + String(handlername);
    return HandlerCall::call_write(hdesc, String(writestring), r->root_element(), ErrorHandler::default_handler());
}
