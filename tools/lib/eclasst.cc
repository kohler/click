// -*- c-basic-offset: 4 -*-
/*
 * eclass.{cc,hh} -- tool definition of element classes
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

#include "eclasst.hh"
#include "routert.hh"
#include "elementmap.hh"
#include "processingt.hh"
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/variableenv.hh>
#include <stdlib.h>

static String::Initializer string_initializer;
static HashMap<String, int> default_class_map(-1);
static Vector<ElementClassT *> default_classes;
static int unique_id_storage = ElementClassT::UNUSED_UID;

typedef ElementTraits Traits;

class TraitsElementClassT : public ElementClassT { public:
    TraitsElementClassT(const String &, int uid, int component, ...);
    const ElementTraits *find_traits() const;
  private:
    ElementTraits _the_traits;
};

TraitsElementClassT::TraitsElementClassT(const String &name, int uid, int component, ...)
    : ElementClassT(name, uid)
{
    *(_the_traits.component(Traits::D_CLASS)) = name;
    va_list val;
    va_start(val, component);
    while (component > Traits::D_NONE) {
	const char *x = va_arg(val, const char *);
	if (String *val = _the_traits.component(component))
	    *val = x;
	component = va_arg(val, int);
    }
    va_end(val);
}

const ElementTraits *
TraitsElementClassT::find_traits() const
{
    return &_the_traits;
}

ElementClassT *ElementClassT::the_unused_type = new TraitsElementClassT("<unused>", UNUSED_UID, Traits::D_REQUIREMENTS, "false", 0);
ElementClassT *ElementClassT::the_tunnel_type = new TraitsElementClassT("<tunnel>", TUNNEL_UID, Traits::D_PROCESSING, "a/a", Traits::D_FLOW_CODE, "x/y", 0);


ElementClassT::ElementClassT(const String &name)
    : _name(name), _use_count(1), _unique_id(unique_id_storage++),
      _traits_version(-1)
{
}

ElementClassT::ElementClassT(const String &name, int uid)
    : _name(name), _use_count(1), _unique_id(uid), _traits_version(-1)
{
    assert(uid >= unique_id_storage);
    unique_id_storage = uid + 1;
}

const char *
ElementClassT::printable_name_cc()
{
    if (_name)
	return _name.cc();
    else
	return "<anonymous>";
}

void
ElementClassT::set_default_class(ElementClassT *ec)
{
    int i = default_class_map[ec->name()];
    if (i < 0) {
	default_class_map.insert(ec->name(), default_classes.size());
	default_classes.push_back(ec);
    } else {
	default_classes[i]->unuse();
	default_classes[i] = ec;
    }
    ec->use();
}

ElementClassT *
ElementClassT::default_class(const String &name)
{
    int i = default_class_map[name];
    if (i >= 0)
	return default_classes[i];
    ElementClassT *ec = new ElementClassT(name);
    set_default_class(ec);
    return ec;
}


const ElementTraits *
ElementClassT::find_traits() const
{
    ElementMap *em = ElementMap::default_map();
    assert(em);
    return &em->traits(_name);
}

const String &
ElementClassT::package() const
{
    return ElementMap::default_map()->package(traits());
}

String
ElementClassT::documentation_url() const
{
    return ElementMap::default_map()->documentation_url(traits());
}


ElementClassT *
ElementClassT::find_relevant_class(int, int, const Vector<String> &)
{
    return this;
}

void
ElementClassT::report_signatures(const String &lm, String name, ErrorHandler *errh)
{
    errh->lmessage(lm, "`%s[...]'", name.cc());
}

ElementT *
ElementClassT::direct_expand_element(
	ElementT *e, RouterT *tor,
	const VariableEnvironment &env, ErrorHandler *errh)
{
    RouterT *fromr = e->router();
    String new_name = env.prefix() + e->name();
    String new_configuration = env.interpolate(e->configuration());

    // check for tunnel
    if (e->tunnel()) {
	assert(this == ElementClassT::tunnel_type());
	// common case -- expanding router into itself
	if (fromr == tor && !env.prefix())
	    return e;
	// make the tunnel or tunnel pair
	if (e->tunnel_output()) {
	    tor->add_tunnel(new_name,
			    env.prefix() + e->tunnel_output()->name(),
			    e->landmark(), errh);
	    return tor->element(new_name);
	} else
	    return tor->get_element
		(new_name, e->type(), new_configuration, e->landmark());
    }
    
    // otherwise, not tunnel
	  
    // check for common case -- expanding router into itself
    if (fromr == tor && !env.prefix()) {
	e->configuration() = new_configuration;
	e->set_type(this);
	return e;
    }

    // check for old element
    if (ElementT *new_e = tor->element(new_name))
	ElementT::redeclaration_error(errh, "element", new_name, e->landmark(), new_e->landmark());
    
    // add element
    return tor->get_element(new_name, this, new_configuration, e->landmark());
}

ElementT *
ElementClassT::expand_element(
	ElementT *e, RouterT *tor,
	const VariableEnvironment &env, ErrorHandler *errh)
{
    ElementClassT *c = e->type();
    if (c->simple())
	return c->direct_expand_element(e, tor, env, errh);

    // if not direct expansion, do some more work
    int inputs_used = e->ninputs();
    int outputs_used = e->noutputs();
    RouterT *fromr = e->router();

    Vector<String> args;
    String new_configuration = env.interpolate(e->configuration());
    cp_argvec(new_configuration, args);

    ElementClassT *found_c = c->find_relevant_class(inputs_used, outputs_used, args);
    if (!found_c) {
	String lm = e->landmark(), name = e->type_name();
	errh->lerror(lm, "no match for `%s'", name.cc(), signature(name, inputs_used, outputs_used, args.size()).cc());
	ContextErrorHandler cerrh(errh, "possibilities are:", "  ");
	c->report_signatures(lm, name, &cerrh);
	if (fromr == tor)
	    e->kill();
	return 0;
    }

    return found_c->complex_expand_element
	(e, new_configuration, args,
	 tor, env, errh);
}

ElementT *
ElementClassT::complex_expand_element(
	ElementT *e, const String &, Vector<String> &,
	RouterT *tor, const VariableEnvironment &env, ErrorHandler *errh)
{
    return direct_expand_element(e, tor, env, errh);
}

void
ElementClassT::collect_primitive_classes(HashMap<String, int> &m)
{
    if (_unique_id > TUNNEL_UID)
	m.insert(_name, 1);
}

void
ElementClassT::collect_prerequisites(Vector<ElementClassT *> &)
{
}

void
ElementClassT::unparse_declaration(StringAccum &, const String &)
{
}

String
ElementClassT::signature(const String &name, int ninputs, int noutputs, int nargs)
{
    const char *pl_args = (nargs == 1 ? " argument, " : " arguments, ");
    const char *pl_ins = (ninputs == 1 ? " input, " : " inputs, ");
    const char *pl_outs = (noutputs == 1 ? " output" : " outputs");
    StringAccum sa;
    sa << name << '[' << nargs << pl_args << ninputs << pl_ins << noutputs << pl_outs << ']';
    return sa.take_string();
}


SynonymElementClassT::SynonymElementClassT(const String &name, ElementClassT *eclass)
    : ElementClassT(name), _eclass(eclass)
{
    assert(eclass);
}

ElementClassT *
SynonymElementClassT::find_relevant_class(int ninputs, int noutputs, const Vector<String> &args)
{
    return _eclass->find_relevant_class(ninputs, noutputs, args);
}

ElementT *
SynonymElementClassT::complex_expand_element(
	ElementT *, const String &, Vector<String> &,
	RouterT *, const VariableEnvironment &, ErrorHandler *)
{
    assert(0);
    return 0;
}

void
SynonymElementClassT::collect_primitive_classes(HashMap<String, int> &m)
{
    _eclass->collect_primitive_classes(m);
}

void
SynonymElementClassT::collect_prerequisites(Vector<ElementClassT *> &v)
{
    _eclass->collect_prerequisites(v);
    v.push_back(_eclass);
}

void
SynonymElementClassT::unparse_declaration(StringAccum &sa, const String &indent)
{
    sa << indent << "elementclass " << name() << " " << _eclass->name() << ";\n";
}

const ElementTraits *
SynonymElementClassT::find_traits() const
{
    return _eclass->find_traits();
}

CompoundElementClassT *
SynonymElementClassT::cast_compound()
{
    return _eclass->cast_compound();
}

RouterT *
SynonymElementClassT::cast_router()
{
    return _eclass->cast_router();
}


//
// Compound
//

CompoundElementClassT::CompoundElementClassT
	(const String &name, ElementClassT *prev, int depth,
	 RouterT *enclosing_router, const String &landmark)
    : ElementClassT(name), _landmark(landmark), _depth(depth),
      _ninputs(0), _noutputs(0), _prev(prev),
      _circularity_flag(false)
{
    _router = new RouterT(this, enclosing_router);
    _router->get_element("input", ElementClassT::tunnel_type(), String(), landmark);
    _router->get_element("output", ElementClassT::tunnel_type(), String(), landmark);
    _router->use();
    if (_prev)
	_prev->use();
}

CompoundElementClassT::CompoundElementClassT(const String &name, RouterT *r)
    : ElementClassT(name), _router(r), _depth(0), _ninputs(0), _noutputs(0),
      _prev(0), _circularity_flag(false)
{
    _router->use();
    *(_traits.component(Traits::D_CLASS)) = name;
}

CompoundElementClassT::~CompoundElementClassT()
{
    _router->unuse();
    if (_prev)
	_prev->unuse();
}

void
CompoundElementClassT::finish(ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::silent_handler();

    if (ElementT *einput = _router->element("input")) {
	_ninputs = einput->noutputs();
	if (einput->ninputs())
	    errh->lerror(_landmark, "`%s' pseudoelement `input' may only be used as output", printable_name_cc());

	if (_ninputs) {
	    Vector<int> used;
	    _router->find_connection_vector_from(einput, used);
	    assert(used.size() == _ninputs);
	    for (int i = 0; i < _ninputs; i++)
		if (used[i] == -1)
		    errh->lerror(_landmark, "compound element `%s' input %d unused", printable_name_cc(), i);
	}
    } else
	_ninputs = 0;

    if (ElementT *eoutput = _router->element("output")) {
	_noutputs = eoutput->ninputs();
	if (eoutput->noutputs())
	    errh->lerror(_landmark, "`%s' pseudoelement `output' may only be used as input", printable_name_cc());

	if (_noutputs) {
	    Vector<int> used;
	    _router->find_connection_vector_to(eoutput, used);
	    assert(used.size() == _noutputs);
	    for (int i = 0; i < _noutputs; i++)
		if (used[i] == -1)
		    errh->lerror(_landmark, "compound element `%s' output %d unused", printable_name_cc(), i);
	}
    } else
	_noutputs = 0;

    // resolve anonymous element names
    _router->deanonymize_elements();
}

void
CompoundElementClassT::check_duplicates_until(ElementClassT *last, ErrorHandler *errh)
{
  if (this == last || !_prev)
    return;
  
  ElementClassT *n = _prev;
  while (n && n != last) {
    CompoundElementClassT *nc = n->cast_compound();
    if (!nc) break;
    if (nc->_ninputs == _ninputs && nc->_noutputs == _noutputs && nc->_formals.size() == _formals.size()) {
      ElementT::redeclaration_error(errh, "", signature(), _landmark, nc->_landmark);
      break;
    }
    n = nc->_prev;
  }

  if (CompoundElementClassT *nc = _prev->cast_compound())
    nc->check_duplicates_until(last, errh);
}

ElementClassT *
CompoundElementClassT::find_relevant_class(int ninputs, int noutputs, const Vector<String> &args)
{
    // Try to return an element class, even if it is wrong -- the error
    // messages are friendlier
    CompoundElementClassT *ct = this;
    CompoundElementClassT *closest = 0;
    int nclosest = 0;

    while (1) {
	if (ct->_ninputs == ninputs && ct->_noutputs == noutputs && ct->_formals.size() == args.size())
	    return ct;

	// replace `closest'
	if (ct->_formals.size() == args.size()) {
	    closest = ct;
	    nclosest++;
	}

	ElementClassT *e = ct->_prev;
	if (!e)
	    return (nclosest == 1 ? closest : 0);
	else if (CompoundElementClassT *next = e->cast_compound())
	    ct = next;
	else
	    return e->find_relevant_class(ninputs, noutputs, args);
    }
}

String
CompoundElementClassT::signature() const
{
    return ElementClassT::signature(name(), _ninputs, _noutputs, _formals.size());
}

void
CompoundElementClassT::report_signatures(const String &lm, String name, ErrorHandler *errh)
{
    if (_prev)
	_prev->report_signatures(lm, name, errh);
    errh->lmessage(lm, "`%s'", signature().cc());
}

ElementT *
CompoundElementClassT::complex_expand_element(
	ElementT *compound, const String &, Vector<String> &args,
	RouterT *tor, const VariableEnvironment &env, ErrorHandler *errh)
{
    RouterT *fromr = compound->router();
    assert(fromr != _router && tor != _router);
    assert(!_circularity_flag);
    _circularity_flag = true;

    // parse configuration string
    int nargs = _formals.size();
    if (args.size() != nargs) {
	const char *whoops = (args.size() < nargs ? "few" : "many");
	String signature;
	for (int i = 0; i < nargs; i++) {
	    if (i) signature += ", ";
	    signature += _formals[i];
	}
	if (errh)
	    errh->lerror(compound->landmark(),
			 "too %s arguments to compound element `%s(%s)'",
			 whoops, printable_name_cc(), signature.cc());
	for (int i = args.size(); i < nargs; i++)
	    args.push_back("");
    }

    // create prefix
    assert(compound->name());
    VariableEnvironment new_env(env, compound->name());
    String prefix = env.prefix();
    String new_prefix = new_env.prefix(); // includes previous prefix
    new_env.limit_depth(_depth);
    new_env.enter(_formals, args, _depth);

    // create input/output tunnels
    if (fromr == tor)
	compound->set_type(tunnel_type());
    tor->add_tunnel(prefix + compound->name(), new_prefix + "input", compound->landmark(), errh);
    tor->add_tunnel(new_prefix + "output", prefix + compound->name(), compound->landmark(), errh);
    ElementT *new_e = tor->element(prefix + compound->name());

    // dump compound router into `tor'
    _router->expand_into(tor, new_env, errh);

    // yes, we expanded it
    _circularity_flag = false;
    return new_e;
}

void
CompoundElementClassT::collect_primitive_classes(HashMap<String, int> &m)
{
    _router->collect_primitive_classes(m);
    if (_prev)
	_prev->collect_primitive_classes(m);
}

void
CompoundElementClassT::collect_prerequisites(Vector<ElementClassT *> &v)
{
    if (_prev) {
	_prev->collect_prerequisites(v);
	v.push_back(_prev);
    }
}

const ElementTraits *
CompoundElementClassT::find_traits() const
{
    if (ElementMap::default_map()) {
	ErrorHandler *errh = ErrorHandler::silent_handler();
	ProcessingT pt(_router, errh);
	*(_traits.component(Traits::D_PROCESSING)) = pt.compound_processing_code();
	*(_traits.component(Traits::D_FLOW_CODE)) = pt.compound_flow_code(errh);
    }
    return &_traits;
}

void
CompoundElementClassT::unparse_declaration(StringAccum &sa, const String &indent)
{
    assert(!_circularity_flag);
    _circularity_flag = true;

    sa << indent << "elementclass " << name() << " {";

    // print formals
    for (int i = 0; i < _formals.size(); i++)
	sa << (i ? ", " : " ") << _formals[i];
    if (_formals.size())
	sa << " |";
    sa << "\n";

    _router->unparse(sa, indent + "  ");

    sa << indent << "}\n";
    _circularity_flag = false;
}
