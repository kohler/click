// -*- c-basic-offset: 4 -*-
/*
 * runparse.cc -- unparse a tool router
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
CompoundElementClassT::unparse_declaration(StringAccum &sa, const String &indent, UnparseKind uk, ElementClassT *stop)
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

    if (_prev) {
	_prev->unparse_declaration(sa, indent, UNPARSE_OVERLOAD, stop);
	sa << indent << "||";
    }

    // print formals
    for (int i = 0; i < _formals.size(); i++)
	sa << (i ? ", " : " ") << _formals[i];
    if (_formals.size())
	sa << " |";
    sa << "\n";

    _router->unparse(sa, indent + "  ");

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
	for (int i = 0; i < _requirements.size(); i++) {
	    if (i) sa << ", ";
	    sa << _requirements[i];
	}
	sa << ");\n\n";
    }
}

void
RouterT::unparse_declarations(StringAccum &sa, const String &indent) const
{
    int nelements = _elements.size();
    int ntypes = _etypes.size();
    check();

    // We may need to interleave element class declarations and element
    // declarations because of scope issues.

    // uid_to_scope[] maps each type name to the latest scope in which it is
    // good.
    HashMap<int, int> uid_to_scope(-2);
    for (int i = 0; i < ntypes; i++) {
	const ElementType &t = _etypes[i];
	uid_to_scope.insert(t.eclass->uid(), _scope_cookie);
	if (t.prev_name >= 0) {
	    const ElementType &pt = _etypes[t.prev_name];
	    uid_to_scope.insert(pt.eclass->uid(), pt.scope_cookie);
	}
    }
    // XXX FIXME
    //for (const_iterator e = begin_elements(); e; e++)
    //    assert(e->tunnel() || uid_to_scope[e->type_uid()] >= -1);

    // For each scope:
    // First print the element class declarations with that scope,
    // then print the elements whose classes are good only at that scope.
    int print_state = 0;
    for (int scope = -2; scope <= _scope_cookie; scope++) {
	for (int i = 0; i < ntypes; i++)
	    if (_etypes[i].scope_cookie == scope && _etypes[i].name()
		&& !_etypes[i].eclass->simple()) {
		ElementClassT *stop_class;
		if (_etypes[i].prev_name >= 0)
		    stop_class = _etypes[ _etypes[i].prev_name ].eclass;
		else {
		    stop_class = declared_type(_etypes[i].eclass->name(), -1);
		    if (!stop_class)
			stop_class = ElementClassT::default_class(_etypes[i].eclass->name());
		}
		if (print_state == 2)
		    sa << "\n";
		_etypes[i].eclass->unparse_declaration(sa, indent, ElementClassT::UNPARSE_NAMED, stop_class);
		print_state = 1;
	    }

	for (const_iterator e = begin_elements(); e; e++) {
	    if (e->dead() || e->tunnel()
		|| uid_to_scope[e->type_uid()] != scope)
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

void
RouterT::unparse_connections(StringAccum &sa, const String &indent) const
{
    // mark loser connections
    int nc = _conn.size();
    Bitvector used(nc, false);
    for (int c = 0; c < nc; c++)
	if (_conn[c].dead())
	    used[c] = true;

    // prepare hookup chains
    Vector<int> next(nc, -1);
    Bitvector startchain(nc, true);
    for (int c = 0; c < nc; c++) {
	const PortT &ht = _conn[c].to();
	if (ht.port != 0 || used[c])
	    continue;
	int result = -1;
	for (int d = 0; d < nc; d++)
	    if (d != c && _conn[d].from() == ht && !used[d]) {
		result = d;
		if (_conn[d].to().port == 0)
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
	    const PortT &hf = _conn[c].from();
	    if (used[c] || !startchain[c])
		continue;
	    
	    sa << indent << hf.elt->name();
	    if (hf.port)
		sa << " [" << hf.port << "]";

	    int d = c;
	    while (d >= 0 && !used[d]) {
		if (d == c)
		    sa << " -> ";
		else
		    sa << "\n" << indent << "    -> ";
		const PortT &ht = _conn[d].to();
		if (ht.port)
		    sa << "[" << ht.port << "] ";
		sa << ht.elt->name();
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
