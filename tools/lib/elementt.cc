/*
 * elementt.{cc,hh} -- tool definition of element
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "elementt.hh"
#include "routert.hh"
#include "straccum.hh"
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

static int
resolve_upref(const String &upref, String prefix, RouterT *r)
{
  while (prefix) {
    int pos = prefix.find_right('/', prefix.length() - 2);
    prefix = (pos >= 0 ? prefix.substring(0, pos + 1) : String());
    
    String try_name = prefix + upref;
    int en = r->eindex(try_name);
    if (en >= 0) return en;
  }
  return -1;
}

int
ElementClassT::simple_expand_into(RouterT *fromr, int which,
				  RouterT *tor,
				  const RouterScope &scope, ErrorHandler *errh)
{
  if (fromr == tor)
    return which;

  const ElementT &e = fromr->element(which);
  
  if (e.type == RouterT::UPREF_TYPE) {
    // try and resolve the upref within the current prefix. if not, then
    // pass the upref on up, without tacking on the prefix.
    int new_index = resolve_upref(e.name, scope.prefix(), tor);
    if (new_index < 0)
      new_index = tor->get_anon_eindex(e.name, RouterT::UPREF_TYPE,
				       e.configuration, e.landmark);
    return new_index;
  }

  if (e.type == RouterT::TUNNEL_TYPE) {
    // make the tunnel or tunnel pair
    String new_name = scope.prefix() + e.name;
    if (e.tunnel_output >= 0 && e.tunnel_output < fromr->nelements()) {
      tor->add_tunnel(new_name,
		      scope.prefix() + fromr->ename(e.tunnel_output),
		      e.landmark, errh);
      return tor->eindex(new_name);
    } else
      return tor->get_eindex
	(new_name, RouterT::TUNNEL_TYPE, e.configuration, e.landmark);
  }
  
  // get element type index. add new "anonymous" element type if needed
  String type_name = fromr->type_name(e.type);
  ElementClassT *type_class = fromr->type_class(e.type);
  int new_type = tor->get_type_index(type_name, type_class);
  ElementClassT *new_type_class = tor->type_class(new_type);
  if (type_class != new_type_class && new_type_class)
    new_type = tor->get_anon_type_index(type_name, type_class);
  
  // add element
  return tor->get_eindex(scope.prefix() + e.name, new_type,
			 scope.interpolate(e.configuration), e.landmark);
}

int
ElementClassT::expand_into(RouterT *fromr, int which,
			   RouterT *tor, const RouterScope &scope,
			   ErrorHandler *errh)
{
  return simple_expand_into(fromr, which, tor, scope, errh);
}

void
ElementClassT::compound_declaration_string(StringAccum &, const String &, const String &)
{
}


SynonymElementClassT::SynonymElementClassT(const String &name, ElementClassT *eclass)
  : _name(name), _eclass(eclass)
{
}

int
SynonymElementClassT::expand_into(RouterT *fromr, int which,
				  RouterT *tor, const RouterScope &scope,
				  ErrorHandler *)
{
  // get element type index. add new "anonymous" element type if needed
  int new_type = tor->get_type_index(_name, _eclass);
  ElementClassT *new_type_class = tor->type_class(new_type);
  if (_eclass != new_type_class)
    new_type = tor->get_anon_type_index(_name, _eclass);

  ElementT &e = fromr->element(which);
  if (fromr == tor) {
    e.type = new_type;
    return which;
  } else
    return tor->get_eindex(scope.prefix() + e.name, new_type,
			   scope.interpolate(e.configuration), e.landmark);
}

void
SynonymElementClassT::compound_declaration_string(StringAccum &sa, const String &name, const String &indent)
{
  sa << indent << "elementclass " << name << " " << _name << ";\n";
}
