// -*- c-basic-offset: 4; related-file-name: "../include/click/router.hh" -*-
/*
 * router.{cc,hh} -- a Click router configuration
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2004-2007 Regents of the University of California
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

#define WANT_MOD_USE_COUNT 1
#include <click/config.h>
#include <click/router.hh>
#include <click/routerthread.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/elemfilter.hh>
#include <click/confparse.hh>
#include <click/timer.hh>
#include <click/master.hh>
#include <click/notifier.hh>
#include <click/nameinfo.hh>
#include <click/bighashmap_arena.hh>
#include <click/standard/errorelement.hh>
#include <click/standard/threadsched.hh>
#include <stdarg.h>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
#if CLICK_NS
# include "../elements/ns/fromsimdevice.hh"
#endif

CLICK_DECLS

/** @file router.hh
 * @brief The Router class implementing a router configuration.
 */

/** @class Router
 * @brief A router configuration.
 */

const Handler* Handler::the_blank_handler;
static Handler* globalh;
static int nglobalh;
static int globalh_cap;

/** @brief  Create a router.
 *  @param  configuration  router configuration
 *  @param  master         Master object
 *
 *  Users generally do not call this function directly, instead creating a
 *  router object by calling Lexer functions (this function doesn't actually
 *  parse the configuration string).  The router is registered with the Master
 *  object, but not initialized or activated. */
Router::Router(const String &configuration, Master *master)
    : _master(0), _state(ROUTER_NEW), _have_connections(false),
      _running(RUNNING_INACTIVE),
      _handler_bufs(0), _nhandlers_bufs(0), _free_handler(-1),
      _root_element(0),
      _configuration(configuration),
      _notifier_signals(0), _n_notifier_signals(0),
      _arena_factory(new HashMap_ArenaFactory),
      _hotswap_router(0), _thread_sched(0), _name_info(0), _next_router(0)
{
    _refcount = 0;
    _runcount = 0;
    _root_element = new ErrorElement;
    _root_element->attach_router(this, -1);
    master->register_router(this);
}

/** @brief  Destroy the router.
 *  @invariant  The reference count must be zero.
 *
 *  Users generally do not destroy Router objects directly, instead calling
 *  Router::unuse(). */
Router::~Router()
{
    if (_refcount != 0)
	click_chatter("deleting router while ref count = %d", _refcount.value());

    // unuse the hotswap router
    if (_hotswap_router)
	_hotswap_router->unuse();
  
    // Delete the ArenaFactory, which detaches the Arenas
    delete _arena_factory;

    // Clean up elements in reverse configuration order
    if (_state == ROUTER_LIVE) {
	// Unschedule tasks and timers
	_master->kill_router(this);
	for (int ord = _elements.size() - 1; ord >= 0; ord--)
	    _elements[ _element_configure_order[ord] ]->cleanup(Element::CLEANUP_ROUTER_INITIALIZED);
    } else if (_state != ROUTER_DEAD) {
	assert(_element_configure_order.size() == 0 && _state <= ROUTER_PRECONFIGURE);
	for (int i = _elements.size() - 1; i >= 0; i--)
	    _elements[i]->cleanup(Element::CLEANUP_NO_ROUTER);
    }
  
    // Delete elements in reverse configuration order
    if (_element_configure_order.size())
	for (int ord = _elements.size() - 1; ord >= 0; ord--)
	    delete _elements[ _element_configure_order[ord] ];
    else
	for (int i = 0; i < _elements.size(); i++)
	    delete _elements[i];
    
    delete _root_element;

#if CLICK_LINUXMODULE
    // decrement module use counts
    for (struct module **m = _modules.begin(); m < _modules.end(); m++) {
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	module_put(*m);
# else
	__MOD_DEC_USE_COUNT(*m);
# endif
    }
#endif
    
    for (int i = 0; i < _nhandlers_bufs; i += HANDLER_BUFSIZ)
	delete[] _handler_bufs[i / HANDLER_BUFSIZ];
    delete[] _handler_bufs;
    delete[] _notifier_signals;
    delete _name_info;
    if (_master)
	_master->unregister_router(this);
}

/** @brief  Decrement the router's reference count.
 *
 *  Destroys the router if the reference count decrements to zero. */
void
Router::unuse()
{
    if (_refcount.dec_and_test())
	delete this;
}


// ACCESS

/** @brief  Finds an element named @a name.
 *  @param  name     element name
 *  @param  context  compound element context
 *  @param  errh     optional error handler
 *
 *  Searches for an element named @a name in the compound element context
 *  specified by @a context, returning the first element found.  For example,
 *  if @a context was <tt>"aaa/bbb/ccc/"</tt>, then find() would search for
 *  elements named <tt>aaa/bbb/ccc/name</tt>, <tt>aaa/bbb/name</tt>,
 *  <tt>aaa/name</tt>, and finally <tt>name</tt>, returning the first element
 *  found.  If nonempty, @a context should end with a slash.
 *
 *  If no element named @a name is found, reports an error to @a errh and
 *  returns null.  The error is "<tt>no element named 'name'</tt>".  If @a
 *  errh is null, no error is reported.
 */
Element *
Router::find(const String &name, String context, ErrorHandler *errh) const
{
  while (1) {
    int got = -1;
    String n = context + name;
    for (int i = 0; i < _elements.size(); i++)
      if (_element_names[i] == n) {
	if (got >= 0) {
	  if (errh) errh->error("more than one element named '%s'", n.c_str());
	  return 0;
	} else
	  got = i;
      }
    if (got >= 0)
      return _elements[got];
    if (!context)
      break;
    
    int slash = context.find_right('/', context.length() - 2);
    context = (slash >= 0 ? context.substring(0, slash + 1) : String());
  }
  
  if (errh)
    errh->error("no element named '%s'", name.c_str());
  return 0;
}

/** @brief  Finds an element named @a name.
 *  @param  name     element name
 *  @param  context  compound element context
 *  @param  errh     optional error handler
 *
 *  Searches for an element named @a name in the compound element context
 *  specified by @a context, returning the first element found.  For example,
 *  if ename(@a context) was <tt>"aaa/bbb/element"</tt>, then find(@a name, @a
 *  context, @a errh) is equivalent to find(@a name, <tt>"aaa/bbb/"</tt>, @a
 *  errh), and will search for elements named <tt>aaa/bbb/name</tt>,
 *  <tt>aaa/name</tt>, and finally <tt>name</tt>.
 *
 *  If no element named @a name is found, reports an error to @a errh and
 *  returns null.  The error is "<tt>no element named 'name'</tt>".  If @a
 *  errh is null, no error is reported. */
Element *
Router::find(const String &name, Element *context, ErrorHandler *errh) const
{
  String prefix = ename(context->eindex());
  int slash = prefix.find_right('/');
  return find(name, (slash >= 0 ? prefix.substring(0, slash + 1) : String()), errh);
}

/** @brief  Return @a router's element with index @a eindex.
 *  @param  router  the router (may be null)
 *  @param  eindex  element index, or -1 for router->root_element()
 *
 *  This function returns @a router's element with index @a eindex.  If
 *  @a router is null or @a eindex is out of range, returns null. */
Element *
Router::element(const Router *router, int eindex)
{
    if (router && eindex >= 0 && eindex < router->nelements())
	return router->_elements[eindex];
    else if (router && eindex == -1)
	return router->root_element();
    else
	return 0;
}

/** @brief  Returns element index @a eindex's name.
 *  @param  eindex  element index
 *
 *  Returns the empty string if @a eindex is out of range. */
const String &
Router::ename(int eindex) const
{
    if (eindex < 0 || eindex >= nelements())
	return String::empty_string();
    else
	return _element_names[eindex];
}

/** @brief  Returns element index @a eindex's configuration string.
 *  @param  eindex  element index
 *
 *  Returns the empty string if @a eindex is out of range.
 *
 *  @note econfiguration() returns the element's most recently specified
 *  static configuration string, which might differ from the element's active
 *  configuration string.  For the active configuration, call the virtual
 *  function Element::configuration(), which defaults to returning
 *  econfiguration(). */
const String &
Router::econfiguration(int eindex) const
{
    if (eindex < 0 || eindex >= nelements())
	return String::empty_string();
    else
	return _element_configurations[eindex];
}

/** @brief  Sets element index @a eindex's configuration string.
 *  @param  eindex  element index
 *  @param  conf    configuration string
 *
 *  Does nothing if @a eindex is out of range. */
void
Router::set_econfiguration(int eindex, const String &conf)
{
    if (eindex >= 0 && eindex < nelements())
	_element_configurations[eindex] = conf;
}

/** @brief  Returns element index @a eindex's landmark.
 *  @param  eindex  element index
 *
 *  A landmark is a short string specifying where the element was defined.  A
 *  typical landmark has the form "file:linenumber", as in
 *  <tt>"file.click:30"</tt>.  Returns the empty string if @a eindex is out of
 *  range. */
const String &
Router::elandmark(int eindex) const
{
    if (eindex < 0 || eindex >= nelements())
	return String::empty_string();
    else
	return _element_landmarks[eindex];
}


// CREATION 

#if CLICK_LINUXMODULE
int
Router::add_module_ref(struct module *module)
{
    if (!module)
	return 0;
    for (struct module **m = _modules.begin(); m < _modules.end(); m++)
	if (*m == module)
	    return 0;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
    if (try_module_get(module) == 0)
	return -1;
# else
    __MOD_INC_USE_COUNT(module);
# endif
    _modules.push_back(module);
    return 0;
}
#endif
    
int
Router::add_element(Element *e, const String &ename, const String &conf, const String &landmark)
{
    if (_state != ROUTER_NEW || !e || e->router())
	return -1;

    _elements.push_back(e);
    _element_names.push_back(ename);
    _element_landmarks.push_back(landmark);
    _element_configurations.push_back(conf);
    int i = _elements.size() - 1;
    e->attach_router(this, i);

    // router now owns the element
    return i;
}

int
Router::add_connection(int from_idx, int from_port, int to_idx, int to_port)
{
    assert(from_idx >= 0 && from_port >= 0 && to_idx >= 0 && to_port >= 0);
    if (_state != ROUTER_NEW)
	return -1;
    Hookup hfrom(from_idx, from_port);
    Hookup hto(to_idx, to_port);
    // only add new connections
    for (int i = 0; i < _hookup_from.size(); i++)
	if (_hookup_from[i] == hfrom && _hookup_to[i] == hto)
	    return 0;
    _hookup_from.push_back(hfrom);
    _hookup_to.push_back(hto);
    return 0;
}

void
Router::add_requirement(const String &r)
{
    assert(cp_is_word(r));
    _requirements.push_back(r);
}


// CHECKING HOOKUP

void
Router::remove_hookup(int c)
{
    _hookup_from[c] = _hookup_from.back();
    _hookup_from.pop_back();
    _hookup_to[c] = _hookup_to.back();
    _hookup_to.pop_back();
}

void
Router::hookup_error(const Hookup &h, bool is_from, const char *message,
		     ErrorHandler *errh)
{
    bool is_output = is_from;
    const char *kind = (is_output ? "output" : "input");
    element_lerror(errh, _elements[h.idx], message,
		   _elements[h.idx], kind, h.port);
}

int
Router::check_hookup_elements(ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    int before = errh->nerrors();
  
    // Check each hookup to ensure it connects valid elements
    for (int c = 0; c < _hookup_from.size(); c++) {
	Hookup &hfrom = _hookup_from[c];
	Hookup &hto = _hookup_to[c];
	int before = errh->nerrors();
    
	if (hfrom.idx < 0 || hfrom.idx >= nelements() || !_elements[hfrom.idx])
	    errh->error("bad element number '%d'", hfrom.idx);
	if (hto.idx < 0 || hto.idx >= nelements() || !_elements[hto.idx])
	    errh->error("bad element number '%d'", hto.idx);
	if (hfrom.port < 0)
	    errh->error("bad port number '%d'", hfrom.port);
	if (hto.port < 0)
	    errh->error("bad port number '%d'", hto.port);
    
	// remove the connection if there were errors
	if (errh->nerrors() != before) {
	    remove_hookup(c);
	    c--;
	}
    }
  
    return (errh->nerrors() == before ? 0 : -1);
}

int
Router::check_hookup_range(ErrorHandler *errh)
{
    int before_all = errh->nerrors();
    
    // Count inputs and outputs, and notify elements how many they have
    Vector<int> nin(nelements(), -1);
    Vector<int> nout(nelements(), -1);
    for (int c = 0; c < _hookup_from.size(); c++) {
	if (_hookup_from[c].port > nout[_hookup_from[c].idx])
	    nout[_hookup_from[c].idx] = _hookup_from[c].port;
	if (_hookup_to[c].port > nin[_hookup_to[c].idx])
	    nin[_hookup_to[c].idx] = _hookup_to[c].port;
    }
    for (int f = 0; f < nelements(); f++)
	_elements[f]->notify_nports(nin[f] + 1, nout[f] + 1, errh);

    // Check each hookup to ensure its port numbers are within range
    for (int c = 0; c < _hookup_from.size(); c++) {
	Hookup &hfrom = _hookup_from[c];
	Hookup &hto = _hookup_to[c];
	int before = errh->nerrors();
    
	if (hfrom.port >= _elements[hfrom.idx]->noutputs())
	    hookup_error(hfrom, true, "'%{element}' has no %s %d", errh);
	if (hto.port >= _elements[hto.idx]->ninputs())
	    hookup_error(hto, false, "'%{element}' has no %s %d", errh);
    
	// remove the connection if there were errors
	if (errh->nerrors() == before)
	    /* do nothing */;
	else {
	    remove_hookup(c);
	    c--;
	}
    }
    
    return errh->nerrors() == before_all ? 0 : -1;
}

int
Router::check_hookup_completeness(ErrorHandler *errh)
{
    Bitvector used_outputs(ngports(true), false);
    Bitvector used_inputs(ngports(false), false);
    int before_all = errh->nerrors();
  
    // Check each hookup to ensure it doesn't reuse a port.
    // Completely duplicate connections never got into the Router
    for (int c = 0; c < _hookup_from.size(); c++) {
	Hookup &hfrom = _hookup_from[c];
	Hookup &hto = _hookup_to[c];
	int before = errh->nerrors();
    
	int fromg = gport(true, hfrom);
	int tog = gport(false, hto);
	if (used_outputs[fromg]
	    && _elements[hfrom.idx]->output_is_push(hfrom.port))
	    hookup_error(hfrom, true, "illegal reuse of '%{element}' push %s %d", errh);
	else if (used_inputs[tog]
		 && _elements[hto.idx]->input_is_pull(hto.port))
	    hookup_error(hto, false, "illegal reuse of '%{element}' pull %s %d", errh);
	
	// remove the connection if there were errors
	if (errh->nerrors() == before) {
	    used_outputs[fromg] = true;
	    used_inputs[tog] = true;
	} else {
	    remove_hookup(c);
	    c--;
	}
    }

    // Check for unused inputs and outputs.
    for (int g = 0; g < ngports(false); g++)
	if (!used_inputs[g])
	    hookup_error(gport_hookup(false, g), false, "'%{element}' %s %d unused", errh);
    for (int g = 0; g < ngports(true); g++)
	if (!used_outputs[g])
	    hookup_error(gport_hookup(true, g), true, "'%{element}' %s %d unused", errh);

    return errh->nerrors() == before_all ? 0 : -1;
}


// PORT INDEXES

void
Router::make_gports()
{
    _gports[0].e2g.assign(1, 0);
    _gports[1].e2g.assign(1, 0);
    _gports[0].g2e.clear();
    _gports[1].g2e.clear();
    for (int i = 0; i < _elements.size(); i++) {
	Element *e = _elements[i];
	_gports[0].e2g.push_back(_gports[0].e2g.back() + e->ninputs());
	_gports[1].e2g.push_back(_gports[1].e2g.back() + e->noutputs());
	for (int j = 0; j < e->ninputs(); j++)
	    _gports[0].g2e.push_back(i);
	for (int j = 0; j < e->noutputs(); j++)
	    _gports[1].g2e.push_back(i);
    }
}

inline int
Router::gport(bool isoutput, const Hookup &h) const
{
    return _gports[isoutput].e2g[h.idx] + h.port;
}

inline Router::Hookup
Router::gport_hookup(bool isoutput, int g) const
{
    int e = _gports[isoutput].g2e[g];
    return Hookup(e, g - _gports[isoutput].e2g[e]);
}

void
Router::gport_list_elements(bool isoutput, const Bitvector& bv, Vector<Element*>& results) const
{
    const Gport& gport = _gports[isoutput];
    int ng = gport.size();
    assert(bv.size() == ng);
    const int* gp = gport.e2g.begin();
    for (int g = 0; g < ng; ) {
	while (g >= gp[1])
	    gp++;
	if (bv[g]) {
	    results.push_back(_elements[gp - gport.e2g.begin()]);
	    g = gp[1];
	} else
	    g++;
    }
}

void
Router::make_hookup_gports()
{
    if (_hookup_gports[0].size() != _hookup_to.size()) {
	for (int c = 0; c < _hookup_to.size(); c++) {
	    _hookup_gports[0].push_back(gport(false, _hookup_to[c]));
	    _hookup_gports[1].push_back(gport(true, _hookup_from[c]));
	}
	assert(_hookup_gports[0].size() == _hookup_to.size());
    }
}

// PROCESSING

int
Router::processing_error(const Hookup &hfrom, const Hookup &hto, bool aggie,
			 int processing_from, ErrorHandler *errh)
{
    const char *type1 = (processing_from == Element::VPUSH ? "push" : "pull");
    const char *type2 = (processing_from == Element::VPUSH ? "pull" : "push");
    if (!aggie)
	errh->error("'%{element}' %s output %d connected to '%{element}' %s input %d",
		    _elements[hfrom.idx], type1, hfrom.port,
		    _elements[hto.idx], type2, hto.port);
    else
	errh->error("agnostic '%{element}' in mixed context: %s input %d, %s output %d",
		    _elements[hfrom.idx], type2, hto.port, type1, hfrom.port);
    return -1;
}

int
Router::check_push_and_pull(ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::default_handler();
  
    // set up processing vectors
    Vector<int> input_pers(ngports(false), 0);
    Vector<int> output_pers(ngports(true), 0);
    for (int e = 0; e < nelements(); e++)
	_elements[e]->processing_vector(input_pers.begin() + _gports[0].e2g[e], output_pers.begin() + _gports[1].e2g[e], errh);
  
    // add fake connections for agnostics
    Vector<Hookup> hookup_from = _hookup_from;
    Vector<Hookup> hookup_to = _hookup_to;
    Bitvector bv;
    for (int* inputp = input_pers.begin(); inputp < input_pers.end(); inputp++)
	if (*inputp == Element::VAGNOSTIC) {
	    Hookup h = gport_hookup(false, inputp - input_pers.begin());
	    _elements[h.idx]->port_flow(false, h.port, &bv);
	    int og = _gports[1].e2g[h.idx];
	    for (int j = 0; j < bv.size(); j++)
		if (bv[j] && output_pers[og+j] == Element::VAGNOSTIC) {
		    hookup_from.push_back(Hookup(h.idx, j));
		    hookup_to.push_back(Hookup(h.idx, h.port));
		}
	}
    
    int before = errh->nerrors();
    int first_agnostic = _hookup_from.size();
  
    // spread personalities
    while (true) {
    
	bool changed = false;
	for (int c = 0; c < hookup_from.size(); c++) {
	    if (hookup_from[c].idx < 0)
		continue;
      
	    int gf = gport(true, hookup_from[c]);
	    int gt = gport(false, hookup_to[c]);
	    int pf = output_pers[gf];
	    int pt = input_pers[gt];
      
	    switch (pt) {
		
	      case Element::VAGNOSTIC:
		if (pf != Element::VAGNOSTIC) {
		    input_pers[gt] = pf;
		    changed = true;
		}
		break;
	
	      case Element::VPUSH:
	      case Element::VPULL:
		if (pf == Element::VAGNOSTIC) {
		    output_pers[gf] = pt;
		    changed = true;
		} else if (pf != pt) {
		    processing_error(hookup_from[c], hookup_to[c], c >= first_agnostic, pf, errh);
		    hookup_from[c].idx = -1;
		}
		break;
	
	    }
	}
    
	if (!changed)
	    break;
    }
  
    if (errh->nerrors() != before)
	return -1;

    for (int e = 0; e < nelements(); e++)
	_elements[e]->initialize_ports(input_pers.begin() + _gports[0].e2g[e], output_pers.begin() + _gports[1].e2g[e]);
    return 0;
}


// SET CONNECTIONS

int
Router::element_lerror(ErrorHandler *errh, Element *e,
		       const char *format, ...) const
{
    va_list val;
    va_start(val, format);
    errh->verror(ErrorHandler::ERR_ERROR, e->landmark(), format, val);
    va_end(val);
    return -1;
}

void
Router::set_connections()
{
    // actually assign ports
    for (int c = 0; c < _hookup_from.size(); c++) {
	Hookup& hfrom = _hookup_from[c];
	Element* frome = _elements[hfrom.idx];
	Hookup& hto = _hookup_to[c];
	Element* toe = _elements[hto.idx];
	frome->connect_port(true, hfrom.port, toe, hto.port);
	toe->connect_port(false, hto.port, frome, hfrom.port);
    }
    _have_connections = true;
}


// RUNCOUNT

/** @brief  Set the runcount.
 *  @param  rc  new runcount
 *
 *  Sets the runcount to a specific value.  If the new runcount is zero or
 *  negative, stops the router; see adjust_runcount(). */
void
Router::set_runcount(int32_t rc)
{
    _runcount = rc;
    if (rc <= 0) {
	_master->set_stopper(1);
	// ensure that at least one thread is awake to handle the stop event
	_master->_threads[2]->wake();
    }
}

/** @brief  Adjust the runcount by @a delta.
 *  @param  delta  runcount adjustment
 *
 *  Essentially performs the assignment "runcount += delta" with compensation
 *  for integer overflow.  (For instance, if runcount is INT_MAX and delta is
 *  INT_MAX, the resulting runcount will be INT_MAX, not -2.)  Uses atomic
 *  operations to ensure that runcount adjustments are not lost.
 *
 *  If the adjusted runcount is zero or negative, the router is asked to stop
 *  its normal processing.  This will happen soon, although not necessarily
 *  immediately.  Once it stops, the router will search for an element to
 *  manage the stop event (see the Script and DriverManager elements).  If no
 *  such element exists, or the script completes without raising the runcount,
 *  the router stops permanently. */
void
Router::adjust_runcount(int32_t delta)
{
    // beware of overflow
    int32_t old_value, new_value;
    do {
	old_value = _runcount;
	if (delta > 0 && old_value > 0x7FFFFFFF - delta)
	    new_value = 0x7FFFFFFF;
	else if (delta < 0 && old_value < STOP_RUNCOUNT - delta)
	    new_value = STOP_RUNCOUNT;
	else
	    new_value = old_value + delta;
    } while (!_runcount.compare_and_swap(old_value, new_value));
    if (new_value <= 0) {
	_master->set_stopper(1);
	// ensure that at least one thread is awake to handle the stop event
	_master->_threads[2]->wake();
    }
}


// FLOWS

int
Router::global_port_flow(bool forward, Element* first_element, int first_port, ElementFilter* stop_filter, Bitvector& results)
{
    if (!_have_connections || first_element->router() != this)
	return -1;
    make_hookup_gports();
    int nresult = ngports(!forward);
    int nsource = ngports(forward);
    
    Bitvector old_results(nresult, false);
    results.assign(nresult, false);
    Bitvector diff, scratch;
    
    Bitvector source(nsource, false);
    int first_gport = _gports[forward].e2g[first_element->eindex()];
    if (first_port < 0)
	for (int i = 0; i < first_element->nports(forward); i++)
	    source[first_gport + i] = true;
    else if (first_port < first_element->nports(forward))
	source[first_gport + first_port] = true;

    while (true) {
	old_results = results;

	for (const int* gfrom = _hookup_gports[forward].begin(); gfrom < _hookup_gports[forward].end(); gfrom++)
	    if (source[*gfrom]) {
		int i = gfrom - _hookup_gports[forward].begin();
		results[_hookup_gports[!forward][i]] = true;
	    }
    
	diff = results - old_results;
	if (diff.zero())
	    break;
	
	source.assign(nsource, false);
	for (int g = 0; g < nresult; g++)
	    if (diff[g]) {
		Hookup h = gport_hookup(!forward, g);
		if (!stop_filter || !stop_filter->check_match(_elements[h.idx], !forward, h.port)) {
		    _elements[h.idx]->port_flow(!forward, h.port, &scratch);
		    source.or_at(scratch, _gports[forward].e2g[h.idx]);
		}
	    }
    }
  
    return 0;
}

/** @brief Search for elements downstream from @a e.
 * @param e element to start search
 * @param port output port (or -1 to search all output ports)
 * @param filter ElementFilter naming elements that stop the search
 * @param result stores results
 * @return 0 on success, -1 in early router configuration stages
 *
 * This function searches the router configuration graph, starting from @a e's
 * output port @a port and proceeding downstream along element connections,
 * and inside elements from port to port by Element::flow_code(). All found
 * elements are stored in @a result.
 *
 * If @a filter != NULL, then each found port is passed to @a filter's
 * ElementFilter::check_match() function to check whether to stop the search.
 * For example, if @a filter is CastElementFilter("Storage"), then the search
 * will stop after any Storage element.  The @a result will then include the
 * Storage elements and any elements in between @a e and the Storage elements.
 *
 * @sa upstream_elements()
 */
int
Router::downstream_elements(Element *e, int port, ElementFilter *filter, Vector<Element *> &result)
{
    Bitvector bv;
    if (global_port_flow(true, e, port, filter, bv) < 0)
	return -1;
    gport_list_elements(false, bv, result);
    return 0;
}

/** @brief Search for elements upstream from @a e.
 * @param e element to start search
 * @param port input port (or -1 to search all ports)
 * @param filter ElementFilter naming elements that stop the search
 * @param result stores results
 * @return 0 on success, -1 in early router configuration stages
 *
 * This function searches the router configuration graph, starting from @a e's
 * input port @a port and proceeding upstream along element connections,
 * and inside elements from port to port by Element::flow_code(). All found
 * elements are stored in @a result.
 *
 * If @a filter != NULL, then each found port is passed to @a filter's
 * ElementFilter::check_match() function to check whether to stop the search.
 * For example, if @a filter is CastElementFilter("Storage"), then the search
 * will stop after any Storage element.  The @a result will then include the
 * Storage elements and any elements in between @a e and the Storage elements.
 *
 * @sa downstream_elements()
 */
int
Router::upstream_elements(Element *e, int port, ElementFilter *filter, Vector<Element *> &result)
{
    Bitvector bv;
    if (global_port_flow(false, e, port, filter, bv) < 0)
	return -1;
    gport_list_elements(true, bv, result);
    return 0;
}


// INITIALIZATION

String
Router::context_message(int element_no, const char* message) const
{
    Element* e = _elements[element_no];
    StringAccum sa;
    if (e->landmark())
	sa << e->landmark() << ": ";
    sa << message << " '" << e->declaration() << "':";
    return sa.take_string();
}

static int
configure_order_compar(const void *athunk, const void *bthunk, void *copthunk)
{
    const int* a = (const int*) athunk, *b = (const int*) bthunk;
    const int* configure_order_phase = (const int*) copthunk;
    return configure_order_phase[*a] - configure_order_phase[*b];
}

inline Handler*
Router::xhandler(int hi) const
{
    return &_handler_bufs[hi / HANDLER_BUFSIZ][hi % HANDLER_BUFSIZ];
}

void
Router::initialize_handlers(bool defaults, bool specifics)
{
    _ehandler_first_by_element.assign(nelements(), -1);
    _ehandler_to_handler.clear();
    _ehandler_next.clear();

    _handler_first_by_name.clear();

    for (int i = 0; i < _nhandlers_bufs; i += HANDLER_BUFSIZ)
	delete[] _handler_bufs[i / HANDLER_BUFSIZ];
    _nhandlers_bufs = 0;
    _free_handler = -1;

    if (defaults)
	for (int i = 0; i < _elements.size(); i++)
	    _elements[i]->add_default_handlers(specifics);

    if (specifics)
	for (int i = 0; i < _elements.size(); i++)
	    _elements[i]->add_handlers();
}

int
Router::initialize(ErrorHandler *errh)
{
    if (_state != ROUTER_NEW)
	return errh->error("second attempt to initialize router");
    _state = ROUTER_PRECONFIGURE;

    // initialize handlers to empty
    initialize_handlers(false, false);
  
    // clear attachments
    _attachment_names.clear();
    _attachments.clear();
  
    if (check_hookup_elements(errh) < 0)
	return -1;
  
    // set up configuration order
    _element_configure_order.assign(nelements(), 0);
    if (_element_configure_order.size()) {
	Vector<int> configure_phase(nelements(), 0);
	for (int i = 0; i < _elements.size(); i++) {
	    configure_phase[i] = _elements[i]->configure_phase();
	    _element_configure_order[i] = i;
	}
	click_qsort(&_element_configure_order[0], _element_configure_order.size(), sizeof(int), configure_order_compar, configure_phase.begin());
    }

    // remember how far the configuration process got for each element
    Vector<int> element_stage(nelements(), Element::CLEANUP_BEFORE_CONFIGURE);
    bool all_ok = false;
    
    // check connections
    if (check_hookup_range(errh) >= 0) {
	make_gports();
	if (check_push_and_pull(errh) >= 0
	    && check_hookup_completeness(errh) >= 0) {
	    set_connections();
	    all_ok = true;
	}
    }

    // prepare master
    _runcount = 1;
    _master->prepare_router(this);
#if CLICK_DMALLOC
    char dmalloc_buf[12];
#endif

    // Configure all elements in configure order. Remember the ones that failed
    if (all_ok) {
	Vector<String> conf;
	for (int ord = 0; ord < _elements.size(); ord++) {
	    int i = _element_configure_order[ord];
#if CLICK_DMALLOC
	    sprintf(dmalloc_buf, "c%d  ", i);
	    CLICK_DMALLOC_REG(dmalloc_buf);
#endif
	    ContextErrorHandler cerrh
		(errh, context_message(i, "While configuring"));
	    int before = cerrh.nerrors();
	    conf.clear();
	    cp_argvec(_element_configurations[i], conf);
	    if (_elements[i]->configure(conf, &cerrh) < 0) {
		element_stage[i] = Element::CLEANUP_CONFIGURE_FAILED;
		all_ok = false;
		if (cerrh.nerrors() == before)
		    cerrh.error("unspecified error");
	    } else
		element_stage[i] = Element::CLEANUP_CONFIGURED;
	}
    }

#if CLICK_DMALLOC
    CLICK_DMALLOC_REG("iHoo");
#endif

    // Initialize elements if OK so far.
    if (all_ok) {
	_state = ROUTER_PREINITIALIZE;
	initialize_handlers(true, true);
	for (int ord = 0; all_ok && ord < _elements.size(); ord++) {
	    int i = _element_configure_order[ord];
	    assert(element_stage[i] == Element::CLEANUP_CONFIGURED);
#if CLICK_DMALLOC
	    sprintf(dmalloc_buf, "i%d  ", i);
	    CLICK_DMALLOC_REG(dmalloc_buf);
#endif
	    ContextErrorHandler cerrh
		(errh, context_message(i, "While initializing"));
	    int before = cerrh.nerrors();
	    if (_elements[i]->initialize(&cerrh) >= 0)
		element_stage[i] = Element::CLEANUP_INITIALIZED;
	    else {
		// don't report 'unspecified error' for ErrorElements:
		// keep error messages clean
		if (cerrh.nerrors() == before && !_elements[i]->cast("Error"))
		    cerrh.error("unspecified error");
		element_stage[i] = Element::CLEANUP_INITIALIZE_FAILED;
		all_ok = false;
	    }
	}
    }

#if CLICK_DMALLOC
    CLICK_DMALLOC_REG("iXXX");
#endif
  
    // If there were errors, uninitialize any elements that we initialized
    // successfully and return -1 (error). Otherwise, we're all set!
    if (all_ok) {
	_state = ROUTER_LIVE;
#ifdef CLICK_NAMEDB_CHECK
	NameInfo::check(_root_element, errh);
#endif
	return 0;
    } else {
	_state = ROUTER_DEAD;
	errh->verror_text(ErrorHandler::ERR_CONTEXT_ERROR, "", "Router could not be initialized!");
    
	// Unschedule tasks and timers
	master()->kill_router(this);

	// Clean up elements
	for (int ord = _elements.size() - 1; ord >= 0; ord--) {
	    int i = _element_configure_order[ord];
	    _elements[i]->cleanup((Element::CleanupStage) element_stage[i]);
	}
    
	// Remove element-specific handlers
	initialize_handlers(true, false);
    
	_runcount = 0;
	return -1;
    }
}

void
Router::activate(bool foreground, ErrorHandler *errh)
{
    if (_state != ROUTER_LIVE || _running != RUNNING_PREPARING)
	return;
  
    // Take state if appropriate
    if (_hotswap_router && _hotswap_router->_state == ROUTER_LIVE) {
	// Unschedule tasks and timers
	master()->kill_router(_hotswap_router);
      
	for (int i = 0; i < _elements.size(); i++) {
	    Element *e = _elements[_element_configure_order[i]];
	    if (Element *other = e->hotswap_element()) {
		ContextErrorHandler cerrh
		    (errh, context_message(i, "While hot-swapping state into"));
		e->take_state(other, &cerrh);
	    }
	}
    }
    if (_hotswap_router) {
	_hotswap_router->unuse();
	_hotswap_router = 0;
    }

    // Activate router
    master()->run_router(this, foreground);
    // sets _running to RUNNING_BACKGROUND or RUNNING_ACTIVE
}


// steal state

void
Router::set_hotswap_router(Router *r)
{
    assert(_state == ROUTER_NEW && !_hotswap_router && (!r || r->initialized()));
    _hotswap_router = r;
    if (_hotswap_router)
	_hotswap_router->use();
}


// HANDLERS

/** @class Handler
    @brief Represents a router's handlers.

    Each handler is represented by a Handler object.  Handlers are not
    attached to specific elements, allowing a single handler object to be
    shared among multiple elements with the same basic handler definition.
    Handlers cannot be created directly; to create one, call methods such as
    Router::add_read_handler(), Router::add_write_handler(),
    Router::set_handler(), Element::add_read_handler(),
    Element::add_write_handler(), and Element::set_handler().

    The HandlerCall class simplifies interaction with handlers and is
    preferred to direct Handler calls for most purposes. */

/** @brief Call a read handler, possibly with parameters.
    @param e element on which to call the handler
    @param param parameters, or an empty string if no parameters
    @param raw true iff param is raw text (see raw())
    @param errh optional error handler

    The element must be nonnull; to call a global handler, pass the relevant
    router's Router::root_element().  @a errh may be null, in which case
    errors are reported to ErrorHandler::silent_handler(). */
String
Handler::call_read(Element* e, const String& param, bool raw, ErrorHandler* errh) const
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    if (param && !(_flags & READ_PARAM))
	errh->error("read handler '%s' does not take parameters", unparse_name(e).c_str());
    else if ((_flags & (UNIFORM | OP_READ)) == OP_READ)
	return _hook.rw.r(e, _thunk1);
    else if (_flags & OP_READ) {
	String s(param);
	if ((_flags & RAW) && !raw)
	    s = cp_unquote(s);
	if (_hook.h(OP_READ, s, e, this, errh) >= 0)
	    return s;
    } else
	errh->error("'%s' not a read handler", unparse_name(e).c_str());
    // error cases get here
    return String();
}

/** @brief Call a write handler.
    @param value value to write to the handler
    @param e element on which to call the handler
    @param raw true iff value is raw text (see raw())
    @param errh optional error handler

    The element must be nonnull; to call a global handler, pass the relevant
    router's Router::root_element().  @a errh may be null, in which case
    errors are reported to ErrorHandler::silent_handler(). */
int
Handler::call_write(const String& value, Element* e, bool raw, ErrorHandler* errh) const
{
    if (!errh)
	errh = ErrorHandler::silent_handler();
    String value_copy(value);
    if ((_flags & RAW) && !raw)
	value_copy = cp_unquote(value_copy);
    if ((_flags & (UNIFORM | OP_WRITE)) == OP_WRITE)
	return _hook.rw.w(value_copy, e, _thunk2, errh);
    else if (_flags & OP_WRITE)
	return _hook.h(OP_WRITE, value_copy, e, this, errh);
    else {
	errh->error("'%s' not a write handler", unparse_name(e).c_str());
	return -EACCES;
    }
}

/** @brief Unparse a handler name.
 *  @param  e      relevant element, if any
 *  @param  hname  handler name
 *
 *  If @a e is an actual element on some router, then returns "ENAME.hname",
 *  where ENAME is @a e's @link Element::name() name@endlink.  Otherwise, just
 *  returns @a hname.*/
String
Handler::unparse_name(Element *e, const String &hname)
{
    if (e && e->eindex() >= 0)
	return e->name() + "." + hname;
    else
	return hname;
}

/** @brief Unparse this handler's name.
    @param  e  relevant element

    If @a e is an actual element, then returns "ENAME.HNAME", where ENAME is
    @a e's @link Element::name() name@endlink and HNAME is this handler's
    name().  Otherwise, just returns name(). */
String
Handler::unparse_name(Element *e) const
{
    if (this == the_blank_handler)
	return _name;
    else
	return unparse_name(e, _name);
}


// Private functions for finding and storing handlers

// 11.Jul.2000 - We had problems with handlers for medium-sized configurations
// (~400 elements): the Linux kernel would crash with a "kmalloc too large".
// The solution: Observe that most handlers are shared. For example, all
// 'name' handlers can share a Handler structure, since they share the same
// read function, read thunk, write function (0), write thunk, and name. This
// introduced a bunch of structure to go from elements to handler indices and
// from handler names to handler indices, but it was worth it: it reduced the
// amount of space required by a normal set of Handlers by about a factor of
// 100 -- there used to be 2998 Handlers, now there are 30.  (Some of this
// space is still not available for system use -- it gets used up by the
// indexing structures, particularly _ehandlers. Every element has its own
// list of "element handlers", even though most elements with element class C
// could share one such list. The space cost is about (48 bytes * # of
// elements) more or less. Detecting this sharing would be harder to
// implement.)

int
Router::find_ehandler(int eindex, const String& name, bool allow_star) const
{
    int eh = _ehandler_first_by_element[eindex];
    int star_h = -1;
    while (eh >= 0) {
	int h = _ehandler_to_handler[eh];
	const String &hname = xhandler(h)->name();
	if (hname == name)
	    return eh;
	else if (hname.length() == 1 && hname[0] == '*')
	    star_h = h;
	eh = _ehandler_next[eh];
    }
    if (allow_star && star_h >= 0 && xhandler(star_h)->writable()) {
	if (xhandler(star_h)->call_write(name, element(eindex), true, ErrorHandler::default_handler()) >= 0)
	    eh = find_ehandler(eindex, name, false);
    }
    return eh;
}

inline Handler
Router::fetch_handler(const Element* e, const String& name)
{
    if (const Handler* h = handler(e, name))
	return *h;
    else
	return Handler(name);
}

void
Router::store_local_handler(int eindex, const Handler& to_store)
{
    int old_eh = find_ehandler(eindex, to_store.name(), false);
    if (old_eh >= 0)
	xhandler(_ehandler_to_handler[old_eh])->_use_count--;
  
    // find the offset in _name_handlers
    int name_index;
    for (name_index = 0;
	 name_index < _handler_first_by_name.size();
	 name_index++) {
	int h = _handler_first_by_name[name_index];
	if (xhandler(h)->_name == to_store._name)
	    break;
    }
    if (name_index == _handler_first_by_name.size())
	_handler_first_by_name.push_back(-1);

    // find a similar handler, if any exists
    int* prev_h = &_handler_first_by_name[name_index];
    int h = *prev_h;
    int* blank_prev_h = 0;
    int blank_h = -1;
    int stored_h = -1;
    while (h >= 0) {
	Handler* han = xhandler(h);
	if (han->compatible(to_store))
	    stored_h = h;
	else if (han->_use_count == 0)
	    blank_h = h, blank_prev_h = prev_h;
	prev_h = &han->_next_by_name;
	h = *prev_h;
    }

    // if none exists, assign this one to a blank spot
    if (stored_h < 0 && blank_h >= 0) {
	stored_h = blank_h;
	*xhandler(stored_h) = to_store;
	xhandler(stored_h)->_use_count = 0;
    }

    // if no blank spot, add a handler
    if (stored_h < 0) {
	if (_free_handler < 0) {
	    int n_handler_bufs = _nhandlers_bufs / HANDLER_BUFSIZ;
	    Handler** new_handler_bufs = new Handler*[n_handler_bufs + 1];
	    Handler* new_handler_buf = new Handler[HANDLER_BUFSIZ];
	    if (!new_handler_buf || !new_handler_bufs) {	// out of memory
		delete[] new_handler_bufs;
		delete[] new_handler_buf;
		if (old_eh >= 0)	// restore use count
		    xhandler(_ehandler_to_handler[old_eh])->_use_count++;
		return;
	    }
	    for (int i = 0; i < HANDLER_BUFSIZ - 1; i++)
		new_handler_buf[i]._next_by_name = _nhandlers_bufs + i + 1;
	    _free_handler = _nhandlers_bufs;
	    memcpy(new_handler_bufs, _handler_bufs, sizeof(Handler*) * n_handler_bufs);
	    new_handler_bufs[n_handler_bufs] = new_handler_buf;
	    delete[] _handler_bufs;
	    _handler_bufs = new_handler_bufs;
	    _nhandlers_bufs += HANDLER_BUFSIZ;
	}
	stored_h = _free_handler;
	_free_handler = xhandler(stored_h)->_next_by_name;
	*xhandler(stored_h) = to_store;
	xhandler(stored_h)->_use_count = 0;
	xhandler(stored_h)->_next_by_name = _handler_first_by_name[name_index];
	_handler_first_by_name[name_index] = stored_h;
    }

    // point ehandler list at new handler
    if (old_eh >= 0)
	_ehandler_to_handler[old_eh] = stored_h;
    else {
	int new_eh = _ehandler_to_handler.size();
	_ehandler_to_handler.push_back(stored_h);
	_ehandler_next.push_back(_ehandler_first_by_element[eindex]);
	_ehandler_first_by_element[eindex] = new_eh;
    }

    // increment use count
    xhandler(stored_h)->_use_count++;

    // perhaps free blank_h
    if (blank_h >= 0 && xhandler(blank_h)->_use_count == 0) {
	*blank_prev_h = xhandler(blank_h)->_next_by_name;
	xhandler(blank_h)->_next_by_name = _free_handler;
	_free_handler = blank_h;
    }
}

void
Router::store_global_handler(const Handler &h)
{
    for (int i = 0; i < nglobalh; i++)
	if (globalh[i]._name == h._name) {
	    globalh[i] = h;
	    globalh[i]._use_count = 1;
	    return;
	}
  
    if (nglobalh >= globalh_cap) {
	int n = (globalh_cap ? 2 * globalh_cap : 4);
	Handler *hs = new Handler[n];
	if (!hs)			// out of memory
	    return;
	for (int i = 0; i < nglobalh; i++)
	    hs[i] = globalh[i];
	delete[] globalh;
	globalh = hs;
	globalh_cap = n;
    }

    globalh[nglobalh] = h;
    globalh[nglobalh]._use_count = 1;
    nglobalh++;
}

inline void
Router::store_handler(const Element* e, const Handler& to_store)
{
    if (e && e->eindex() >= 0)
	e->router()->store_local_handler(e->eindex(), to_store);
    else
	store_global_handler(to_store);
}


// Public functions for finding handlers

/** @brief Return @a router's handler with index @a hindex.
 * @param router the router
 * @param hindex handler index (Router::hindex())
 * @return the Handler, or null if no such handler exists
 *
 * Returns the Handler object on @a router with handler index @a hindex.  If
 * @a hindex >= FIRST_GLOBAL_HANDLER, then returns a global handler.  If @a
 * hindex is < 0 or corresponds to no existing handler, returns null.
 *
 * The return Handler pointer remains valid until the named handler is changed
 * in some way (add_read_handler(), add_write_handler(), set_handler(), or
 * set_handler_flags()).
 */
const Handler*
Router::handler(const Router *router, int hindex)
{
    if (router && hindex >= 0 && hindex < router->_nhandlers_bufs)
	return router->xhandler(hindex);
    else if (hindex >= FIRST_GLOBAL_HANDLER
	     && hindex < FIRST_GLOBAL_HANDLER + nglobalh)
	return &globalh[hindex - FIRST_GLOBAL_HANDLER];
    else
	return 0;
}

/** @brief Return element @a e's handler named @a hname.
 * @param e element, if any
 * @param hname handler name
 * @return the Handler, or null if no such handler exists
 *
 * Searches for element @a e's handler named @a hname.  Returns NULL if no
 * such handler exists.  If @a e is NULL or equal to some root_element(), then
 * this function searches for a global handler named @a hname.
 *
 * The return Handler pointer remains valid until the named handler is changed
 * in some way (add_read_handler(), add_write_handler(), set_handler(), or
 * set_handler_flags()).
 */
const Handler *
Router::handler(const Element* e, const String& hname)
{
    if (e && e->eindex() >= 0) {
	const Router *r = e->router();
	int eh = r->find_ehandler(e->eindex(), hname, true);
	if (eh >= 0)
	    return r->xhandler(r->_ehandler_to_handler[eh]);
    } else {			// global handler
	for (int i = 0; i < nglobalh; i++)
	    if (globalh[i]._name == hname)
		return &globalh[i];
    }
    return 0;
}

/** @brief Return the handler index for element @a e's handler named @a hname.
 * @param e element, if any
 * @param hname handler name
 * @return the handler index, or -1 if no such handler exists
 *
 * Searches for element @a e's handler named @a hname.  Returns -1 if no
 * such handler exists.  If @a e is NULL or equal to some root_element(), then
 * this function searches for a global handler named @a hname.
 *
 * The returned integer is a handler index, which is a number that identifies
 * the handler.  An integer >= FIRST_GLOBAL_HANDLER corresponds to a global
 * handler.
 */
int
Router::hindex(const Element *e, const String &hname)
{
    if (e && e->eindex() >= 0) {
	const Router *r = e->router();
	int eh = r->find_ehandler(e->eindex(), hname, true);
	if (eh >= 0)
	    return r->_ehandler_to_handler[eh];
    } else {			// global handler
	for (int i = 0; i < nglobalh; i++)
	    if (globalh[i]._name == hname)
		return FIRST_GLOBAL_HANDLER + i;
    }
    return -1;
}

/** @brief Return the handler indexes for element @a e's handlers.
 * @param e element, if any
 * @param result collector for handler indexes
 *
 * Iterates over all element @a e's handlers, and appends their handler
 * indexes to @a result.  If @a e is NULL or equal to some root_element(),
 * then iterates over the global handlers.
 */
void
Router::element_hindexes(const Element *e, Vector<int> &result)
{
    if (e && e->eindex() >= 0) {
	const Router *r = e->router();
	for (int eh = r->_ehandler_first_by_element[e->eindex()];
	     eh >= 0;
	     eh = r->_ehandler_next[eh])
	    result.push_back(r->_ehandler_to_handler[eh]);
    } else {
	for (int i = 0; i < nglobalh; i++)
	    result.push_back(FIRST_GLOBAL_HANDLER + i);
    }
}


// Public functions for storing handlers

/** @brief Add an @a e.@a hname read handler.
 * @param e element, if any
 * @param hname handler name
 * @param hook read hook
 * @param user_data user data for read hook
 * @param flags additional flags to set (Handler::flags())
 *
 * Adds a read handler named @a hname for element @a e.  If @a e is NULL or
 * equal to some root_element(), then adds a global read handler.  The
 * handler's hook function is @a hook.  When the read handler is triggered,
 * Click will call @a hook(@a e, @a user_data).
 *
 * Any previous read handler with the same name and element is replaced.  Any
 * uniform handler function (see set_handler()) is replaced.  Any write-only
 * handler (add_write_handler()) remains.
 *
 * The new handler's flags equal the old flags or'ed with @a flags.  Any
 * special flags in @a flags are ignored.
 *
 * To create a read handler with parameters, you must use @a set_handler().
 *
 * @sa add_write_handler(), set_handler(), set_handler_flags()
 */
void
Router::add_read_handler(const Element *e, const String &hname,
			 ReadHandlerHook hook, void *user_data, uint32_t flags)
{
    Handler to_add = fetch_handler(e, hname);
    if (to_add._flags & Handler::UNIFORM) {
	to_add._hook.rw.w = 0;
	to_add._thunk2 = 0;
	to_add._flags &= ~Handler::SPECIAL_FLAGS;
    }
    to_add._hook.rw.r = hook;
    to_add._thunk1 = user_data;
    to_add._flags |= Handler::OP_READ | (flags & ~Handler::SPECIAL_FLAGS);
    store_handler(e, to_add);
}

/** @brief Add an @a e.@a hname write handler.
 * @param e element, if any
 * @param hname handler name
 * @param hook read hook
 * @param user_data user data for write hook
 * @param flags additional flags to set (Handler::flags())
 *
 * Adds a write handler named @a hname for element @a e.  If @a e is NULL or
 * equal to some root_element(), then adds a global write handler.  The
 * handler's hook function is @a hook.  When the write handler is triggered,
 * Click will call @a hook(data, @a e, @a user_data, errh).
 *
 * Any previous write handler with the same name and element is replaced.  Any
 * uniform handler function (see set_handler()) is replaced.  Any read-only
 * handler (add_read_handler()) remains.
 *
 * The new handler's flags equal the old flags or'ed with @a flags.  Any
 * special flags in @a flags are ignored.
 *
 * @sa add_read_handler(), set_handler(), set_handler_flags()
 */
void
Router::add_write_handler(const Element *e, const String &hname,
			  WriteHandlerHook hook, void *user_data, uint32_t flags)
{
    Handler to_add = fetch_handler(e, hname);
    if (to_add._flags & Handler::UNIFORM) {
	to_add._hook.rw.r = 0;
	to_add._thunk1 = 0;
	to_add._flags &= ~Handler::SPECIAL_FLAGS;
    }
    to_add._hook.rw.w = hook;
    to_add._thunk2 = user_data;
    to_add._flags |= Handler::OP_WRITE | (flags & ~Handler::SPECIAL_FLAGS);
    store_handler(e, to_add);
}

/** @brief Add a uniform @a e.@a hname handler.
 * @param e element, if any
 * @param hname handler name
 * @param flags flags to set (Handler::flags())
 * @param hook uniform handler hook
 * @param user_data1 first user data for hook
 * @param user_data2 second user data for hook
 *
 * Sets a handler named @a hname for element @a e.  If @a e is NULL or equal
 * to some root_element(), then sets a global handler.  The handler's hook
 * function is @a hook.  The resulting handler is a read handler if @a flags
 * contains Handler::OP_READ, and a write handler if @a flags contains
 * Handler::OP_WRITE.  If the flags contain Handler::READ_PARAM, then any read
 * handler will accept parameters.
 *
 * When the handler is triggered, Click will call @a hook(operation, data, @a
 * e, h, errh), where:
 *
 * <ul>
 * <li>"operation" is Handler::OP_READ or Handler::OP_WRITE;</li>
 * <li>"data" is the handler data (empty for reads without parameters);</li>
 * <li>"h" is a pointer to a Handler object; and</li>
 * <li>"errh" is an ErrorHandler.</li>
 * </ul>
 *
 * Any previous handlers with the same name and element are replaced.
 *
 * The new handler's flags equal @a flags or'ed with Handler::UNIFORM.
 *
 * @sa add_read_handler(), add_write_handler(), set_handler_flags()
 */
void
Router::set_handler(const Element *e, const String &hname, uint32_t flags,
		    HandlerHook hook, void *user_data1, void *user_data2)
{
    Handler to_add(hname);
    to_add._hook.h = hook;
    to_add._thunk1 = user_data1;
    to_add._thunk2 = user_data2;
    to_add._flags = flags | Handler::UNIFORM;
    store_handler(e, to_add);
}

/** @brief Change the @a e.@a hname handler's flags.
 * @param e element, if any
 * @param hname handler name
 * @param set_flags flags to set (Handler::flags())
 * @param clear_flags flags to clear (Handler::flags())
 * @return 0 if the handler existed, -1 otherwise
 *
 * Changes the handler flags for the handler named @a hname on element @a e.
 * If @a e is NULL or equal to some root_element(), then changes a global
 * handler.  The handler's flags are changed by clearing the @a clear_flags
 * and then setting the @a set_flags, except that the special flags
 * (Handler::SPECIAL_FLAGS) are unchanged.
 *
 * @sa add_read_handler(), add_write_handler(), set_handler()
 */
int
Router::set_handler_flags(const Element *e, const String &hname,
			  uint32_t set_flags, uint32_t clear_flags)
{
    Handler to_add = fetch_handler(e, hname);
    if (to_add._use_count > 0) {	// only modify existing handlers
	clear_flags &= ~Handler::SPECIAL_FLAGS;
	set_flags &= ~Handler::SPECIAL_FLAGS;
	to_add._flags = (to_add._flags & ~clear_flags) | set_flags;
	store_handler(e, to_add);
	return 0;
    } else
	return -1;
}


// ATTACHMENTS

void*
Router::attachment(const String &name) const
{
    for (int i = 0; i < _attachments.size(); i++)
	if (_attachment_names[i] == name)
	    return _attachments[i];
    return 0;
}

void*&
Router::force_attachment(const String &name)
{
    for (int i = 0; i < _attachments.size(); i++)
	if (_attachment_names[i] == name)
	    return _attachments[i];
    _attachment_names.push_back(name);
    _attachments.push_back(0);
    return _attachments.back();
}

void *
Router::set_attachment(const String &name, void *value)
{
    for (int i = 0; i < _attachments.size(); i++)
	if (_attachment_names[i] == name) {
	    void *v = _attachments[i];
	    _attachments[i] = value;
	    return v;
	}
    _attachment_names.push_back(name);
    _attachments.push_back(value);
    return 0;
}

ErrorHandler *
Router::chatter_channel(const String &name) const
{
    if (!name || name == "default")
	return ErrorHandler::default_handler();
    else if (void *v = attachment("ChatterChannel." + name))
	return (ErrorHandler *)v;
    else
	return ErrorHandler::silent_handler();
}

/** @brief Create a new basic signal.
 * @param[out] signal the new signal
 *
 * Creates a new basic NotifierSignal and stores it in @a signal.  The signal
 * is initially active.
 *
 * @note Users will not generally call this function directly;
 * Notifier::initialize() will call it as required.
 *
 * @return >= 0 on success, < 0 on failure
 * @sa NotifierSignal
 */
int
Router::new_notifier_signal(NotifierSignal &signal)
{
    if (!_notifier_signals)
	_notifier_signals = new atomic_uint32_t[NOTIFIER_SIGNALS_CAPACITY / 32];
    if (_n_notifier_signals >= NOTIFIER_SIGNALS_CAPACITY)
	return -1;
    else {
	signal = NotifierSignal(&_notifier_signals[_n_notifier_signals / 32], 1 << (_n_notifier_signals % 32));
	signal.set_active(true);
	_n_notifier_signals++;
	return 0;
    }
}

int
Router::notifier_signal_id(const atomic_uint32_t *signal)
{
    if (_notifier_signals && signal >= _notifier_signals
	&& signal < _notifier_signals + (NOTIFIER_SIGNALS_CAPACITY / 32))
	return signal - _notifier_signals;
    else
	return -1;
}

int
ThreadSched::initial_home_thread_id(Task *, bool)
{
    return 0;
}

/** @cond never */
/** @brief  Create (if necessary) and return the NameInfo object for this router.
 *
 * Users never need to call this. */
NameInfo*
Router::force_name_info()
{
    if (!_name_info)
	_name_info = new NameInfo;
    return _name_info;
}
/** @endcond never */


// PRINTING

/** @brief Unparse the router's requirements into @a sa.
 *
 * Appends at most one require() statement to @a sa. */
void
Router::unparse_requirements(StringAccum &sa, const String &indent) const
{
    // requirements
    if (_requirements.size())
	sa << indent << "require(" << cp_unargvec(_requirements) << ");\n\n";
}

/** @brief Unparse declarations of the router's elements into @a sa.
 *
 * Appends this router's elements' declarations to @a sa.  If the router is
 * initialized, then each element's configuration string is found by
 * Element::configuration(), which might include post-initialization changes.
 */
void
Router::unparse_declarations(StringAccum &sa, const String &indent) const
{  
  // element classes
  Vector<String> conf;
  for (int i = 0; i < nelements(); i++) {
    sa << indent << _element_names[i] << " :: " << _elements[i]->class_name();
    String conf = (initialized() ? _elements[i]->configuration() : _element_configurations[i]);
    if (conf.length())
      sa << "(" << conf << ")";
    sa << ";\n";
  }
  
  if (nelements() > 0)
    sa << "\n";
}

/** @brief Unparse the router's connections into @a sa.
 *
 * Appends this router's connections to @a sa in parseable format. */
void
Router::unparse_connections(StringAccum &sa, const String &indent) const
{  
  int nhookup = _hookup_from.size();
  Vector<int> next(nhookup, -1);
  Bitvector startchain(nhookup, true);
  for (int c = 0; c < nhookup; c++) {
    const Hookup &ht = _hookup_to[c];
    if (ht.port != 0) continue;
    int result = -1;
    for (int d = 0; d < nhookup; d++)
      if (d != c && _hookup_from[d] == ht) {
	result = d;
	if (_hookup_to[d].port == 0)
	  break;
      }
    if (result >= 0) {
      next[c] = result;
      startchain[result] = false;
    }
  }
  
  // print hookup
  Bitvector used(nhookup, false);
  bool done = false;
  while (!done) {
    // print chains
    for (int c = 0; c < nhookup; c++) {
      if (used[c] || !startchain[c]) continue;
      
      const Hookup &hf = _hookup_from[c];
      sa << indent << _element_names[hf.idx];
      if (hf.port)
	sa << " [" << hf.port << "]";
      
      int d = c;
      while (d >= 0 && !used[d]) {
	if (d == c) sa << " -> ";
	else sa << "\n" << indent << "    -> ";
	const Hookup &ht = _hookup_to[d];
	if (ht.port)
	  sa << "[" << ht.port << "] ";
	sa << _element_names[ht.idx];
	used[d] = true;
	d = next[d];
      }
      
      sa << ";\n";
    }

    // add new chains to include cycles
    done = true;
    for (int c = 0; c < nhookup && done; c++)
      if (!used[c])
	startchain[c] = true, done = false;
  }
}

/** @brief Unparse this router into @a sa.
 *
 * Calls unparse_requirements(), unparse_declarations(), and
 * unparse_connections(), in that order.  Each line is prefixed by @a indent.
 */
void
Router::unparse(StringAccum &sa, const String &indent) const
{
    unparse_requirements(sa, indent);
    unparse_declarations(sa, indent);
    unparse_connections(sa, indent);
}

/** @brief Return a string representing @a e's ports.
 * @param e element
 *
 * The returned string is suitable for an element's <tt>ports</tt> handler.
 * It lists the input ports, then the output ports.  For example:
 *
 * <pre>
 * 1 input
 * push~   -       InfiniteSource@@1 [0], InfiniteSource@@4 [0]
 * 1 output
 * push~   -       [0] Align@@3
 * </pre>
 *
 * In the port lines, the first column describes the processing type (a tilde
 * suffix represents an agnostic port); the second column lists any packet
 * statistics available; and the third column lists other ports that are
 * connected to this port.
 */
String
Router::element_ports_string(const Element *e) const
{
    if (!e || e->eindex() < 0 || e->router() != this)
	return String();
  
    StringAccum sa;
    Vector<int> pers(e->ninputs() + e->noutputs(), 0);
    int *in_pers = pers.begin();
    int *out_pers = pers.begin() + e->ninputs();
    e->processing_vector(in_pers, out_pers, 0);

    sa << e->ninputs() << (e->ninputs() == 1 ? " input\n" : " inputs\n");
    for (int i = 0; i < e->ninputs(); i++) {
	// processing
	const char *persid = (e->input_is_pull(i) ? "pull" : "push");
	if (in_pers[i] == Element::VAGNOSTIC)
	    sa << persid << "~\t";
	else
	    sa << persid << "\t";
    
	// counts
#if CLICK_STATS >= 1
	if (e->input_is_pull(i) || CLICK_STATS >= 2)
	    sa << e->input(i).npackets() << "\t";
	else
#endif
	    sa << "-\t";
    
	// connections
	Hookup h(e->eindex(), i);
	const char *sep = "";
	for (int c = 0; c < _hookup_from.size(); c++)
	    if (_hookup_to[c] == h) {
		sa << sep << _element_names[_hookup_from[c].idx]
		   << " [" << _hookup_from[c].port << "]";
		sep = ", ";
	    }
	sa << "\n";
    }

    sa << e->noutputs() << (e->noutputs() == 1 ? " output\n" : " outputs\n");
    for (int i = 0; i < e->noutputs(); i++) {
	// processing
	const char *persid = (e->output_is_push(i) ? "push" : "pull");
	if (out_pers[i] == Element::VAGNOSTIC)
	    sa << persid << "~\t";
	else
	    sa << persid << "\t";
    
	// counts
#if CLICK_STATS >= 1
	if (e->output_is_push(i) || CLICK_STATS >= 2)
	    sa << e->output(i).npackets() << "\t";
	else
#endif
	    sa << "-\t";
    
	// hookup
	Hookup h(e->eindex(), i);
	const char *sep = "";
	for (int c = 0; c < _hookup_from.size(); c++)
	    if (_hookup_from[c] == h) {
		sa << sep << "[" << _hookup_to[c].port << "] "
		   << _element_names[_hookup_to[c].idx];
		sep = ", ";
	    }
	sa << "\n";
    }
  
    return sa.take_string();
}


// STATIC INITIALIZATION, DEFAULT GLOBAL HANDLERS

enum { GH_VERSION, GH_CONFIG, GH_FLATCONFIG, GH_LIST, GH_REQUIREMENTS,
       GH_DRIVER };

String
Router::router_read_handler(Element *e, void *thunk)
{
    Router *r = (e ? e->router() : 0);
    switch (reinterpret_cast<intptr_t>(thunk)) {

      case GH_VERSION:
	return String(CLICK_VERSION);
    
      case GH_CONFIG:
	if (r)
	    return r->configuration_string();
	break;

      case GH_FLATCONFIG:
	if (r) {
	    StringAccum sa;
	    r->unparse(sa);
	    return sa.take_string();
	}
	break;

      case GH_LIST:
	if (r) {
	    StringAccum sa;
	    sa << r->nelements() << "\n";
	    for (int i = 0; i < r->nelements(); i++)
		sa << r->_element_names[i] << "\n";
	    return sa.take_string();
	}
	break;

      case GH_REQUIREMENTS:
	if (r) {
	    StringAccum sa;
	    for (int i = 0; i < r->_requirements.size(); i++)
		sa << r->_requirements[i] << "\n";
	    return sa.take_string();
	}
	break;

      case GH_DRIVER:
#if CLICK_NS
	return String::stable_string("ns", 2);
#elif CLICK_USERLEVEL
	return String::stable_string("userlevel", 9);
#elif CLICK_LINUXMODULE
	return String::stable_string("linuxmodule", 11);
#elif CLICK_BSDMODULE
	return String::stable_string("bsdmodule", 9);
#else
	break;
#endif
    
    }
    return String();
}

static int
stop_global_handler(const String &s, Element *e, void *, ErrorHandler *errh)
{
    if (e) {
	int n = 1;
	(void) cp_integer(cp_uncomment(s), &n);
	e->router()->adjust_runcount(-n);
    } else
	errh->message("no router to stop");
    return 0;
}

void
Router::static_initialize()
{
    if (!nglobalh) {
	Handler::the_blank_handler = new Handler("<bad handler>");
	add_read_handler(0, "version", router_read_handler, (void *)GH_VERSION);
	add_read_handler(0, "config", router_read_handler, (void *)GH_CONFIG);
	add_read_handler(0, "flatconfig", router_read_handler, (void *)GH_FLATCONFIG);
	add_read_handler(0, "list", router_read_handler, (void *)GH_LIST);
	add_read_handler(0, "requirements", router_read_handler, (void *)GH_REQUIREMENTS);
	add_read_handler(0, "driver", router_read_handler, (void *)GH_DRIVER);
	add_write_handler(0, "stop", stop_global_handler, 0);
    }
}

void
Router::static_cleanup()
{
    delete[] globalh;
    globalh = 0;
    nglobalh = globalh_cap = 0;
    delete Handler::the_blank_handler;
}


#if CLICK_NS

simclick_node_t *
Router::simnode() const
{
    return master()->simnode();
}

int
Router::sim_get_ifid(const char* ifname) {
    return simclick_sim_command(_master->simnode(), SIMCLICK_IFID_FROM_NAME, ifname);
}

Vector<int> *
Router::sim_listenvec(int ifid) {
  for (int i = 0; i < _listenvecs.size(); i++)
    if (_listenvecs[i]->at(0) == ifid)
      return _listenvecs[i];
  Vector<int> *new_vec = new Vector<int>(1, ifid);
  if (new_vec)
    _listenvecs.push_back(new_vec);
  return new_vec;
}

int
Router::sim_listen(int ifid, int element) {
  if (Vector<int> *vec = sim_listenvec(ifid)) {
    for (int i = 1; i < vec->size(); i++)
      if ((*vec)[i] == element)
	return 0;
    vec->push_back(element);
    return 0;
  } else
    return -1;
}

int
Router::sim_write(int ifid,int ptype,const unsigned char* data,int len,
		     simclick_simpacketinfo* pinfo) {
    return simclick_sim_send(_master->simnode(),ifid,ptype,data,len,pinfo);
}

int
Router::sim_if_ready(int ifid) {
    return simclick_sim_command(_master->simnode(), SIMCLICK_IF_READY, ifid);
}

int
Router::sim_incoming_packet(int ifid, int ptype, const unsigned char* data,
			    int len, simclick_simpacketinfo* pinfo) {
  if (Vector<int> *vec = sim_listenvec(ifid))
    for (int i = 1; i < vec->size(); i++)
      ((FromSimDevice *)element((*vec)[i]))->incoming_packet(ifid, ptype, data,
							     len, pinfo);
  return 0;
}

void
Router::sim_trace(const char* event) {
    simclick_sim_command(_master->simnode(), SIMCLICK_TRACE, event);
}

int
Router::sim_get_node_id() {
    return simclick_sim_command(_master->simnode(), SIMCLICK_GET_NODE_ID);
}

int
Router::sim_get_next_pkt_id() {
    return simclick_sim_command(_master->simnode(), SIMCLICK_GET_NEXT_PKT_ID);
}

#endif // CLICK_NS

#if CLICK_USERLEVEL
// Vector template instance
# include <click/vector.cc>
#endif
CLICK_ENDDECLS
