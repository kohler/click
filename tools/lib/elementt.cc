/*
 * elementt.{cc,hh} -- tool definition of element
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

#include "elementt.hh"
#include "routert.hh"
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/variableenv.hh>
#include <stdlib.h>

ElementT::ElementT()
  : type(-1), tunnel_input(-1), tunnel_output(-1), flags(0)
{
}

ElementT::ElementT(const String &n, int type_index, const String &config,
		   const String &lm)
  : type(type_index), name(n), configuration(config),
    tunnel_input(-1), tunnel_output(-1), landmark(lm), flags(0)
{
}


int
Hookup::index_in(const Vector<Hookup> &v, int start) const
{
  int size = v.size();
  for (int i = start; i < size; i++)
    if (v[i] == *this)
      return i;
  return -1;
}

int
Hookup::force_index_in(Vector<Hookup> &v, int start) const
{
  int size = v.size();
  for (int i = start; i < size; i++)
    if (v[i] == *this)
      return i;
  v.push_back(*this);
  return size;
}

int
Hookup::sorter(const void *av, const void *bv)
{
  const Hookup *a = (const Hookup *)av, *b = (const Hookup *)bv;
  if (a->idx == b->idx)
    return a->port - b->port;
  else
    return a->idx - b->idx;
}

void
Hookup::sort(Vector<Hookup> &v)
{
  qsort(&v[0], v.size(), sizeof(Hookup), &sorter);
}


ElementClassT::ElementClassT()
  : _use_count(1)
{
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

int
ElementClassT::direct_expand_element(RouterT *fromr, int which, ElementClassT *thus, RouterT *tor,
				     const VariableEnvironment &env, ErrorHandler *errh)
{
  ElementT &e = fromr->element(which);
  String new_name = env.prefix() + e.name;
  String new_configuration = env.interpolate(e.configuration);
  
  switch (e.type) {

   case RouterT::TUNNEL_TYPE: {
     // common case -- expanding router into itself
     if (fromr == tor && !env.prefix())
       return which;
     // make the tunnel or tunnel pair
     if (e.tunnel_output >= 0 && e.tunnel_output < fromr->nelements()) {
       tor->add_tunnel(new_name,
		       env.prefix() + fromr->ename(e.tunnel_output),
		       e.landmark, errh);
       return tor->eindex(new_name);
     } else
       return tor->get_eindex
	 (new_name, RouterT::TUNNEL_TYPE, e.configuration, e.landmark);
   }

   default: {
     // get element type index. add new "anonymous" element type if needed
     String type_name = fromr->type_name(e.type);
     int new_type = tor->get_type_index(type_name, thus);
     ElementClassT *new_type_class = tor->type_class(new_type);
     if (thus != new_type_class && new_type_class)
       new_type = tor->get_anon_type_index(type_name, thus);
     
     // check for common case -- expanding router into itself
     if (fromr == tor && !env.prefix()) {
       e.configuration = new_configuration;
       e.type = new_type;
       return which;
     }

     // check for old element
     int new_eidx = tor->eindex(new_name);
     if (new_eidx >= 0) {
       errh->lerror(e.landmark, "redeclaration of element `%s'", new_name.cc());
       errh->lerror(tor->elandmark(new_eidx), "`%s' previously declared here", tor->edeclaration(new_eidx).cc());
     }
     
     // add element
     return tor->get_eindex(new_name, new_type, new_configuration, e.landmark);
   }

  }
}

int
ElementClassT::expand_element(RouterT *fromr, int which, RouterT *tor,
			      const VariableEnvironment &env, ErrorHandler *errh)
{
  ElementClassT *c = fromr->etype_class(which);

  if (!c || c->direct_expansion())
    return direct_expand_element(fromr, which, c, tor, env, errh);

  // if not direct expansion, do some more work
  int inputs_used = fromr->ninputs(which);
  int outputs_used = fromr->noutputs(which);

  Vector<String> args;
  String new_configuration = env.interpolate(fromr->econfiguration(which));
  cp_argvec(new_configuration, args);

  ElementClassT *found_c = c->find_relevant_class(inputs_used, outputs_used, args);
  if (!found_c) {
    String lm = fromr->elandmark(which), name = fromr->etype_name(which);
    errh->lerror(lm, "no match for `%s'", name.cc(), signature(name, inputs_used, outputs_used, args.size()).cc());
    ContextErrorHandler cerrh(errh, "possibilities are:", "  ");
    c->report_signatures(lm, name, &cerrh);
    if (fromr == tor)
      tor->kill_element(which);
    return -1;
  }

  return found_c->complex_expand_element(fromr, which, new_configuration, args,
					 tor, env, errh);
}

int
ElementClassT::complex_expand_element(RouterT *fromr, int which, const String &, Vector<String> &,
				      RouterT *tor, const VariableEnvironment &env, ErrorHandler *errh)
{
  return direct_expand_element(fromr, which, this, tor, env, errh);
}

void
ElementClassT::unparse_declaration(StringAccum &, const String &, const String &)
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
  : _name(name), _eclass(eclass)
{
}

ElementClassT *
SynonymElementClassT::find_relevant_class(int ninputs, int noutputs, const Vector<String> &args)
{
  if (!_eclass)
    return this;
  else
    return _eclass->find_relevant_class(ninputs, noutputs, args);
}

int
SynonymElementClassT::complex_expand_element(
	RouterT *fromr, int which, const String &new_config, Vector<String> &,
	RouterT *tor, const VariableEnvironment &env, ErrorHandler *errh)
{
  // get element type index. add new "anonymous" element type if needed
  int new_type = tor->get_type_index(_name, _eclass);
  ElementClassT *new_type_class = tor->type_class(new_type);
  if (_eclass != new_type_class)
    new_type = tor->get_anon_type_index(_name, _eclass);

  ElementT &e = fromr->element(which);
  if (fromr == tor && !env.prefix()) {
    e.configuration = new_config; // interpolated
    e.type = new_type;
    return which;
  } else {
    String new_name = env.prefix() + e.name;
    int new_eidx = tor->eindex(new_name);
    if (new_eidx >= 0) {
      errh->lerror(e.landmark, "redeclaration of element `%s'", new_name.cc());
      errh->lerror(tor->elandmark(new_eidx), "`%s' previously declared here", tor->edeclaration(new_eidx).cc());
      return -1;
    } else
      return tor->get_eindex(new_name, new_type, new_config, e.landmark);
  }
}

void
SynonymElementClassT::unparse_declaration(StringAccum &sa, const String &name, const String &indent)
{
  sa << indent << "elementclass " << name << " " << _name << ";\n";
}

CompoundElementClassT *
SynonymElementClassT::cast_compound()
{
  return (_eclass ? _eclass->cast_compound() : 0);
}

RouterT *
SynonymElementClassT::cast_router()
{
  return (_eclass ? _eclass->cast_router() : 0);
}

CompoundElementClassT::CompoundElementClassT(RouterT *r)
  : _router(r), _depth(0), _ninputs(0), _noutputs(0), _next(0),
    _circularity_flag(false)
{
  _router->use();
}

CompoundElementClassT::CompoundElementClassT(const String &name, const String &landmark, RouterT *r, ElementClassT *next, int depth)
  : _name(name), _landmark(landmark), _router(r), _depth(depth),
    _ninputs(0), _noutputs(0), _next(next),
    _circularity_flag(false)
{
  _router->use();
  if (_next)
    _next->use();
}

CompoundElementClassT::~CompoundElementClassT()
{
  _router->unuse();
  if (_next)
    _next->unuse();
}

void
CompoundElementClassT::finish(ErrorHandler *errh)
{
  if (!errh)
    errh = ErrorHandler::silent_handler();

  int einput = _router->eindex("input");
  if (einput >= 0) {
    _ninputs = _router->noutputs(einput);
    if (_router->ninputs(einput))
      errh->lerror(_landmark, "`%s' pseudoelement `input' may only be used as output", _name.cc());

    if (_ninputs) {
      Vector<int> used;
      _router->find_connection_vector_from(einput, used);
      assert(used.size() == _ninputs);
      for (int i = 0; i < _ninputs; i++)
	if (used[i] == -1)
	  errh->lerror(_landmark, "compound element `%s' input %d unused", _name.cc(), i);
    }
  } else
    _ninputs = 0;

  int eoutput = _router->eindex("output");
  if (eoutput >= 0) {
    _noutputs = _router->ninputs(eoutput);
    if (_router->noutputs(eoutput))
      errh->lerror(_landmark, "`%s' pseudoelement `output' may only be used as input", _name.cc());

    if (_noutputs) {
      Vector<int> used;
      _router->find_connection_vector_to(eoutput, used);
      assert(used.size() == _noutputs);
      for (int i = 0; i < _noutputs; i++)
	if (used[i] == -1)
	  errh->lerror(_landmark, "compound element `%s' output %d unused", _name.cc(), i);
    }
  } else
    _noutputs = 0;
}

void
CompoundElementClassT::check_duplicates_until(ElementClassT *last, ErrorHandler *errh)
{
  if (this == last || !_next)
    return;
  
  ElementClassT *n = _next;
  while (n && n != last) {
    CompoundElementClassT *nc = n->cast_compound();
    if (!nc) break;
    if (nc->_ninputs == _ninputs && nc->_noutputs == _noutputs && nc->_formals.size() == _formals.size()) {
      errh->lerror(_landmark, "redeclaration of `%s'", signature().cc());
      errh->lerror(nc->_landmark, "`%s' previously declared here", signature().cc());
      break;
    }
    n = nc->_next;
  }

  if (CompoundElementClassT *nc = _next->cast_compound())
    nc->check_duplicates_until(last, errh);
}

ElementClassT *
CompoundElementClassT::find_relevant_class(int ninputs, int noutputs, const Vector<String> &args)
{
  // Try to return an element class, even if it is wrong -- the error messages
  // are friendlier
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

    ElementClassT *e = ct->_next;
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
  return ElementClassT::signature(_name, _ninputs, _noutputs, _formals.size());
}

void
CompoundElementClassT::report_signatures(const String &lm, String name, ErrorHandler *errh)
{
  if (_next)
    _next->report_signatures(lm, name, errh);
  errh->lmessage(lm, "`%s'", signature().cc());
}

int
CompoundElementClassT::complex_expand_element(
	RouterT *fromr, int which, const String &, Vector<String> &args,
	RouterT *tor, const VariableEnvironment &env, ErrorHandler *errh)
{
  assert(fromr != _router && tor != _router);
  assert(!_circularity_flag);
  _circularity_flag = true;
  
  // must make a copy of `compound' because we might be growing the _elements
  // vector, in which case our reference would die
  ElementT compound = fromr->element(which);
  
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
      errh->lerror(compound.landmark,
		   "too %s arguments to compound element `%s(%s)'", whoops,
		   compound.name.cc() /* XXX should be class_name */, signature.cc());
    for (int i = args.size(); i < nargs; i++)
      args.push_back("");
  }

  // create prefix
  assert(compound.name);
  VariableEnvironment new_env(env, compound.name);
  String prefix = env.prefix();
  String new_prefix = new_env.prefix(); // includes previous prefix
  new_env.limit_depth(_depth);
  new_env.enter(_formals, args, _depth);

  // create input/output tunnels
  if (fromr == tor)
    tor->element(which).type = RouterT::TUNNEL_TYPE;
  tor->add_tunnel(prefix + compound.name, new_prefix + "input", compound.landmark, errh);
  tor->add_tunnel(new_prefix + "output", prefix + compound.name, compound.landmark, errh);
  int new_eindex = tor->eindex(prefix + compound.name);

  // dump compound router into `tor'
  _router->expand_into(tor, new_env, errh);
  
  // yes, we expanded it
  _circularity_flag = false;
  return new_eindex;
}

void
CompoundElementClassT::unparse_declaration(StringAccum &sa, const String &name, const String &indent)
{
  assert(!_circularity_flag);
  _circularity_flag = true;

  sa << indent << "elementclass " << name << " {";
  
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
