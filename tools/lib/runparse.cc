// -*- c-basic-offset: 4 -*-
/*
 * runparse.cc -- unparse a tool router
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2009 Meraki, Inc.
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

#include "routert.hh"
#include "eclasst.hh"
#include <click/bitvector.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/variableenv.hh>
#include <stdio.h>

static void
add_line_directive(StringAccum &sa, const String &landmark)
{
    int colon = landmark.find_right(':');
    // XXX protect filename
    if (colon >= 0)
	sa << "# " << landmark.substring(colon + 1) << " \""
	   << landmark.substring(0, colon) << "\"\n";
    else
	sa << "# 0 \"" << landmark << "\"\n";
}


void
ElementClassT::unparse_declaration(StringAccum &sa, const String &, UnparseKind uk, ElementClassT *)
{
    if (uk == UNPARSE_OVERLOAD)
	sa << " ...\n";
}

void
SynonymElementClassT::unparse_declaration(StringAccum &sa, const String &indent, UnparseKind uk, ElementClassT *)
{
    if (uk == UNPARSE_OVERLOAD)
	sa << " ...\n";
    else if (uk == UNPARSE_NAMED)
	sa << indent << "elementclass " << name() << " " << _eclass->name() << ";\n";
}

void
RouterT::unparse_declaration(StringAccum &sa, const String &indent, UnparseKind uk, ElementClassT *stop)
{
    assert(!_circularity_flag && (name() || uk != UNPARSE_NAMED));

    // stop early: scope control
    if (stop == this) {
	if (uk == UNPARSE_OVERLOAD)
	    sa << " ...\n";
	return;
    }

    _circularity_flag = true;

    if (uk == UNPARSE_NAMED)
	sa << indent << "elementclass " << name() << " {";
    else if (uk == UNPARSE_ANONYMOUS)
	sa << '{';

    if (_overload_type) {
	_overload_type->unparse_declaration(sa, indent, UNPARSE_OVERLOAD, stop);
	sa << indent << "||";
    }

    // print formals
    for (int i = 0; i < _formals.size(); i++) {
	sa << (i ? ", " : " ");
	if (_formal_types[i])
	    sa << _formal_types[i] << ' ';
	sa << _formals[i];
    }
    if (_formals.size())
	sa << " |";
    sa << "\n";

    unparse(sa, indent + "  ");

    if (uk == UNPARSE_NAMED)
	sa << indent << "}\n";
    else if (uk == UNPARSE_ANONYMOUS)
	sa << indent << '}';

    _circularity_flag = false;
}


void
RouterT::unparse_requirements(StringAccum &sa, const String &indent) const
{
    if (_requirements.size() > 0) {
	sa << indent << "require(";
	for (int i = 0; i < _requirements.size(); i += 2) {
	    sa << (i ? ", " : "") << _requirements[i];
	    if (_requirements[i+1])
		sa << " " << cp_quote(_requirements[i+1]);
	}
	sa << ");\n\n";
    }
}

void
RouterT::unparse_defines(StringAccum &sa, const String &indent) const
{
    if (_scope.size()) {
	sa << indent << "define(";
	for (int i = 0; i < _scope.size(); i++) {
	    if (i > 0)
		sa << ", ";
	    sa << '$' << _scope.name(i) << ' ' << _scope.value(i);
	}
	sa << ");\n\n";
    }
}


#if 0

RouterUnparserT::RouterUnparserT(ErrorHandler *errh)
    : _tuid_map(-1), _relation(X_UNK), _errh(errh ? errh : ErrorHandler::silent_handler())
{
}

int RouterUnparseT::relation_negater[X_NUM] = {
    X_BAD, X_UNK, X_GT, X_GEQ, X_EQ, X_LEQ, X_LT
};

uint8_t RouterUnparseT::relation_combiner[X_NUM][X_NUM] = {
    //X_BAD, X_UNK, X_LT,  X_LEQ, X_EQ,  X_GEQ, X_GT
    { X_BAD, X_BAD, X_BAD, X_BAD, X_BAD, X_BAD, X_BAD }, // X_BAD
    { X_BAD, X_UNK, X_LT,  X_LEQ, X_EQ,  X_GEQ, X_GT  }, // X_UNK
    { X_BAD, X_LT,  X_LT,  X_LT,  X_BAD, X_BAD, X_BAD }, // X_LT
    { X_BAD, X_LEQ, X_LT,  X_LEQ, X_EQ,  X_EQ,  X_BAD }, // X_LEQ
    { X_BAD, X_EQ,  X_BAD, X_EQ,  X_EQ,  X_EQ,  X_BAD }, // X_EQ
    { X_BAD, X_GEQ, X_BAD, X_EQ,  X_EQ,  X_GEQ, X_GT  }, // X_GEQ
    { X_BAD, X_GT,  X_BAD, X_BAD, X_BAD, X_GT,  X_GT  }  // X_GT
};

int
RouterUnparseT::apply_relation(ElementClassT *a, ElementClassT *b, int new_relation)
{
    assert(new_relation >= X_LT && new_relation <= X_GT);
    if (a->uid() > b->uid()) {
	click_swap(a, b);
	new_relation = relation_negater[new_relation];
    }

    int *relation = _relation.findp(make_pair(a, b));
    int old_relation = *relation;
    *relation = relation_combiner[old_relation][new_relation];
    return (*relation != X_BAD || old_relation == X_BAD ? 0 : -1);
}

void
RouterUnparseT::collect_types()
{
    HashTable<int, ElementClassT *> class_map;
    collect_types(class_map);
    for (HashTable<int, ElementClassT *>::iterator i = class_map.begin(); i; i++) {
	_tuid_map.set(k.key(), _types.size());
	_types.push_back(k.value());
    }
}

void
RouterUnparseT::relate_types()
{
    // collect type relations
    for (Vector<ElementClassT *>::iterator i = _types.begin(); i != _types.end(); i++)
	if (RouterT *r = i->cast_router())
	    if (r->previous() && apply_relation(c, c->previous(), X_GEQ))
		_errh->lerror(c->landmark(), "circular type relationship involving %<%s%>", c->printable_name_c_str());
}

#else

void
RouterT::unparse_declarations(StringAccum &sa, const String &indent) const
{
    int nelements = _elements.size();
    int ntypes = _declared_types.size();
    check();

    // We may need to interleave element class declarations and element
    // declarations because of scope issues.

    // type_to_scope[] maps each type name to the latest scope in which it is
    // good.
    HashTable<ElementClassT *, int> type_to_scope(-2);
    for (int i = 0; i < ntypes; i++) {
	const ElementType &t = _declared_types[i];
	type_to_scope.set(t.type, _scope_cookie);
	if (t.prev_name >= 0) {
	    const ElementType &pt = _declared_types[t.prev_name];
	    type_to_scope.set(pt.type, pt.scope_cookie);
	}
    }
    // XXX FIXME
    //for (const_iterator e = begin_elements(); e; e++)
    //    assert(e->tunnel() || type_to_scope[e->type()] >= -1);

    // For each scope:
    // First print the element class declarations with that scope,
    // then print the elements whose classes are good only at that scope.
    int print_state = 0;
    for (int scope = -2; scope <= _scope_cookie; scope++) {
	for (Vector<ElementType>::const_iterator t = _declared_types.begin(); t != _declared_types.end(); t++)
	    if (t->scope_cookie == scope && t->name()
		&& !t->type->primitive()) {
		ElementClassT *stop_class = declared_type(t->name(), t->scope_cookie - 1);
		if (print_state == 2)
		    sa << "\n";
		t->type->unparse_declaration(sa, indent, ElementClassT::UNPARSE_NAMED, stop_class);
		print_state = 1;
	    }

	for (const_iterator e = begin_elements(); e; e++) {
	    if (e->dead() || e->tunnel()
		|| type_to_scope.get(e->type()) != scope)
		continue;
	    if (print_state == 1)
		sa << "\n";
	    add_line_directive(sa, e->landmark());
	    sa << indent << e->name() << " :: ";
	    if (e->type()->name())
		sa << e->type()->name();
	    else
		e->type()->unparse_declaration(sa, indent, ElementClassT::UNPARSE_ANONYMOUS, 0);
	    if (e->configuration())
		sa << "(" << e->configuration() << ")";
	    sa << ";\n";
	    print_state = 2;
	}
    }

    // print tunnel pairs
    for (int i = 0; i < nelements; i++)
	if (_elements[i]->tunnel() && _elements[i]->tunnel_output()) {
	    add_line_directive(sa, _elements[i]->landmark());
	    if (print_state > 0)
		sa << "\n";
	    sa << indent << "connectiontunnel " << _elements[i]->name()
	       << " -> " << _elements[i]->tunnel_output()->name() << ";\n";
	    print_state = 3;
	}
}
#endif

void
RouterT::unparse_connections(StringAccum &sa, const String &indent) const
{
    // collect connections
    Vector<ConnectionX *> conns;
    for (ConnectionX *c = _conn_head; c; c = c->_next[end_all])
	conns.push_back(c);
    int nc = conns.size();
    Bitvector used(nc, false);

    // prepare hookup chains
    Vector<int> next(nc, -1);
    Bitvector startchain(nc, true);
    for (int c = 0; c < nc; c++) {
	const PortT &ht = conns[c]->to();
	if (ht.port != 0 || used[c])
	    continue;
	int result = -1;
	for (int d = 0; d < nc; d++)
	    if (d != c && conns[d]->from() == ht && !used[d]) {
		result = d;
		if (conns[d]->to().port == 0)
		    break;
	    }
	if (result >= 0) {
	    next[c] = result;
	    startchain[result] = false;
	}
    }

    // count line numbers so we can give reasonable error messages
    if (nc) {
	int lineno = 1;
	const char *s = sa.data();
	int len = sa.length();
	for (int i = 0; i < len; i++)
	    if (s[i] == '\n')
		lineno++;
	sa << "# " << lineno + 1 << " \"\"\n";
    }

    // print hookup
    bool done = false;
    while (!done) {
	// print chains
	for (int c = 0; c < nc; c++) {
	    const PortT &hf = conns[c]->from();
	    if (used[c] || !startchain[c])
		continue;

	    sa << indent << hf.element->name();
	    if (hf.port)
		sa << " [" << hf.port << "]";

	    int d = c;
	    while (d >= 0 && !used[d]) {
		if (d == c)
		    sa << " -> ";
		else
		    sa << "\n" << indent << "    -> ";
		const PortT &ht = conns[d]->to();
		if (ht.port)
		    sa << "[" << ht.port << "] ";
		sa << ht.element->name();
		used[d] = true;
		d = next[d];
	    }

	    sa << ";\n";
	}

	// add new chains to include cycles
	done = true;
	for (int c = 0; c < nc && done; c++)
	    if (!used[c])
		startchain[c] = true, done = false;
    }
}

void
RouterT::unparse(StringAccum &sa, const String &indent) const
{
    unparse_requirements(sa, indent);
    unparse_defines(sa, indent);
    unparse_declarations(sa, indent);
    unparse_connections(sa, indent);
}

String
RouterT::configuration_string() const
{
    StringAccum sa;
    unparse(sa);
    return sa.take_string();
}
