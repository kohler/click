// -*- c-basic-offset: 4 -*-
/*
 * eclasst.{cc,hh} -- tool definition of element classes
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
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

static HashTable<String, int> base_type_map(-1);
static Vector<ElementClassT *> base_types;

typedef ElementTraits Traits;

namespace {

class TraitsElementClassT : public ElementClassT { public:
    TraitsElementClassT(const String &, int component, ...);
    bool primitive() const		{ return false; }
    const ElementTraits *find_traits(ElementMap *emap) const;
  private:
    ElementTraits _the_traits;
};

TraitsElementClassT::TraitsElementClassT(const String &name, int component, ...)
    : ElementClassT(name)
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
    use();			// make sure we hold onto this forever
}

const ElementTraits *
TraitsElementClassT::find_traits(ElementMap *) const
{
    return &_the_traits;
}

}

ElementClassT *ElementClassT::the_tunnel_type = new TraitsElementClassT("<tunnel>", Traits::D_PROCESSING, "a/a", Traits::D_FLOW_CODE, "x/y", 0);


ElementClassT::ElementClassT(const String &name)
    : _name(name),
      _printable_name(name ? name : String::make_stable("<anonymous>")),
      _use_count(0), _traits_version(-1)
{
    //fprintf(stderr, "%p: %s\n", this, printable_name_c_str());
}

ElementClassT::~ElementClassT()
{
    //fprintf(stderr, "%p: ~%s\n", this, printable_name_c_str());
}

static ElementClassT *default_base_type_factory(const String &name)
{
    return new ElementClassT(name);
}

static ElementClassT *(*base_type_factory)(const String &) = default_base_type_factory;

void ElementClassT::set_base_type_factory(ElementClassT *(*f)(const String &))
{
    base_type_factory = f;
}

ElementClassT *
ElementClassT::base_type(const String &name)
{
    if (!name)
	return 0;
    int &i = base_type_map[name];
    if (i < 0) {
	i = base_types.size();
	ElementClassT *t = base_type_factory(name);
	assert(t && t->name() == name);
	t->use();
	base_types.push_back(t);
    }
    return base_types[i];
}


ElementTraits &
ElementClassT::force_traits(ElementMap *emap) const
{
    ElementTraits &traits = emap->force_traits(_name);
    _traits = &traits;
    _traits_version = emap->version();
    return traits;
}

const ElementTraits *
ElementClassT::find_traits(ElementMap *emap) const
{
    return &emap->traits(_name);
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


bool
ElementClassT::need_resolve() const
{
    return false;
}

ElementClassT *
ElementClassT::resolve(int, int, Vector<String> &, ErrorHandler *, const LandmarkT &)
{
    return this;
}

void
ElementClassT::create_scope(const Vector<String> &, const VariableEnvironment &, VariableEnvironment &)
{
}

ElementT *
ElementClassT::direct_expand_element(
	ElementT *e, RouterT *tor, const String &prefix,
	const VariableEnvironment &env, ErrorHandler *errh)
{
    assert(!prefix || prefix.back() == '/');
    RouterT *fromr = e->router();
    String new_name = prefix + e->name();
    String new_configuration = cp_expand(e->configuration(), env);

    // check for tunnel
    if (e->tunnel()) {
	assert(this == ElementClassT::tunnel_type());
	// common case -- expanding router into itself
	if (fromr == tor && !prefix)
	    return e;
	// make the tunnel or tunnel pair
	if (e->tunnel_output()) {
	    tor->add_tunnel(new_name,
			    prefix + e->tunnel_output()->name(),
			    e->landmarkt(), errh);
	    return tor->element(new_name);
	} else
	    return tor->get_element
		(new_name, e->type(), new_configuration, e->landmarkt());
    }

    // otherwise, not tunnel

    // check for common case -- expanding router into itself
    if (fromr == tor && !prefix) {
	e->set_configuration(new_configuration);
	e->set_type(this);
	return e;
    }

    // check for old element
    if (ElementT *new_e = tor->element(new_name))
	ElementT::redeclaration_error(errh, "element", new_name, e->landmark(), new_e->landmark());

    // add element
    return tor->get_element(new_name, this, new_configuration, e->landmarkt());
}

ElementT *
ElementClassT::expand_element(
	ElementT *e, RouterT *tor, const String &prefix,
	const VariableEnvironment &env, ErrorHandler *errh)
{
    ElementClassT *c = e->type();
    if (c->primitive())
	return c->direct_expand_element(e, tor, prefix, env, errh);

    // if not direct expansion, do some more work
    int inputs_used = e->ninputs();
    int outputs_used = e->noutputs();
    RouterT *fromr = e->router();

    Vector<String> args;
    String new_configuration = cp_expand(e->configuration(), env);
    cp_argvec(new_configuration, args);

    ElementClassT *found_c = c->resolve(inputs_used, outputs_used, args, errh, e->landmarkt());
    if (!found_c) {		// destroy element
	if (fromr == tor)
	    e->kill();
	return 0;
    }

    return found_c->complex_expand_element(e, args, tor, prefix, env, errh);
}

ElementT *
ElementClassT::complex_expand_element(
	ElementT *e, const Vector<String> &,
	RouterT *tor, const String &prefix,
	const VariableEnvironment &env, ErrorHandler *errh)
{
    return direct_expand_element(e, tor, prefix, env, errh);
}

String
ElementClassT::unparse_signature(const String &name, const Vector<String> *formal_types, int nargs, int ninputs, int noutputs)
{
    StringAccum sa;
    sa << (name ? name : String("<anonymous>"));

    if (formal_types && formal_types->size()) {
	sa << '(';
	for (int i = 0; i < formal_types->size(); i++) {
	    if (i)
		sa << ", ";
	    if ((*formal_types)[i] == "")
		sa << "<arg>";
	    else if ((*formal_types)[i] == "__REST__")
		sa << "...";
	    else
		sa << (*formal_types)[i];
	}
	sa << ')';
    }

    const char *pl_args = (nargs == 1 ? " argument, " : " arguments, ");
    const char *pl_ins = (ninputs == 1 ? " input, " : " inputs, ");
    const char *pl_outs = (noutputs == 1 ? " output" : " outputs");
    sa << '[';
    if (!formal_types && nargs > 0)
	sa << nargs << pl_args;
    sa << ninputs << pl_ins << noutputs << pl_outs;
    sa << ']';

    return sa.take_string();
}


SynonymElementClassT::SynonymElementClassT(const String &name, ElementClassT *eclass, RouterT *declaration_scope)
    : ElementClassT(name), _eclass(eclass), _declaration_scope(declaration_scope)
{
    assert(eclass);
}

bool
SynonymElementClassT::need_resolve() const
{
    return true;
}

ElementClassT *
SynonymElementClassT::resolve(int ninputs, int noutputs, Vector<String> &args, ErrorHandler *errh, const LandmarkT &landmark)
{
    return _eclass->resolve(ninputs, noutputs, args, errh, landmark);
}

void
SynonymElementClassT::create_scope(const Vector<String> &, const VariableEnvironment &, VariableEnvironment &)
{
    assert(0);
}

ElementT *
SynonymElementClassT::complex_expand_element(
	ElementT *, const Vector<String> &,
	RouterT *, const String &, const VariableEnvironment &, ErrorHandler *)
{
    assert(0);
    return 0;
}

const ElementTraits *
SynonymElementClassT::find_traits(ElementMap *emap) const
{
    return _eclass->find_traits(emap);
}

RouterT *
SynonymElementClassT::cast_router()
{
    return _eclass->cast_router();
}


RouterT *
ElementClassT::declaration_scope() const
{
    return 0;
}

RouterT *
SynonymElementClassT::declaration_scope() const
{
    return _declaration_scope;
}


ElementClassT *
ElementClassT::overload_type() const
{
    return 0;
}


String
ElementClassT::unparse_signature() const
{
    return name() + "[...]";
}


void
ElementClassT::collect_types(HashTable<ElementClassT *, int> &m) const
{
    m.set(const_cast<ElementClassT *>(this), 1);
}

void
SynonymElementClassT::collect_types(HashTable<ElementClassT *, int> &m) const
{
    HashTable<ElementClassT *, int>::iterator it = m.find_insert(const_cast<SynonymElementClassT *>(this), 0);
    if (it != m.end() && it.value() == 0) {
	it.value() = 1;
	_eclass->collect_types(m);
    }
}


void
ElementClassT::collect_overloads(Vector<ElementClassT *> &v) const
{
    v.push_back(const_cast<ElementClassT *>(this));
}

void
SynonymElementClassT::collect_overloads(Vector<ElementClassT *> &v) const
{
    _eclass->collect_overloads(v);
}
