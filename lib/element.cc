/*
 * element.{cc,hh} -- the Element base class
 * Eddie Kohler
 * statistics: Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
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
#include "subvector.hh"
#include <errno.h>

const char *Element::AGNOSTIC = "a";
const char *Element::PUSH = "h";
const char *Element::PULL = "l";
const char *Element::PUSH_TO_PULL = "h/l";
const char *Element::PULL_TO_PUSH = "l/h";

int Element::nelements_allocated;

#if CLICK_STATS >= 2
# define ELEMENT_CTOR_STATS _calls(0), _self_cycles(0), _child_cycles(0),
#else
# define ELEMENT_CTOR_STATS
#endif

Element::Element()
  : ELEMENT_CTOR_STATS _inputs(&_ports0[0]), _outputs(&_ports0[0]),
    _ninputs(0), _noutputs(0)
{
  nelements_allocated++;
}

Element::Element(int ninputs, int noutputs)
  : ELEMENT_CTOR_STATS _inputs(&_ports0[0]), _outputs(&_ports0[0]),
    _ninputs(0), _noutputs(0)
{
  set_nports(ninputs, noutputs);
  nelements_allocated++;
}

Element::~Element()
{
  nelements_allocated--;
  if (_inputs != _ports0)
    delete[] _inputs;
  if (_outputs != _ports0 && _outputs != _ports0 + _ninputs)
    delete[] _outputs;
}

void
Element::static_initialize()
{
  nelements_allocated = 0;
}

// CHARACTERISTICS

void *
Element::cast(const char *name)
{
  const char *my_name = class_name();
  if (my_name && name && strcmp(my_name, name) == 0)
    return this;
  else
    return 0;
}

String
Element::id() const
{
  String s;
  if (Router *r = router())
    s = r->ename(_elementno);
  return (s ? s : String("<unknown>"));
}

String
Element::declaration() const
{
  return id() + " :: " + class_name();
}

String
Element::landmark() const
{
  String s;
  if (Router *r = router())
    s = r->elandmark(_elementno);
  return (s ? s : String("<unknown>"));
}

// INPUTS AND OUTPUTS

void
Element::set_nports(int new_ninputs, int new_noutputs)
{
  // exit on bad counts
  if (new_ninputs < 0 || new_noutputs < 0)
    return;

  // decide if inputs & outputs were inlined
  bool old_in_inline =
    (_inputs == _ports0);
  bool old_out_inline =
    (_outputs == _ports0 || _outputs == _ports0 + _ninputs);

  // decide if inputs & outputs should be inlined
  bool new_in_inline =
    (new_ninputs == 0
     || new_ninputs + new_noutputs <= INLINE_PORTS
     || (new_ninputs <= INLINE_PORTS && new_ninputs > new_noutputs
	 && processing() == PULL));
  bool new_out_inline =
    (new_noutputs == 0
     || new_ninputs + new_noutputs <= INLINE_PORTS
     || (new_noutputs <= INLINE_PORTS && !new_in_inline));

  // save inlined ports
  Connection ports_storage[INLINE_PORTS];
  memcpy(ports_storage, _ports0, sizeof(Connection) * INLINE_PORTS);

  // create new port arrays
  Connection *old_inputs =
    (old_in_inline ? ports_storage : _inputs);
  Connection *new_inputs =
    (new_in_inline ? _ports0 : new Connection[new_ninputs]);
  if (!new_inputs)		// out of memory -- return
    return;

  Connection *old_outputs =
    (old_out_inline ? ports_storage + (_outputs - _ports0) : _outputs);
  Connection *new_outputs =
    (new_out_inline ? (new_in_inline ? _ports0 + new_ninputs : _ports0)
     : new Connection[new_noutputs]);
  if (!new_outputs) {		// out of memory -- return
    if (!new_in_inline)
      delete[] new_inputs;
    return;
  }

  // set up ports
  int smaller_ninputs = (_ninputs < new_ninputs ? _ninputs : new_ninputs);
  int smaller_noutputs = (_noutputs < new_noutputs ? _noutputs : new_noutputs);
  memcpy(new_inputs, old_inputs, sizeof(Connection) * smaller_ninputs);
  memcpy(new_outputs, old_outputs, sizeof(Connection) * smaller_noutputs);
  for (int i = _ninputs; i < new_ninputs; i++)
    new_inputs[i] = Connection(this);
  for (int i = _noutputs; i < new_noutputs; i++)
    new_outputs[i] = Connection(this);

  // install information
  if (!old_in_inline)
    delete[] _inputs;
  if (!old_out_inline)
    delete[] _outputs;
  _inputs = new_inputs;
  _outputs = new_outputs;
  _ninputs = new_ninputs;
  _noutputs = new_noutputs;
}

void
Element::set_ninputs(int count)
{
  set_nports(count, _noutputs);
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
  set_nports(_ninputs, count);
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

static int
next_processing_code(const char *&p, ErrorHandler *errh)
{
  while (isspace(*p)) p++;
  switch (*p) {
    
   case 'h': case 'H':
    p++;
    return Element::VPUSH;
    
   case 'l': case 'L':
    p++;
    return Element::VPULL;
    
   case 'a': case 'A':
    p++;
    return Element::VAGNOSTIC;

   case '/': case '#': case 0:
    return -2;

   default:
    if (errh) errh->error("invalid character `%c' in processing code", *p);
    p++;
    return -1;
    
  }
}

void
Element::processing_vector(Subvector<int> &in_v, Subvector<int> &out_v,
			   ErrorHandler *errh) const
{
  const char *p_in = processing();
  int val = 0;

  const char *p = p_in;
  int last_val = 0;
  for (int i = 0; i < ninputs(); i++) {
    if (last_val >= 0) last_val = next_processing_code(p, errh);
    if (last_val >= 0) val = last_val;
    in_v[i] = val;
  }

  while (*p && *p != '/')
    p++;
  if (!*p)
    p = p_in;
  else
    p++;

  last_val = 0;
  for (int i = 0; i < noutputs(); i++) {
    if (last_val >= 0) last_val = next_processing_code(p, errh);
    if (last_val >= 0) val = last_val;
    out_v[i] = val;
  }
}

const char *
Element::processing() const
{
  return AGNOSTIC;
}

void
Element::set_processing_vector(const Subvector<int> &in_v,
			       const Subvector<int> &out_v)
{
  for (int i = 0; i < ninputs(); i++)
    if (in_v[i] == VPULL)
      _inputs[i].clear();
    else
      _inputs[i].disallow();
  for (int o = 0; o < noutputs(); o++)
    if (out_v[o] == VPUSH)
      _outputs[o].clear();
    else
      _outputs[o].disallow();
}

// CLONING AND CONFIGURING

int
Element::configure_phase() const
{
  return CONFIGURE_PHASE_DEFAULT;
}

int
Element::configure(const Vector<String> &conf, ErrorHandler *errh)
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
Element::live_reconfigure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (can_live_reconfigure())
    return configure(conf, errh);
  else
    return -1;
}

void
Element::take_state(Element *, ErrorHandler *)
{
}


// SELECT

#if CLICK_USERLEVEL

int
Element::add_select(int fd, int mask) const
{
  return router()->add_select(fd, elementno(), mask);
}

int
Element::remove_select(int fd, int mask) const
{
  return router()->remove_select(fd, elementno(), mask);
}

void
Element::selected(int)
{
}

#endif


// HANDLERS

static String
read_element_class(Element *e, void *)
{
  return String(e->class_name()) + "\n";
}

static String
read_element_name(Element *e, void *)
{
  return e->id() + "\n";
}

static String
read_element_config(Element *e, void *)
{
  String s = e->router()->econfiguration(e->elementno());
  if (s) {
    int c = s[s.length() - 1];
    if (c != '\n' && c != '\\')
      s += "\n";
  }
  return s;
}

static int
write_element_config(const String &conf, Element *e, void *,
		     ErrorHandler *errh)
{
  if (e->can_live_reconfigure())
    return e->router()->live_reconfigure(e->elementno(), conf, errh);
  else
    return -EPERM;
}

static String
read_element_inputs(Element *e, void *)
{
  return e->router()->element_inputs_string(e->elementno());
}

static String
read_element_outputs(Element *e, void *)
{
  return e->router()->element_outputs_string(e->elementno());
}

static String
read_element_tickets(Element *e, void *)
{
#if RR_SCHED
  return (e->scheduled() ? "scheduled\n" : "unscheduled\n");
#else
  String s = String(e->tickets()) + " " + String(e->max_tickets());
  s += (e->scheduled() ? " scheduled\n" : " unscheduled\n");
  return s;
#endif
}

#if CLICK_STATS >= 1

static String
read_element_icounts(Element *f, void *)
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
read_element_ocounts(Element *f, void *)
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
read_element_cycles(Element *f, void *)
{
  return(String(f->_calls) + "\n" +
         String(f->_self_cycles) + "\n" +
         String(f->_child_cycles) + "\n");
}
#endif

void
Element::add_read_handler(const String &name, ReadHandler h, void *thunk)
{
  router()->add_read_handler(this, name, h, thunk);
}

void
Element::add_write_handler(const String &name, WriteHandler h, void *thunk)
{
  router()->add_write_handler(this, name, h, thunk);
}

void
Element::add_default_handlers(bool allow_write_config)
{
  add_read_handler("class", read_element_class, 0);
  add_read_handler("name", read_element_name, 0);
  add_read_handler("config", read_element_config, 0);
  if (allow_write_config && can_live_reconfigure())
    add_write_handler("config", write_element_config, 0);
  add_read_handler("inputs", read_element_inputs, 0);
  add_read_handler("outputs", read_element_outputs, 0);
  add_read_handler("tickets", read_element_tickets, 0);
#if CLICK_STATS >= 1
  add_read_handler("icounts", read_element_icounts, 0);
  add_read_handler("ocounts", read_element_ocounts, 0);
# if CLICK_STATS >= 2
  add_read_handler("cycles", read_element_cycles, 0);
# endif
#endif
}

void
Element::add_handlers()
{
}

String
Element::configuration_read_handler(Element *element, void *vno)
{
  Router *router = element->router();
  Vector<String> args;
  cp_argvec(router->econfiguration(element->elementno()), args);
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
  cp_argvec(router->econfiguration(element->elementno()), args);
  int no = (int)vno;
  while (args.size() <= no)
    args.push_back(String());
  args[no] = arg;
  if (router->live_reconfigure(element->elementno(), args, errh) < 0)
    return -EINVAL;
  else
    return 0;
}

void
Element::set_configuration(const String &conf)
{
  router()->set_configuration(elementno(), conf);
}

void
Element::set_configuration_argument(int which, const String &arg)
{
  assert(which >= 0);
  Vector<String> args;
  cp_argvec(router()->econfiguration(elementno()), args);
  while (args.size() <= which)
    args.push_back(String());
  args[which] = arg;
  router()->set_configuration(elementno(), cp_unargvec(args));
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

void
Element::run_scheduled()
{
  assert(0 && "bad run_scheduled");
}
