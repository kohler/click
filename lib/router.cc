#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "router.hh"
#include "bitvector.hh"
#include "error.hh"
#include "straccum.hh"
#include "elemfilter.hh"
#ifndef __KERNEL__
# include "timer.hh"
#endif
#include <stdarg.h>
#include <unistd.h>

#ifdef __KERNEL__
static struct wait_queue *router_run_scheduled_wait_queue = 0;

static void
router_run_scheduled_task(void *thunk)
{
  Router *r = (Router *)thunk;
#if CLICK_STATS > 0 || XCYC > 0
  entering_ipb();
#endif
  r->run_scheduled();
#if CLICK_STATS > 0 || XCYC > 0
  leaving_ipb();
#endif
  wake_up_interruptible(&router_run_scheduled_wait_queue);
}
#endif

Router::Router()
  : _closed(0), _initialized(0), _have_connections(0), _have_hookpidx(0),
    _please_stop_driver(0)
{
  initialize_head();
#ifdef __KERNEL__
  _schedule_task.next = 0;
  _schedule_task.sync = 0;
  _schedule_task.routine = router_run_scheduled_task;
  _schedule_task.data = 0;
#endif
}

Router::~Router()
{
  if (_initialized)
    for (int i = 0; i < _elements.size(); i++)
      _elements[i]->uninitialize(this);
  for (int i = 0; i < _elements.size(); i++)
    _elements[i]->unuse();
#ifdef __KERNEL__
  initialize_head();		// get rid of scheduled wait queue
  _please_stop_driver = true;	// XXX races?
  while (_schedule_task.sync)
    interruptible_sleep_on(&router_run_scheduled_wait_queue);
#endif
}


// ACCESS

Element *
Router::find(const String &id, ErrorHandler *errh) const
{
  int got = -1;
  for (int i = 0; i < _elements.size(); i++)
    if (_elements[i]->id() == id) {
      if (got >= 0) {
	if (errh) errh->error("more than one element named `%s'",
			      String(id).cc());
	return 0;
      } else
	got = i;
    }
  if (got >= 0)
    return _elements[got];
  else {
    if (errh) errh->error("no element named `%s'", String(id).cc());
    return 0;
  }
}

int
Router::findex(Element *f)
{
  for (int i = 0; i < _elements.size(); i++)
    if (_elements[i] == f)
      return i;
  return -1;
}

Element *
Router::element(int fi) const
{
  if (fi < 0 || fi >= nelements())
    return 0;
  else
    return _elements[fi];
}

const String &
Router::configuration(int fi) const
{
  if (fi < 0 || fi >= nelements())
    return String::null_string();
  else
    return _configurations[fi];
}


// CREATION 

int
Router::add(Element *f, const String &conf)
{
  if (_closed || !f) return -1;
  _elements.push_back(f);
  _configurations.push_back(conf);
  f->use();
  return _elements.size() - 1;
}

int
Router::connect(int from_idx, int from_port, int to_idx, int to_port)
{
  if (_closed) return -1;
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

int
Router::close(ErrorHandler *errh)
{
  _closed = true;
  return check_hookup_elements(errh);
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
  if (!errh) errh = ErrorHandler::default_handler();
  int before = errh->nerrors();
  
  // Check each hookup to ensure it connects valid elements
  for (int c = 0; c < _hookup_from.size(); c++) {
    Hookup &hfrom = _hookup_from[c];
    Hookup &hto = _hookup_to[c];
    int before = errh->nerrors();
    
    if (hfrom.idx < 0 || hfrom.idx >= nelements() || !_elements[hfrom.idx])
      errh->error("bad element number `%d'", hfrom.idx);
    if (hto.idx < 0 || hto.idx >= nelements() || !_elements[hto.idx])
      errh->error("bad element number `%d'", hto.idx);
    if (hfrom.port < 0)
      errh->error("bad port number `%d'", hfrom.port);
    if (hto.port < 0)
      errh->error("bad port number `%d'", hto.port);
    
    // remove the connection if there were errors
    if (errh->nerrors() != before) {
      remove_hookup(c);
      c--;
    }
  }
  
  return (errh->nerrors() == before ? 0 : -1);
}

void
Router::notify_hookup_range()
{
  // Count inputs and outputs, and notify elements how many they have
  Vector<int> nin(nelements(), -1);
  Vector<int> nout(nelements(), -1);
  for (int c = 0; c < _hookup_from.size(); c++) {
    if (_hookup_from[c].port > nout[_hookup_from[c].idx])
      nout[_hookup_from[c].idx] = _hookup_from[c].port;
    if (_hookup_to[c].port > nin[_hookup_to[c].idx])
      nin[_hookup_to[c].idx] = _hookup_to[c].port;
  }
  for (int f = 0; f < nelements(); f++) {
    _elements[f]->notify_inputs(nin[f] + 1);
    _elements[f]->notify_outputs(nout[f] + 1);
  }
}

void
Router::check_hookup_range(ErrorHandler *errh)
{
  // Check each hookup to ensure its port numbers are within range
  for (int c = 0; c < _hookup_from.size(); c++) {
    Hookup &hfrom = _hookup_from[c];
    Hookup &hto = _hookup_to[c];
    int before = errh->nerrors();
    
    if (hfrom.port >= _elements[hfrom.idx]->noutputs())
      hookup_error(hfrom, true, "`%e' has no %s %d", errh);
    if (hto.port >= _elements[hto.idx]->ninputs())
      hookup_error(hto, false, "`%e' has no %s %d", errh);
    
    // remove the connection if there were errors
    if (errh->nerrors() != before) {
      remove_hookup(c);
      c--;
    }
  }
}

void
Router::check_hookup_completeness(ErrorHandler *errh)
{
  Bitvector used_outputs(noutput_pidx(), false);
  Bitvector used_inputs(ninput_pidx(), false);
  
  // Check each hookup to ensure it doesn't reuse a port.
  // Completely duplicate connections never got into the Router
  for (int c = 0; c < _hookup_from.size(); c++) {
    Hookup &hfrom = _hookup_from[c];
    Hookup &hto = _hookup_to[c];
    int before = errh->nerrors();
    
    int from_pidx = _output_pidx[hfrom.idx] + hfrom.port;
    int to_pidx = _input_pidx[hto.idx] + hto.port;
    if (used_outputs[from_pidx]
	&& _elements[hfrom.idx]->output_is_push(hfrom.port))
      hookup_error(hfrom, true, "can't reuse `%e' push %s %d", errh);
    else if (used_inputs[to_pidx]
	     && _elements[hto.idx]->input_is_pull(hto.port))
      hookup_error(hto, false, "can't reuse `%e' pull %s %d", errh);
    
    // remove the connection if there were errors
    if (errh->nerrors() != before) {
      remove_hookup(c);
      c--;
    } else {
      used_outputs[from_pidx] = true;
      used_inputs[to_pidx] = true;
    }
  }

  // Check for unused inputs and outputs.
  for (int i = 0; i < ninput_pidx(); i++)
    if (!used_inputs[i]) {
      Element *f = _elements[input_pidx_element(i)];
      int p = input_pidx_port(i);
      element_lerror(errh, f, "`%e' %s input %d not connected", f,
		     (f->input_is_pull(p) ? "pull" : "push"), p);
    }
  for (int i = 0; i < noutput_pidx(); i++)
    if (!used_outputs[i]) {
      Element *f = _elements[output_pidx_element(i)];
      int p = output_pidx_port(i);
      element_lerror(errh, f, "`%e' %s output %d not connected", f,
		     (f->output_is_push(p) ? "push" : "pull"), p);
    }

#if 0
  int before = errh->nerrors();
  for (int fi = 0; fi < nelements(); fi++) {
    Element *f = _elements[fi];
    for (int i = 0; i < f->ninputs(); i++)
      if (!f->input(i) && f->input(i).allowed())
	element_lerror(errh, f, "`%e' pull input %d not connected", f, i);
    for (int o = 0; o < f->noutputs(); o++)
      if (!f->output(o) && f->output(o).allowed())
	element_lerror(errh, f, "`%e' push output %d not connected", f, o);
  }
#endif
}


// PORT INDEXES

void
Router::make_pidxes()
{
  _input_pidx.clear();
  _input_pidx.push_back(0);
  _output_pidx.clear();
  _output_pidx.push_back(0);
  for (int i = 0; i < _elements.size(); i++) {
    Element *f = _elements[i];
    _input_pidx.push_back(_input_pidx.back() + f->ninputs());
    _output_pidx.push_back(_output_pidx.back() + f->noutputs());
    for (int j = 0; j < f->ninputs(); j++)
      _input_fidx.push_back(i);
    for (int j = 0; j < f->noutputs(); j++)
      _output_fidx.push_back(i);
  }
}

extern inline int
Router::input_pidx(const Hookup &h) const
{
  return _input_pidx[h.idx] + h.port;
}

extern inline int
Router::input_pidx_element(int pidx) const
{
  return _input_fidx[pidx];
}

extern inline int
Router::input_pidx_port(int pidx) const
{
  return pidx - _input_pidx[_input_fidx[pidx]];
}

extern inline int
Router::output_pidx(const Hookup &h) const
{
  return _output_pidx[h.idx] + h.port;
}

extern inline int
Router::output_pidx_element(int pidx) const
{
  return _output_fidx[pidx];
}

extern inline int
Router::output_pidx_port(int pidx) const
{
  return pidx - _output_pidx[_output_fidx[pidx]];
}

void
Router::make_hookpidxes()
{
  if (_have_hookpidx) return;
  for (int c = 0; c < _hookup_from.size(); c++) {
    int p1 = _output_pidx[_hookup_from[c].idx] + _hookup_from[c].port;
    _hookpidx_from.push_back(p1);
    int p2 = _input_pidx[_hookup_to[c].idx] + _hookup_to[c].port;
    _hookpidx_to.push_back(p2);
  }
}

// PROCESSING

int
Router::processing_error(const Hookup &hfrom, const Hookup &hto, int idx,
			 int processing_from, ErrorHandler *errh)
{
  const char *type1 = (processing_from == Element::PUSH ? "push" : "pull");
  const char *type2 = (processing_from == Element::PUSH ? "pull" : "push");
  if (idx < _hookup_from.size())
    errh->error("`%e' %s output %d connected to `%e' %s input %d",
		_elements[hfrom.idx], type1, hfrom.port,
		_elements[hto.idx], type2, hto.port);
  else
    errh->error("agnostic `%e' in mixed context: %s input %d, %s output %d",
		_elements[hfrom.idx], type2, hto.port, type1, hfrom.port);
  return -1;
}

int
Router::check_push_and_pull(ErrorHandler *errh)
{
  if (!errh) errh = ErrorHandler::default_handler();
  
  // set up processing vectors
  Vector<int> input_pers(ninput_pidx(), 0);
  Vector<int> output_pers(noutput_pidx(), 0);
  for (int f = 0; f < nelements(); f++)
    _elements[f]->processing_vector(input_pers, _input_pidx[f],
				    output_pers, _output_pidx[f]);
  
  // add fake connections for agnostics
  Vector<Hookup> hookup_from = _hookup_from;
  Vector<Hookup> hookup_to = _hookup_to;
  for (int i = 0; i < ninput_pidx(); i++)
    if (input_pers[i] == Element::AGNOSTIC) {
      int fid = _input_fidx[i];
      int port = i - _input_pidx[fid];
      Bitvector bv = _elements[fid]->forward_flow(port);
      int opidx = _output_pidx[fid];
      for (int j = 0; j < bv.size(); j++)
	if (bv[j] && output_pers[opidx+j] == Element::AGNOSTIC) {
	  hookup_from.push_back(Hookup(fid, j));
	  hookup_to.push_back(Hookup(fid, port));
	}
    }
  
  int before = errh->nerrors();
  
  // spread personalities
  while (true) {
    
    bool changed = false;
    for (int c = 0; c < hookup_from.size(); c++) {
      int offf = output_pidx(hookup_from[c]);
      int offt = input_pidx(hookup_to[c]);
      int pf = output_pers[offf];
      int pt = input_pers[offt];
      
      switch (pt) {
	
       case Element::AGNOSTIC:
	if (pf != Element::AGNOSTIC) {
	  input_pers[offt] = pf;
	  changed = true;
	}
	break;
	
       case Element::PUSH:
       case Element::PULL:
	if (pf == Element::AGNOSTIC) {
	  output_pers[offf] = pt;
	  changed = true;
	} else if (pf != pt) {
	  processing_error(hookup_from[c], hookup_to[c], c, pf, errh);
	  hookup_from[c] = hookup_from.back();
	  hookup_to[c] = hookup_to.back();
	  hookup_from.pop_back();
	  hookup_to.pop_back();
	  c--;
	}
	break;
	
      }
    }
    
    if (!changed) break;
  }
  
  if (errh->nerrors() != before)
    return -1;
  
  for (int f = 0; f < nelements(); f++)
    _elements[f]->set_processing_vector(input_pers, _input_pidx[f],
					output_pers, _output_pidx[f]);
  return 0;
}


// SET CONNECTIONS

int
Router::element_lerror(ErrorHandler *errh, Element *f,
		       const char *format, ...) const
{
  va_list val;
  va_start(val, format);
  errh->verror(ErrorHandler::Error, f->landmark(), format, val);
  va_end(val);
  return -1;
}

void
Router::set_connections()
{
  // actually assign ports
  for (int c = 0; c < _hookup_from.size(); c++) {
    Hookup &hfrom = _hookup_from[c];
    Element *fromf = _elements[hfrom.idx];
    Hookup &hto = _hookup_to[c];
    Element *tof = _elements[hto.idx];
    fromf->set_output(hfrom.port, tof, hto.port);
    tof->set_input(hto.port, fromf, hfrom.port);
  }
}


// FLOWS

int
Router::downstream_inputs(Element *first_element, int first_output,
			  ElementFilter *stop_filter, Bitvector &results)
{
  if (!_have_connections) return -1;
  make_hookpidxes();
  int nipidx = ninput_pidx();
  int nopidx = noutput_pidx();
  int nhook = _hookpidx_from.size();
  
  Bitvector old_results(nipidx, false);
  results.assign(nipidx, false);
  Bitvector diff;
  
  Bitvector outputs(nopidx, false);
  int first_fid = findex(first_element);
  if (first_fid < 0) return -1;
  for (int i = 0; i < _elements[first_fid]->noutputs(); i++)
    if (first_output < 0 || first_output == i)
      outputs[_output_pidx[first_fid]+i] = true;
  
  while (true) {
    old_results = results;
    for (int i = 0; i < nhook; i++)
      if (outputs[_hookpidx_from[i]])
	results[_hookpidx_to[i]] = true;
    
    diff = results - old_results;
    if (diff.zero()) break;
    
    outputs.assign(nopidx, false);
    for (int i = 0; i < nipidx; i++)
      if (diff[i]) {
	int facno = _input_fidx[i];
	if (!stop_filter || !stop_filter->match(_elements[facno])) {
	  Bitvector bv = _elements[facno]->forward_flow(input_pidx_port(i));
	  outputs.or_at(bv, _output_pidx[facno]);
	}
      }
  }
  
  return 0;
}

int
Router::downstream_elements(Element *first_element, int first_output,
			    ElementFilter *stop_filter,
			    Vector<Element *> &results)
{
  Bitvector bv;
  if (downstream_inputs(first_element, first_output, stop_filter, bv) < 0)
    return -1;
  int last_input_fidx = -1;
  for (int i = 0; i < ninput_pidx(); i++)
    if (bv[i] && _input_fidx[i] != last_input_fidx) {
      last_input_fidx = _input_fidx[i];
      results.push_back(_elements[last_input_fidx]);
    }
  return 0;
}

int
Router::downstream_elements(Element *first_element, int first_output,
			    Vector<Element *> &results)
{
  return downstream_elements(first_element, first_output, 0, results);
}

int
Router::downstream_elements(Element *first_element, Vector<Element *> &results)
{
  return downstream_elements(first_element, -1, 0, results);
}

int
Router::upstream_outputs(Element *first_element, int first_input,
			 ElementFilter *stop_filter, Bitvector &results)
{
  if (!_have_connections) return -1;
  make_hookpidxes();
  int nipidx = ninput_pidx();
  int nopidx = noutput_pidx();
  int nhook = _hookpidx_from.size();
  
  Bitvector old_results(nopidx, false);
  results.assign(nopidx, false);
  Bitvector diff;
  
  Bitvector inputs(nipidx, false);
  int first_fid = findex(first_element);
  if (first_fid < 0) return -1;
  for (int i = 0; i < _elements[first_fid]->ninputs(); i++)
    if (first_input < 0 || first_input == i)
      inputs[_input_pidx[first_fid]+i] = true;
  
  while (true) {
    old_results = results;
    for (int i = 0; i < nhook; i++)
      if (inputs[_hookpidx_to[i]])
	results[_hookpidx_from[i]] = true;
    
    diff = results - old_results;
    if (diff.zero()) break;
    
    inputs.assign(nipidx, false);
    for (int i = 0; i < nopidx; i++)
      if (diff[i]) {
	int facno = _output_fidx[i];
	if (!stop_filter || !stop_filter->match(_elements[facno])) {
	  Bitvector bv = _elements[facno]->backward_flow(output_pidx_port(i));
	  inputs.or_at(bv, _input_pidx[facno]);
	}
      }
  }
  
  return 0;
}

int
Router::upstream_elements(Element *first_element, int first_input,
			  ElementFilter *stop_filter,
			  Vector<Element *> &results)
{
  Bitvector bv;
  if (upstream_outputs(first_element, first_input, stop_filter, bv) < 0)
    return -1;
  int last_output_fidx = -1;
  for (int i = 0; i < noutput_pidx(); i++)
    if (bv[i] && _output_fidx[i] != last_output_fidx) {
      last_output_fidx = _output_fidx[i];
      results.push_back(_elements[last_output_fidx]);
    }
  return 0;
}

int
Router::upstream_elements(Element *first_element, int first_input,
			  Vector<Element *> &results)
{
  return upstream_elements(first_element, first_input, 0, results);
}

int
Router::upstream_elements(Element *first_element, Vector<Element *> &results)
{
  return upstream_elements(first_element, -1, 0, results);
}


// INITIALIZATION

String
Router::context_message(int element_no, const char *message) const
{
  return _elements[element_no]->context_message(message);
}

int
Router::initialize(ErrorHandler *errh)
{
  assert(!_initialized);
  if (!_closed) {
    errh->error("compound element could not be initialized");
    return -1;
  }
  
  int before = errh->nerrors();
  Bitvector element_ok(nelements(), true);
  bool all_ok = true;
  
  for (int i = 0; i < _elements.size(); i++) {
    _elements[i]->set_number(i);
    _elements[i]->initialize_link(this);
  }
  
  notify_hookup_range();

  // Configure all elements. Remember the ones that failed
  for (int i = 0; i < _elements.size(); i++) {
    ContextErrorHandler cerrh
      (errh, context_message(i, "While configuring"));
    int before = cerrh.nerrors();
    if (_elements[i]->configure(_configurations[i], this, &cerrh) < 0) {
      element_ok[i] = all_ok = false;
      if (cerrh.nerrors() == before)
	cerrh.error("unspecified error");
    }
  }
  
  check_hookup_range(errh);
  make_pidxes();
  check_push_and_pull(errh);
  check_hookup_completeness(errh);
  set_connections();
  _have_connections = true;

  // Initialize elements that are OK so far.
  for (int i = 0; i < _elements.size(); i++)
    if (element_ok[i]) {
      ContextErrorHandler cerrh
	(errh, context_message(i, "While initializing"));
      int before = cerrh.nerrors();
      if (_elements[i]->initialize(this, &cerrh) < 0) {
	element_ok[i] = all_ok = false;
	// don't report `unspecified error' for ErrorElements: keep error
	// messages clean
	if (cerrh.nerrors() == before && !_elements[i]->is_a("Error"))
	  cerrh.error("unspecified error");
      }
    }
  
  // If there were errors, uninitialize any elements that we initialized
  // successfully and return -1 (error). Otherwise, we're all set!
  if (errh->nerrors() != before || !all_ok) {
    errh->error("router could not be initialized");
    for (int i = 0; i < _elements.size(); i++)
      if (element_ok[i])
	_elements[i]->uninitialize(this);
    return -1;
  } else {
    _initialized = true;
    return 0;
  }
}


// LIVE RECONFIGURATION

int
Router::live_reconfigure(int elementno, const String &conf, ErrorHandler *errh)
{
  assert(_initialized);
  if (elementno < 0 || elementno >= nelements())
    return errh->error("no element number %d", elementno);
  Element *f = _elements[elementno];
  if (!f->can_live_reconfigure())
    return errh->error("cannot reconfigure `%s' live", f->declaration().cc());
  _configurations[elementno] = conf;
  return f->live_reconfigure(conf, this, errh);
}


// DRIVER

void
Router::wait()
{
#ifndef __KERNEL__
  int i, n;
  bool any = false;
  fd_set mask;
  struct timeval tv;

  FD_ZERO(&mask);
  for (i = 0; i < _elements.size(); i++) {
    Element *f = _elements[i];
    int fd = f->select_fd();
    if (fd >= 0) {
      FD_SET(fd, &mask);
      any = true;
    }
  }

  if (scheduled_next() != this)
    return;
  if (!any && !Timer::get_next_delay(&tv))
    return;
  
  n = select(FD_SETSIZE, &mask, (fd_set*)0, (fd_set*)0, &tv);
  if(n < 0){
    perror("select");
    sleep(1);
  }

  for (i = 0; i < _elements.size(); i++) {
    Element *f = _elements[i];
    int fd = f->select_fd();
    if(fd >= 0 && FD_ISSET(fd, &mask))
      f->selected(fd);
  }
#endif
}

// WORK LIST

void
Router::run_scheduled()
{
  ElementLink final(this);
  final.schedule_tail();
  for (ElementLink *fl = scheduled_next();
       fl != &final;
       fl = scheduled_next()) {
    fl->unschedule();
    ((Element *)fl)->run_scheduled();
  }
  final.unschedule();
#if 0 && defined(__KERNEL__)
  // undefined because it seems to be causing a crash -- reentrancy?
  if (fl != this && !_please_stop_driver)
    queue_task(&_schedule_task, &tq_scheduler);
#endif
}

bool
Router::driver()
{
#ifndef __KERNEL__
  Timer::run_timers();
#endif
  
  // Alert outputs that they should pull because some upstream
  // Queue became non-empty.
  run_scheduled();

#ifndef __KERNEL__
  // Wait in select() for input or timer.
  // And call relevant elements' selected() methods.
  wait();
#endif

  return !_please_stop_driver;
}

// PRINTING

String
Router::connection_number_string(const Connection &c) const
{
  if (!c.allowed())
    return "X";
  if (!c)
    return ".";
  Element *f = c.element();
  String portid = "[" + String(c.port()) + "]";
  for (int i = 0; i < _elements.size(); i++)
    if (_elements[i] == f)
      return String("#") + String(i) + portid;
  return "??" + portid;
}

void
Router::print_structure(ErrorHandler *errh)
{
  ContextErrorHandler cerrh(errh);
  for (int i = 0; i < _elements.size(); i++) {
    Element *f = _elements[i];
    
    String message = String("#") + String(i) + String(": ");
    message += f->declaration() + " - ";
    
    String inputs = "[";
    for (int i = 0; i < f->ninputs(); i++) {
      if (i) inputs += ", ";
      inputs += connection_number_string(f->input(i));
    }
    inputs += "]";
    
    String outputs = "[";
    for (int i = 0; i < f->noutputs(); i++) {
      if (i) outputs += ", ";
      outputs += connection_number_string(f->output(i));
    }
    outputs += "]";
    
    message += inputs + " -> " + outputs;
    
    errh->message(message);
  }
}

String
Router::flat_configuration_string() const
{
  StringAccum sa;
  
  for (int i = 0; i < nelements(); i++) {
    sa << _elements[i]->id() << " :: " << _elements[i]->class_name();
    if (_configurations[i])
      sa << "(" << _configurations[i] << ")";
    sa << ";\n";
  }
  
  if (nelements() > 0)
    sa << "\n";
  
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
      sa << _elements[hf.idx]->id();
      if (hf.port)
	sa << " [" << hf.port << "]";
      
      int d = c;
      while (d >= 0 && !used[d]) {
	if (d == c) sa << " -> ";
	else sa << "\n    -> ";
	const Hookup &ht = _hookup_to[d];
	if (ht.port)
	  sa << "[" << ht.port << "] ";
	sa << _elements[ht.idx]->id();
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
  
  return sa.take_string();
}

String
Router::element_list_string() const
{
  StringAccum sa;
  sa << nelements() << "\n";
  for (int i = 0; i < nelements(); i++)
    sa << _elements[i]->id() << "\n";
  return sa.take_string();
}

String
Router::element_inputs_string(int fi) const
{
  if (fi < 0 || fi >= nelements()) return String();
  StringAccum sa;
  Element *f = _elements[fi];
  Vector<int> in_pers(f->ninputs(), 0);
  Vector<int> out_pers(f->noutputs(), 0);
  f->processing_vector(in_pers, 0, out_pers, 0);
  sa << f->ninputs() << "\n";
  for (int i = 0; i < f->ninputs(); i++) {
    // processing
    const char *persid = (f->input_is_pull(i) ? "pull" : "push");
    if (in_pers[i] == Element::AGNOSTIC) sa << "(" << persid << ")\t";
    else sa << persid << "\t";
    
    // counts
#if CLICK_STATS >= 1
    if (f->input_is_pull(i) || CLICK_STATS >= 2)
      sa << f->input(i).packet_count() << "\t";
    else
#endif
      sa << "-\t";
    
    // connections
    Hookup h(fi, i);
    const char *sep = "";
    for (int c = 0; c < _hookup_from.size(); c++)
      if (_hookup_to[c] == h) {
	sa << sep << _elements[_hookup_from[c].idx]->id();
	sa << "[" << _hookup_from[c].port << "]";
	sep = " ";
      }
    sa << "\n";
  }
  return sa.take_string();
}

String
Router::element_outputs_string(int fi) const
{
  if (fi < 0 || fi >= nelements()) return String();
  StringAccum sa;
  Element *f = _elements[fi];
  Vector<int> in_pers(f->ninputs(), 0);
  Vector<int> out_pers(f->noutputs(), 0);
  f->processing_vector(in_pers, 0, out_pers, 0);
  sa << f->noutputs() << "\n";
  for (int i = 0; i < f->noutputs(); i++) {
    // processing
    const char *persid = (f->output_is_push(i) ? "push" : "pull");
    if (out_pers[i] == Element::AGNOSTIC) sa << "(" << persid << ")\t";
    else sa << persid << "\t";
    
    // counts
#if CLICK_STATS >= 1
    if (f->output_is_push(i) || CLICK_STATS >= 2)
      sa << f->output(i).packet_count() << "\t";
    else
#endif
      sa << "-\t";
    
    // hookup
    Hookup h(fi, i);
    const char *sep = "";
    for (int c = 0; c < _hookup_from.size(); c++)
      if (_hookup_from[c] == h) {
	sa << sep << "[" << _hookup_from[c].port << "]";
	sa << _elements[_hookup_to[c].idx]->id();
	sep = " ";
      }
    sa << "\n";
  }
  return sa.take_string();
}
