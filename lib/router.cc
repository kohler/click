/*
 * router.{cc,hh} -- a Click router configuration
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "router.hh"
#include "routerthread.hh"
#include "bitvector.hh"
#include "error.hh"
#include "straccum.hh"
#include "elemfilter.hh"
#include "confparse.hh"
#include "subvector.hh"
#include "timer.hh"
#include <stdarg.h>
#ifdef CLICK_USERLEVEL
# include <unistd.h>
# include <errno.h>
#endif

Router::Router()
  : _refcount(0), _initialized(0), _have_connections(0), _have_hookpidx(0),
    _handlers(0), _nhandlers(0), _handlers_cap(0)
{
  initialize_head();
  //printf("sizeof(Anno) %d\n", sizeof(Packet::Anno));
}

Router::~Router()
{
  if (_refcount > 0)
    click_chatter("deleting router while ref count > 0");
  if (_initialized) {
    for (int i = 0; i < _elements.size(); i++)
      _elements[i]->uninitialize();
    for (int i = 0; i < _threads.size(); i++) {
      if (_threads[i])
	_threads[i]->please_stop_driver();
    }
  }
  for (int i = 0; i < _elements.size(); i++)
    delete _elements[i];
  delete[] _handlers;
#ifdef __KERNEL__
  initialize_head();		// get rid of scheduled wait queue
#endif
}

void
Router::please_stop_driver()
{
  for (int i = 0; i < _threads.size(); i++) {
    if (_threads[i])
      _threads[i]->please_stop_driver();
  }
}

void
Router::unuse()
{
  if (--_refcount <= 0)
    delete this;
}


// ACCESS

Element *
Router::find(String prefix, const String &name, ErrorHandler *errh) const
{
  while (1) {
    int got = -1;
    String n = prefix + name;
    for (int i = 0; i < _elements.size(); i++)
      if (_element_names[i] == n) {
	if (got >= 0) {
	  if (errh) errh->error("more than one element named `%s'", n.cc());
	  return 0;
	} else
	  got = i;
      }
    if (got >= 0)
      return _elements[got];
    if (!prefix)
      break;
    
    int slash = prefix.find_right('/', prefix.length() - 2);
    prefix = (slash >= 0 ? prefix.substring(0, slash + 1) : String());
  }
  
  if (errh) errh->error("no element named `%s'", String(name).cc());
  return 0;
}

Element *
Router::find(Element *me, const String &name, ErrorHandler *errh) const
{
  String prefix = _element_names[me->eindex()];
  int slash = prefix.find_right('/');
  return find((slash >= 0 ? prefix.substring(0, slash + 1) : String()),
	      name, errh);
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
Router::ename(int ei) const
{
  if (ei < 0 || ei >= nelements())
    return String::null_string();
  else
    return _element_names[ei];
}

const String &
Router::econfiguration(int ei) const
{
  if (ei < 0 || ei >= nelements())
    return String::null_string();
  else
    return _configurations[ei];
}

const String &
Router::elandmark(int ei) const
{
  if (ei < 0 || ei >= nelements())
    return String::null_string();
  else
    return _element_landmarks[ei];
}


// CREATION 

int
Router::add_element(Element *e, const String &ename, const String &conf,
		    const String &landmark)
{
  // router now owns the element
  if (_initialized || !e) return -1;
  _elements.push_back(e);
  _element_names.push_back(ename);
  _element_landmarks.push_back(landmark);
  _configurations.push_back(conf);
  int i = _elements.size() - 1;
  e->set_eindex(i);
  /* make all elements use Router as its link: subsequent calls to
   * schedule_xxxx places elements on this link, therefore allow
   * driver to see them */
  e->initialize_link(this);
  return i;
}

int
Router::add_connection(int from_idx, int from_port, int to_idx, int to_port)
{
  if (_initialized) return -1;
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

void
Router::add_thread(RouterThread *rt)
{
  _threads.push_back(rt);
}

void
Router::remove_thread(RouterThread *rt)
{
  for (int i=0; i<_threads.size(); i++) {
    if (_threads[i] == rt) _threads[i] = 0;
  }
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
    _elements[f]->notify_ninputs(nin[f] + 1);
    _elements[f]->notify_noutputs(nout[f] + 1);
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
Router::processing_error(const Hookup &hfrom, const Hookup &hto, bool aggie,
			 int processing_from, ErrorHandler *errh)
{
  const char *type1 = (processing_from == Element::VPUSH ? "push" : "pull");
  const char *type2 = (processing_from == Element::VPUSH ? "pull" : "push");
  if (!aggie)
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
  for (int f = 0; f < nelements(); f++) {
    Subvector<int> i(input_pers, _input_pidx[f], _elements[f]->ninputs());
    Subvector<int> o(output_pers, _output_pidx[f], _elements[f]->noutputs());
    _elements[f]->processing_vector(i, o, errh);
  }
  
  // add fake connections for agnostics
  Vector<Hookup> hookup_from = _hookup_from;
  Vector<Hookup> hookup_to = _hookup_to;
  for (int i = 0; i < ninput_pidx(); i++)
    if (input_pers[i] == Element::VAGNOSTIC) {
      int fid = _input_fidx[i];
      int port = i - _input_pidx[fid];
      Bitvector bv = _elements[fid]->forward_flow(port);
      int opidx = _output_pidx[fid];
      for (int j = 0; j < bv.size(); j++)
	if (bv[j] && output_pers[opidx+j] == Element::VAGNOSTIC) {
	  hookup_from.push_back(Hookup(fid, j));
	  hookup_to.push_back(Hookup(fid, port));
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
      
      int offf = output_pidx(hookup_from[c]);
      int offt = input_pidx(hookup_to[c]);
      int pf = output_pers[offf];
      int pt = input_pers[offt];
      
      switch (pt) {
	
       case Element::VAGNOSTIC:
	if (pf != Element::VAGNOSTIC) {
	  input_pers[offt] = pf;
	  changed = true;
	}
	break;
	
       case Element::VPUSH:
       case Element::VPULL:
	if (pf == Element::VAGNOSTIC) {
	  output_pers[offf] = pt;
	  changed = true;
	} else if (pf != pt) {
	  processing_error(hookup_from[c], hookup_to[c], c >= first_agnostic,
			   pf, errh);
	  hookup_from[c].idx = -1;
	}
	break;
	
      }
    }
    
    if (!changed) break;
  }
  
  if (errh->nerrors() != before)
    return -1;
  
  for (int f = 0; f < nelements(); f++) {
    Subvector<int> i(input_pers, _input_pidx[f], _elements[f]->ninputs());
    Subvector<int> o(output_pers, _output_pidx[f], _elements[f]->noutputs());
    _elements[f]->set_processing_vector(i, o);
  }
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
    fromf->connect_output(hfrom.port, tof, hto.port);
    tof->connect_input(hto.port, fromf, hfrom.port);
  }
}

// TIMERS

void
Router::run_timers()
{
  if (_timer_lock.attempt()) {
    _timer_head.run();
    _timer_lock.release();
  }
}

#ifndef __KERNEL__
int
Router::wait_in_select()
{
  struct timeval wait, *wait_ptr = &wait;

  if (scheduled_next() != this)
    return 0;

  if (_wait_lock.attempt()) 
  {
    // wait in select() for input or timer.
    // and call relevant elements' selected() methods.
    fd_set read_mask = _read_select_fd_set;
    fd_set write_mask = _write_select_fd_set;
    bool selects = (_selectors.size() > 0);

    bool timers = _timer_head.get_next_delay(&wait);
    if (!selects && !timers) {
      _wait_lock.release();
      return 0;
    }
    if (!timers)
      wait_ptr = 0;
  
    int n = select
      (_max_select_fd + 1, &read_mask, &write_mask, (fd_set *)0, wait_ptr);
    _wait_lock.release();
    
    if (n < 0 && errno != EINTR) {
      perror("select");
      return 0;
    }
    else if (n > 0) {
      for (int i = 0; i < _selectors.size(); i++) {
        const Selector &s = _selectors[i];
        if (((s.mask & SELECT_READ) && FD_ISSET(s.fd, &read_mask))
	    || ((s.mask & SELECT_WRITE) && FD_ISSET(s.fd, &write_mask)))
	  _elements[s.element]->selected(s.fd);
      }
      return 1;
    }
  }
  else {
    /* no work to be done, and someone else is waiting in select for a
     * descriptor to be ready, so we just give up the processor, and try again
     * later */
    wait.tv_sec = 0;
    wait.tv_usec = 1000;
    /* hopefully this will cause a reschedule */
    int n = select(0, (fd_set*)0, (fd_set*)0, (fd_set*)0, wait_ptr);
    if (n < 0 && errno != EINTR)
      perror("select");
    return 0;
  }
  assert(0);
  return 0;
}
#endif

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
  int first_fid = first_element->eindex(this);
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
  int first_fid = first_element->eindex(this);
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
  Element *e = _elements[element_no];
  String s;
  if (e->landmark())
    s = e->landmark() + ": ";
  s += String(message) + " `" + e->declaration() + "':";
  return s;
}

static void
partition_configure_order(Vector<int> &order, const Vector<int> &phase,
			  int left, int right,
			  int &split_left, int &split_right)
{
  // Dutch national flag algorithm
  int middle = left;
  int pivot = phase[order[(left + right) / 2]];

  // loop invariant:
  // phase[order[i]] < pivot for all left_init <= i < left
  // phase[order[i]] > pivot for all right < i <= right_init
  // phase[order[i]] == pivot for all left <= i < middle
  while (middle <= right) {
    int p = phase[order[middle]];
    if (p < pivot) {
      int t = order[left];
      order[left] = order[middle];
      order[middle] = t;
      left++;
      middle++;
    } else if (p > pivot) {
      int t = order[right];
      order[right] = order[middle];
      order[middle] = t;
      right--;
    } else
      middle++;
  }

  // afterwards, middle == right + 1
  // so phase[order[i]] == pivot for all left <= i <= right
  split_left = left - 1;
  split_right = right + 1;
}

static void
qsort_configure_order(Vector<int> &order, const Vector<int> &phase,
		      int left, int right)
{
  if (left < right) {
    int split_left, split_right;
    partition_configure_order(order, phase, left, right, split_left, split_right);
    qsort_configure_order(order, phase, left, split_left);
    qsort_configure_order(order, phase, split_right, right);
  }
}

int
Router::initialize(ErrorHandler *errh)
{
  assert(!_initialized);
#if CLICK_USERLEVEL
  FD_ZERO(&_read_select_fd_set);
  FD_ZERO(&_write_select_fd_set);
  _max_select_fd = -1;
  assert(!_selectors.size());
#endif

#if CLICK_DMALLOC
  char dmalloc_buf[12];
#endif
  
  if (check_hookup_elements(errh) < 0)
    return -1;
  
  Bitvector element_ok(nelements(), true);
  bool all_ok = true;

  notify_hookup_range();

  // set up configuration order
  Vector<int> configure_phase(nelements(), 0);
  Vector<int> configure_order(nelements(), 0);
  for (int i = 0; i < _elements.size(); i++) {
    configure_phase[i] = _elements[i]->configure_phase();
    configure_order[i] = i;
  }
  qsort_configure_order(configure_order, configure_phase, 0, _elements.size() - 1);

  // Configure all elements in configure order. Remember the ones that failed
  for (int ord = 0; ord < _elements.size(); ord++) {
    int i = configure_order[ord];
#if CLICK_DMALLOC
    sprintf(dmalloc_buf, "c%d  ", i);
    CLICK_DMALLOC_REG(dmalloc_buf);
#endif
    ContextErrorHandler cerrh
      (errh, context_message(i, "While configuring"));
    int before = cerrh.nerrors();
    Vector<String> conf;
    cp_argvec(_configurations[i], conf);
    if (_elements[i]->configure(conf, &cerrh) < 0) {
      element_ok[i] = all_ok = false;
      if (cerrh.nerrors() == before)
	cerrh.error("unspecified error");
    }
  }

#if CLICK_DMALLOC
  CLICK_DMALLOC_REG("iHoo");
#endif
  
  int before = errh->nerrors();
  check_hookup_range(errh);
  make_pidxes();
  check_push_and_pull(errh);
  check_hookup_completeness(errh);
  set_connections();
  _have_connections = true;
  if (before != errh->nerrors())
    all_ok = false;

  // Initialize elements that are OK so far.
  for (int ord = 0; ord < _elements.size(); ord++) {
    int i = configure_order[ord];
    if (element_ok[i]) {
#if CLICK_DMALLOC
      sprintf(dmalloc_buf, "i%d  ", i);
      CLICK_DMALLOC_REG(dmalloc_buf);
#endif
      ContextErrorHandler cerrh
	(errh, context_message(i, "While initializing"));
      int before = cerrh.nerrors();
      if (_elements[i]->initialize(&cerrh) < 0) {
	element_ok[i] = all_ok = false;
	// don't report `unspecified error' for ErrorElements: keep error
	// messages clean
	if (cerrh.nerrors() == before && !_elements[i]->cast("Error"))
	  cerrh.error("unspecified error");
      }
    }
  }

#if CLICK_DMALLOC
  CLICK_DMALLOC_REG("iXXX");
#endif
  
  // clear handler offsets
  _ehandler_first_by_element.assign(nelements(), -1);
  assert(_ehandler_to_handler.size() == 0 && _ehandler_next.size() == 0
	 && _handler_first_by_name.size() == 0
	 && _handler_next_by_name.size() == 0
	 && _handler_use_count.size() == 0
	 && _nhandlers == 0);

  // If there were errors, uninitialize any elements that we initialized
  // successfully and return -1 (error). Otherwise, we're all set!
  if (!all_ok) {
    errh->error("Router could not be initialized!");
    for (int i = 0; i < _elements.size(); i++)
      if (element_ok[i])
	_elements[i]->uninitialize();
    return -1;
  } else {
    _initialized = true;
    return 0;
  }
}


// steal state

void
Router::take_state(Router *r, ErrorHandler *errh)
{
  assert(_initialized);
  for (int i = 0; i < _elements.size(); i++) {
    Element *e = _elements[i];
    Element *other = r->find(_element_names[e->eindex()]);
    if (other) {
      ContextErrorHandler cerrh
	(errh, context_message(i, "While hot-swapping state into"));
      e->take_state(other, &cerrh);
    }
  }
}


// HANDLERS

// 11.Jul.2000 - We had problems with handlers for medium-sized configurations
// (~400 elements): the Linux kernel would crash with a "kmalloc too large".
// The solution: Observe that most handlers are shared. For example, all
// `name' handlers can share a Handler structure, since they share the same
// read function, read thunk, write function (0), write thunk, and name. This
// introduced a bunch of structure to go from elements to handler indices and
// from handler names to handler indices, but it was worth it: it reduced the
// amount of space required by a normal set of Router::Handlers by about a
// factor of 100 -- there used to be 2998 Router::Handlers, now there are 30.
// (Some of this space is still not available for system use -- it gets used
// up by the indexing structures, particularly _ehandlers. Every element has
// its own list of "element handlers", even though most elements with element
// class C could share one such list. The space cost is about (48 bytes * # of
// elements) more or less. Detecting this sharing would be harder to
// implement.)

int
Router::put_handler(const Handler &to_add)
{
  // Find space in _handlers for `to_add'. This might be shared, if another
  // element has already installed a handler corresponding to `to_add'.
  
  assert(_handler_use_count.size() == _nhandlers
	 && _handler_next_by_name.size() == _nhandlers);
  
  // find the offset in _name_handlers
  int name_offset;
  for (name_offset = 0;
       name_offset < _handler_first_by_name.size();
       name_offset++) {
    int hi = _handler_first_by_name[name_offset];
    if (hi < 0 || _handlers[hi].name == to_add.name)
      break;
  }
  if (name_offset == _handler_first_by_name.size())
    _handler_first_by_name.push_back(-1);

  // now find a similar handler, if any exists
  int hi = _handler_first_by_name[name_offset];
  while (hi >= 0) {
    Handler &h = _handlers[hi];
    if (_handler_use_count[hi] == 0)
      h = to_add;
    if (h.read == to_add.read && h.write == to_add.write
	&& h.read_thunk == to_add.read_thunk
	&& h.write_thunk == to_add.write_thunk) {
      _handler_use_count[hi]++;
      return hi;
    }
    hi = _handler_next_by_name[hi];
  }
  
  // no handler found; add one
  if (_nhandlers >= _handlers_cap) {
    int new_cap = (_handlers_cap ? 2*_handlers_cap : 16);
    Handler *new_handlers = new Handler[new_cap];
    if (!new_handlers)	// out of memory
      return -1;
    for (int i = 0; i < _nhandlers; i++)
      new_handlers[i] = _handlers[i];
    delete[] _handlers;
    _handlers = new_handlers;
    _handlers_cap = new_cap;
  }

  hi = _nhandlers;
  _nhandlers++;
  _handlers[hi] = to_add;
  _handler_use_count.push_back(1);
  _handler_next_by_name.push_back(_handler_first_by_name[name_offset]);
  _handler_first_by_name[name_offset] = hi;
  return hi;
}

int
Router::find_ehandler(int eindex, const String &name, bool force)
{
  int eh = _ehandler_first_by_element[eindex];
  while (eh >= 0) {
    int h = _ehandler_to_handler[eh];
    if (h >= 0 && _handlers[h].name == name)
      return eh;
    eh = _ehandler_next[eh];
  }

  if (force) {
    eh = _ehandler_to_handler.size();
    _ehandler_to_handler.push_back(-1);
    _ehandler_next.push_back(_ehandler_first_by_element[eindex]);
    _ehandler_first_by_element[eindex] = eh;
  }

  return eh;
}

void
Router::add_read_handler(int eindex, const String &name,
			 ReadHandler read, void *thunk)
{
  int eh = find_ehandler(eindex, name, true);
  Handler to_add;
  int h = _ehandler_to_handler[eh];
  if (h >= 0) {
    to_add = _handlers[h];
    _handler_use_count[h]--;
  } else {
    to_add.name = name;
    to_add.write = 0;
    to_add.write_thunk = 0;
  }
  to_add.read = read;
  to_add.read_thunk = thunk;
  _ehandler_to_handler[eh] = put_handler(to_add);
}

void
Router::add_write_handler(int eindex, const String &name,
			  WriteHandler write, void *thunk)
{
  int eh = find_ehandler(eindex, name, true);
  Handler to_add;
  int h = _ehandler_to_handler[eh];
  if (h >= 0) {
    to_add = _handlers[h];
    _handler_use_count[h]--;
  } else {
    to_add.name = name;
    to_add.read = 0;
    to_add.read_thunk = 0;
  }
  to_add.write = write;
  to_add.write_thunk = thunk;
  _ehandler_to_handler[eh] = put_handler(to_add);
}

int
Router::find_handler(Element *element, const String &name)
{
  int eh = find_ehandler(element->eindex(), name, false);
  return (eh >= 0 ? _ehandler_to_handler[eh] : -1);
}

void
Router::element_handlers(int eindex, Vector<int> &handlers) const
{
  assert(eindex >= 0 && eindex < _elements.size());
  for (int eh = _ehandler_first_by_element[eindex];
       eh >= 0;
       eh = _ehandler_next[eh]) {
    int h = _ehandler_to_handler[eh];
    if (h >= 0)
      handlers.push_back(h);
  }
}


// LIVE RECONFIGURATION

int
Router::live_reconfigure(int eindex, const Vector<String> &conf,
			 ErrorHandler *errh)
{
  assert(_initialized);
  if (eindex < 0 || eindex >= nelements())
    return errh->error("no element number %d", eindex);
  Element *f = _elements[eindex];
  if (!f->can_live_reconfigure())
    return errh->error("cannot reconfigure `%s' live", f->declaration().cc());
  int result = f->live_reconfigure(conf, errh);
  if (result >= 0)
    _configurations[eindex] = cp_unargvec(conf);
  return result;
}

int
Router::live_reconfigure(int eindex, const String &confstr,
			 ErrorHandler *errh)
{
  Vector<String> conf;
  cp_argvec(confstr, conf);
  return live_reconfigure(eindex, conf, errh);
}

void
Router::set_configuration(int eindex, const String &conf)
{
  assert(_initialized);
  if (eindex >= 0 && eindex < nelements())
    _configurations[eindex] = conf;
}


// SELECT

#if CLICK_USERLEVEL

int
Router::add_select(int fd, int element, int mask)
{
  if (fd < 0) return -1;
  assert(fd >= 0 && element >= 0 && element < nelements());

  for (int i = 0; i < _selectors.size(); i++)
    if (_selectors[i].fd == fd && _selectors[i].element != element
	&& (_selectors[i].mask & mask))
      return -1;
  
  if (mask & SELECT_READ)
    FD_SET(fd, &_read_select_fd_set);
  if (mask & SELECT_WRITE)
    FD_SET(fd, &_write_select_fd_set);
  if ((mask & (SELECT_READ | SELECT_WRITE)) && fd > _max_select_fd)
    _max_select_fd = fd;
  
  for (int i = 0; i < _selectors.size(); i++)
    if (_selectors[i].fd == fd && _selectors[i].element == element) {
      _selectors[i].mask |= mask;
      return 0;
    }
  _selectors.push_back(Selector(fd, element, mask));
  return 0;
}

int
Router::remove_select(int fd, int element, int mask)
{
  if (fd < 0) return -1;
  assert(fd >= 0 && element >= 0 && element < nelements());
  
  for (int i = 0; i < _selectors.size(); i++) {
    Selector &s = _selectors[i];
    if (s.fd == fd && s.element == element) {
      mask &= s.mask;
      if (!mask) return -1;
      if (mask & SELECT_READ)
	FD_CLR(fd, &_read_select_fd_set);
      if (mask & SELECT_WRITE)
	FD_CLR(fd, &_write_select_fd_set);
      s.mask &= ~mask;
      if (!s.mask) {
	s = _selectors.back();
	_selectors.pop_back();
	if (_selectors.size() == 0)
	  _max_select_fd = -1;
      }
      return 0;
    }
  }
  
  return -1;
}

#endif


// PRINTING

String
Router::flat_configuration_string() const
{
  StringAccum sa;

  // requirements
  if (_requirements.size())
    sa << "require(" << cp_unargvec(_requirements) << ");\n\n";
  
  // element classes
  for (int i = 0; i < nelements(); i++) {
    sa << _element_names[i] << " :: " << _elements[i]->class_name();
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
      sa << _element_names[hf.idx];
      if (hf.port)
	sa << " [" << hf.port << "]";
      
      int d = c;
      while (d >= 0 && !used[d]) {
	if (d == c) sa << " -> ";
	else sa << "\n    -> ";
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
  
  return sa.take_string();
}

String
Router::element_list_string() const
{
  StringAccum sa;
  sa << nelements() << "\n";
  for (int i = 0; i < nelements(); i++)
    sa << _element_names[i] << "\n";
  return sa.take_string();
}

String
Router::element_ports_string(int fi) const
{
  if (fi < 0 || fi >= nelements()) return String();
  StringAccum sa;
  Element *f = _elements[fi];
  Vector<int> pers(f->ninputs() + f->noutputs(), 0);
  Subvector<int> in_pers(pers, 0, f->ninputs());
  Subvector<int> out_pers(pers, f->ninputs(), f->noutputs());
  f->processing_vector(in_pers, out_pers, 0);

  sa << f->ninputs() << (f->ninputs() == 1 ? " input\n" : " inputs\n");
  for (int i = 0; i < f->ninputs(); i++) {
    // processing
    const char *persid = (f->input_is_pull(i) ? "pull" : "push");
    if (in_pers[i] == Element::VAGNOSTIC) sa << "(" << persid << ")\t";
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
	sa << sep << _element_names[_hookup_from[c].idx];
	sa << " [" << _hookup_from[c].port << "]";
	sep = " ";
      }
    sa << "\n";
  }

  sa << f->noutputs() << (f->noutputs() == 1 ? " output\n" : " outputs\n");
  for (int i = 0; i < f->noutputs(); i++) {
    // processing
    const char *persid = (f->output_is_push(i) ? "push" : "pull");
    if (out_pers[i] == Element::VAGNOSTIC) sa << "(" << persid << ")\t";
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
	sa << sep << "[" << _hookup_from[c].port << "] ";
	sa << _element_names[_hookup_to[c].idx];
	sep = " ";
      }
    sa << "\n";
  }
  
  return sa.take_string();
}

#if CLICK_USERLEVEL
// Vector template instance
# include "vector.cc"
#endif
