/*
 * element.{cc,hh} -- the Element base class
 * Eddie Kohler
 * statistics: Robert Morris
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "element.hh"
#include "bitvector.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"
#include "straccum.hh"
#include <errno.h>

int Element::nelements_allocated;
#if CLICK_STATS >= 2
# define ELEMENT_CTOR_STATS _calls(0), _self_cycles(0), _child_cycles(0),
#else
# define ELEMENT_CTOR_STATS
#endif

Element::Element()
  : ELEMENT_CTOR_STATS _refcount(0), _ninputs(0), _inputs(&_input0[0]),
    _noutputs(0), _outputs(&_output0[0])
{
  nelements_allocated++;
}

Element::Element(int ninputs, int noutputs)
  : ELEMENT_CTOR_STATS _refcount(0), _ninputs(0), _inputs(&_input0[0]),
    _noutputs(0), _outputs(&_output0[0])
{
  set_ninputs(ninputs);
  set_noutputs(noutputs);
  nelements_allocated++;
}

Element::~Element()
{
  nelements_allocated--;
  if (_ninputs > InlinePorts) delete[] _inputs;
  if (_noutputs > InlinePorts) delete[] _outputs;
}

void
Element::static_initialize()
{
  nelements_allocated = 0;
}

// CHARACTERISTICS

bool
Element::is_a(const char *name) const
{
  const char *my_name = class_name();
  if (my_name && name)
    return strcmp(my_name, name) == 0;
  else
    return false;
}

void
Element::set_id(const String &id)
{
  _id = id;
}

String
Element::declaration() const
{
  return (_id ? _id + " :: " : String("")) + class_name();
}

// INPUTS AND OUTPUTS

void
Element::set_nports(int &store_n, Connection *&store_vec,
		    Connection *store_inline, int new_n)
{
  if (new_n < 0) return;
  int old_n = store_n;
  store_n = new_n;
  
  if (new_n < old_n) {
    if (old_n > InlinePorts && store_n <= InlinePorts) {
      memcpy(store_inline, store_vec, store_n * sizeof(Connection));
      delete[] store_vec;
      store_vec = store_inline;
    }
    return;
  }
  
  Connection *new_vec;
  if (store_n <= InlinePorts)
    new_vec = store_inline;
  else
    new_vec = new Connection[store_n];
  if (!new_vec) {
    // out of memory -- restore old value of old_n
    store_n = old_n;
    return;
  }
  
  if (new_vec != store_vec && old_n)
    memcpy(new_vec, store_vec, old_n * sizeof(Connection));
  if (old_n > InlinePorts)
    delete[] store_vec;
  store_vec = new_vec;
  
  for (int i = old_n; i < store_n; i++)
    store_vec[i] = Connection(this);
}

void
Element::set_ninputs(int count)
{
  set_nports(_ninputs, _inputs, &_input0[0], count);
}

void
Element::notify_ninputs(int)
{
}

int
Element::connect_input(int i, Element *f, int port)
{
  if (i >= 0 && i < ninputs() && _inputs[i].allowed()) {
    _inputs[i] = Connection(this, f, port);
    return 0;
  } else
    return -1;
}

void
Element::set_noutputs(int count)
{
  set_nports(_noutputs, _outputs, &_output0[0], count);
}

void
Element::notify_noutputs(int)
{
}

int
Element::connect_output(int o, Element *f, int port)
{
  if (o >= 0 && o < noutputs() && _outputs[o].allowed()) {
    _outputs[o] = Connection(this, f, port);
    return 0;
  } else
    return -1;
}

// FLOW

Bitvector
Element::forward_flow(int i) const
{
  return Bitvector(noutputs(), i >= 0 && i < ninputs());
}

Bitvector
Element::backward_flow(int o) const
{
  return Bitvector(ninputs(), o >= 0 && o < noutputs());
}

// PUSH OR PULL PROCESSING

void
Element::processing_vector(Vector<int> &in_v, int in_offset,
			   Vector<int> &out_v, int out_offset) const
{
  Processing p = default_processing();
  if (p == PUSH_TO_PULL) p = PUSH;
  else if (p == PULL_TO_PUSH) p = PULL;
  for (int i = 0; i < ninputs(); i++)
    in_v[in_offset+i] = p;
  
  p = default_processing();
  if (p == PUSH_TO_PULL) p = PULL;
  else if (p == PULL_TO_PUSH) p = PUSH;
  for (int o = 0; o < noutputs(); o++)
    out_v[out_offset+o] = p;
}

Element::Processing
Element::default_processing() const
{
  return AGNOSTIC;
}

void
Element::set_processing_vector(const Vector<int> &in_v, int in_offset,
			       const Vector<int> &out_v, int out_offset)
{
  for (int i = 0; i < ninputs(); i++)
    if (in_v[in_offset+i] == PULL)
      _inputs[i].clear();
    else
      _inputs[i].disallow();
  for (int o = 0; o < noutputs(); o++)
    if (out_v[out_offset+o] == PUSH)
      _outputs[o].clear();
    else
      _outputs[o].disallow();
}

const char *
Element::processing_name(Processing p)
{
  switch (p) {
   case AGNOSTIC: return "agnostic";
   case PUSH: return "push";
   case PULL: return "pull";
   case PULL_TO_PUSH: return "pull-to-push";
   case PUSH_TO_PULL: return "push-to-pull";
   default: return "UNKNOWN-PROCESSING-TYPE";
  }
}

const char *
Element::processing_name(int p)
{
  return processing_name((Processing)p);
}

// CLONING AND CONFIGURING

bool
Element::configure_first() const
{
  return false;
}

int
Element::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh, 0);
}

int
Element::initialize(ErrorHandler *)
{
  return 0;
}

void
Element::uninitialize()
{
}

// LIVE CONFIGURATION

bool
Element::can_live_reconfigure() const
{
  return false;
}

int
Element::live_reconfigure(const String &conf, ErrorHandler *errh)
{
  if (can_live_reconfigure())
    return configure(conf, errh);
  else
    return -1;
}

// HANDLERS

#if CLICK_STATS >= 1

static String
element_read_icounts(Element *f, void *)
{
  StringAccum sa;
  for (int i = 0; i < f->ninputs(); i++)
    if (f->input(i).allowed() || CLICK_STATS >= 2)
      sa << f->input(i).packet_count() << "\n";
    else
      sa << "??\n";
  return sa.take_string();
}

static String
element_read_ocounts(Element *f, void *)
{
  StringAccum sa;
  for (int i = 0; i < f->noutputs(); i++)
    if (f->output(i).allowed() || CLICK_STATS >= 2)
      sa << f->output(i).packet_count() << "\n";
    else
      sa << "??\n";
  return sa.take_string();
}

#endif /* CLICK_STATS >= 1 */

#if CLICK_STATS >= 2
/*
 * cycles:
 * # of calls to this element (push or pull).
 * cycles spent in this element and elements it pulls or pushes.
 * cycles spent in the elements this one pulls and pushes.
 */
static String
element_read_cycles(Element *f, void *)
{
  return(String(f->_calls) + "\n" +
         String(f->_self_cycles) + "\n" +
         String(f->_child_cycles) + "\n");
}
#endif

String
Element::configuration_read_handler(Element *element, void *vno)
{
  Router *router = element->router();
  Vector<String> args;
  cp_argvec(router->configuration(element->number()), args);
  int no = (int)vno;
  if (no >= args.size())
    return String();
  String s = args[no];
  // add trailing "\n" if appropriate
  if (s) {
    int c = s[s.length() - 1];
    if (c != '\n' && c != '\\')
      s += "\n";
  }
  return s;
}

int
Element::reconfigure_write_handler(const String &arg, Element *element,
				   void *vno, ErrorHandler *errh)
{
  Router *router = element->router();
  Vector<String> args;
  cp_argvec(router->configuration(element->number()), args);
  int no = (int)vno;
  while (args.size() <= no)
    args.push_back(String());
  String real_arg = cp_subst(arg);
  args[no] = real_arg;
  if (router->live_reconfigure(element->number(), cp_unargvec(args), errh) < 0)
    return -EINVAL;
  else
    return 0;
}

void
Element::add_handlers(HandlerRegistry *fcr)
{
#if CLICK_STATS >= 1
  fcr->add_read("icounts", element_read_icounts, 0);
  fcr->add_read("ocounts", element_read_ocounts, 0);
# if CLICK_STATS >= 2
  fcr->add_read("cycles", element_read_cycles, 0);
# endif
#else
  (void)fcr;			// avoid warnings
#endif
}

void
Element::HandlerRegistry::add_read(const char *, int, ReadHandler, void *)
{
}

void
Element::HandlerRegistry::add_write(const char *, int, WriteHandler, void *)
{
}

void
Element::HandlerRegistry::add_read_write(const char *, int, ReadHandler,
					 void *, WriteHandler, void *)
{
}

void
Element::HandlerRegistry::add_read(const char *n, ReadHandler f, void *t)
{
  add_read(n, strlen(n), f, t);
}

void
Element::HandlerRegistry::add_write(const char *n, WriteHandler f, void *t)
{
  add_write(n, strlen(n), f, t);
}

void
Element::HandlerRegistry::add_read_write(const char *n, ReadHandler rf,
					 void *rt, WriteHandler wf, void *wt)
{
  add_read_write(n, strlen(n), rf, rt, wf, wt);
}

// RUNNING

void
Element::push(int, Packet *p)
{
  p = simple_action(p);
  if (p) output(0).push(p);
}

Packet *
Element::pull(int)
{
  Packet *p = input(0).pull();
  if (p) p = simple_action(p);
  return p;
}

Packet *
Element::simple_action(Packet *p)
{
  return p;
}

bool
Element::wants_packet_upstream() const
{
  return false;
}

void
Element::run_scheduled()
{
  assert(0 && "bad run_scheduled");
}
