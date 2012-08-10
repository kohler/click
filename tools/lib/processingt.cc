// -*- c-basic-offset: 4 -*-
/*
 * processingt.{cc,hh} -- decide on a Click configuration's processing
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2007 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
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

#include "processingt.hh"
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include "elementmap.hh"
#include <string.h>
#include <algorithm>

const char ProcessingT::processing_letters[] = "ahlXahlX";
const char ProcessingT::decorated_processing_letters[] = "ahlXaHLX";

static String dpcode_push("h");
static String dpcode_pull("l");
static String dpcode_agnostic("a");
static String dpcode_apush("H");
static String dpcode_apull("L");
static String dpcode_push_to_pull("h/l");

ProcessingT::ProcessingT(bool resolve_agnostics, RouterT *router,
			 ElementMap *emap, ErrorHandler *errh)
    : _router(router), _element_map(emap), _scope(router->scope()),
      _pidx_created(false)
{
    create("", resolve_agnostics, errh);
}

ProcessingT::ProcessingT(RouterT *router, ElementMap *emap, ErrorHandler *errh)
    : _router(router), _element_map(emap), _scope(router->scope()),
      _pidx_created(false)
{
    create("", true, errh);
}

ProcessingT::ProcessingT(const ProcessingT &processing, ElementT *element,
			 ErrorHandler *errh)
    : _element_map(processing._element_map), _scope(processing._scope),
      _pidx_created(false)
{
    assert(element->router() == processing._router);
    ElementClassT *t = element->resolve(processing._scope, &_scope, errh);
    _router = t->cast_router();
    assert(_router);
    if (!processing._router_name)
	_router_name = element->name();
    else
	_router_name = processing._router_name + "/" + element->name();

    String prefix = _router_name + "/";
    for (HashTable<String, String>::const_iterator it = processing._flow_overrides.begin();
	 it != processing._flow_overrides.end(); ++it)
	if (it.key().starts_with(prefix))
	    _flow_overrides.set(it.key().substring(prefix.length()), it.value());

    create(processing.processing_code(element), true, errh);
}

void
ProcessingT::check_types(ErrorHandler *errh)
{
    Vector<ElementClassT *> types;
    _router->collect_locally_declared_types(types);
    for (Vector<ElementClassT *>::iterator ti = types.begin(); ti != types.end(); ++ti)
	if (RouterT *rx = (*ti)->cast_router()) {
	    ProcessingT subp(rx, _element_map, errh);
	    subp.check_types(errh);
	}
}

void
ProcessingT::create(const String &compound_pcode, bool resolve_agnostics,
		    ErrorHandler *errh)
{
    LocalErrorHandler lerrh(errh);
    ElementMap::push_default(_element_map);

    // create pidx and elt arrays, warn about dead elements
    create_pidx(&lerrh);
    // collect connections
    Vector<ConnectionT> conn;
    for (RouterT::conn_iterator it = _router->begin_connections(); it; ++it)
	conn.push_back(*it);
    Bitvector invalid_conn(conn.size(), false);
    // do processing
    initial_processing(compound_pcode, &lerrh);
    check_processing(conn, invalid_conn, &lerrh);
    // change remaining agnostic ports to agnostic-push
    if (resolve_agnostics)
	this->resolve_agnostics();
    check_connections(conn, invalid_conn, &lerrh);

    ElementMap::pop_default();
}

void
ProcessingT::parse_flow_info(ElementT *e, ErrorHandler *)
{
    Vector<String> conf;
    cp_argvec(cp_expand(e->configuration(), _scope), conf);
    for (String *it = conf.begin(); it != conf.end(); ++it) {
	String name = cp_shift_spacevec(*it);
	String value = cp_shift_spacevec(*it);
	if (name && value && !*it)
	    _flow_overrides.set(name, value);
    }
}

void
ProcessingT::create_pidx(ErrorHandler *errh)
{
    int ne = _router->nelements();
    _pidx[end_to].assign(ne, 0);
    _pidx[end_from].assign(ne, 0);
    ElementClassT *flow_info = ElementClassT::base_type("FlowInfo");

    // count used input and output ports for each element
    int ci = 0, co = 0;
    for (int i = 0; i < ne; i++) {
	_pidx[end_to][i] = ci;
	_pidx[end_from][i] = co;
	ElementT *e = _router->element(i);
	ci += e->ninputs();
	co += e->noutputs();
	if (e->resolved_type(_scope) == flow_info)
	    parse_flow_info(e, errh);
    }
    _pidx[end_to].push_back(ci);
    _pidx[end_from].push_back(co);

    // create eidxes
    _elt[end_to].clear();
    _elt[end_from].clear();
    ci = 0, co = 0;
    for (int i = 1; i <= ne; i++) {
	const ElementT *e = _router->element(i - 1);
	for (; ci < _pidx[end_to][i]; ci++)
	    _elt[end_to].push_back(e);
	for (; co < _pidx[end_from][i]; co++)
	    _elt[end_from].push_back(e);
    }

    // complain about dead elements with live connections
    if (errh) {
	for (RouterT::const_iterator x = _router->begin_elements(); x; x++)
	    if (x->dead() && (x->ninputs() > 0 || x->noutputs() > 0))
		errh->lwarning(x->decorated_landmark(), "dead element %s has live connections", x->name_c_str());
    }

    // mark created
    _pidx_created = true;
}

const char *
ProcessingT::processing_code_next(const char *code, const char *end_code, int &processing)
{
    assert(code <= end_code);
    if (code == end_code) {
	processing = -1;
	return code;
    } else if (*code == 'h')
	processing = ppush;
    else if (*code == 'l')
	processing = ppull;
    else if (*code == 'a')
	processing = pagnostic;
    else if (*code == 'H')
	processing = ppush + pagnostic;
    else if (*code == 'L')
	processing = ppull + pagnostic;
    else {
	processing = -1;
	return code;
    }
    const char *nc = code + 1;
    if (nc != end_code && *nc == '@') {
	processing += perror;
	++nc;
    }
    if (nc == end_code || *nc == '/')
	return code;
    else
	return nc;
}

String
ProcessingT::processing_code_reverse(const String &s)
{
    const char *slash = find(s.begin(), s.end(), '/');
    if (slash != s.end())
	return s.substring(slash + 1, s.end()) + "/" + s.substring(s.begin(), slash);
    else
	return s;
}

void
ProcessingT::initial_processing_for(int ei, const String &compound_pcode, ErrorHandler *errh)
{
    // don't handle uprefs or tunnels
    const ElementT *e = _router->element(ei);
    // resolved_type() errors reported in check_nports(), do not pass errh here
    ElementClassT *etype = e->resolved_type(_scope);
    if (!etype)
	return;

    // fetch initial processing code
    String pc;
    if (e->tunnel() && (e->name() == "input" || e->name() == "output"))
	pc = compound_pcode;
    else
	pc = etype->traits().processing_code;
    if (!pc) {
	int &class_warning = _class_warnings[etype];
	if (!e->tunnel() && !(class_warning & classwarn_unknown)) {
	    class_warning |= classwarn_unknown;
	    errh->lwarning(e->decorated_landmark(), "unknown element class %<%s%>", etype->printable_name_c_str());
	}
	return;
    }

    // parse processing code
    const char *pcpos = pc.begin();

    int start_in = _pidx[end_to][ei];
    int start_out = _pidx[end_from][ei];

    int val;
    for (int i = 0; i < e->ninputs(); i++) {
	pcpos = processing_code_next(pcpos, pc.end(), val);
	if (val < 0) {
	    int &cwarn = _class_warnings[etype];
	    if (!(cwarn & classwarn_pcode)) {
		// "String(pc).c_str()" so pcpos remains valid
		errh->lerror(e->landmark(), "syntax error in processing code %<%s%> for %<%s%>", String(pc).c_str(), etype->printable_name_c_str());
		cwarn |= classwarn_pcode;
	    }
	    val = pagnostic;
	}
	_processing[end_to][start_in + i] = (val & pagnostic ? pagnostic : val);
    }

    pcpos = processing_code_output(pc.begin(), pc.end(), pcpos);

    for (int i = 0; i < e->noutputs(); i++) {
	pcpos = processing_code_next(pcpos, pc.end(), val);
	if (val < 0) {
	    int &cwarn = _class_warnings[etype];
	    if (!(cwarn & classwarn_pcode)) {
		// "String(pc).c_str()" so pcpos remains valid
		errh->lerror(e->landmark(), "syntax error in processing code %<%s%> for %<%s%>", String(pc).c_str(), etype->printable_name_c_str());
		cwarn |= classwarn_pcode;
	    }
	    val = pagnostic;
	}
	_processing[end_from][start_out + i] = (val & pagnostic ? pagnostic : val);
    }
}

void
ProcessingT::initial_processing(const String &compound_pcode, ErrorHandler *errh)
{
    _processing[end_to].assign(ninput_pidx(), pagnostic);
    _processing[end_from].assign(noutput_pidx(), pagnostic);
    String reversed_pcode = processing_code_reverse(compound_pcode);
    for (int i = 0; i < nelements(); i++)
	initial_processing_for(i, reversed_pcode, errh);
}

void
ProcessingT::processing_error(const ConnectionT &conn, int processing_from,
			      ErrorHandler *errh)
{
  const char *type1 = (processing_from & ppush ? "push" : "pull");
  const char *type2 = (processing_from & ppush ? "pull" : "push");
  if (conn.landmark() == "<agnostic>")
    errh->lerror(conn.from_element()->decorated_landmark(),
		 "agnostic %<%s%> in mixed context: %s input %d, %s output %d",
		 conn.from_element()->name_c_str(), type2, conn.to_port(),
		 type1, conn.from_port());
  else
    errh->lerror(conn.decorated_landmark(),
		 "%<%s%> %s output %d connected to %<%s%> %s input %d",
		 conn.from_element()->name_c_str(), type1, conn.from_port(),
		 conn.to_element()->name_c_str(), type2, conn.to_port());
  _processing[end_to][input_pidx(conn)] |= perror;
  _processing[end_from][output_pidx(conn)] |= perror;
}

void
ProcessingT::check_processing(Vector<ConnectionT> &conn, Bitvector &invalid_conn, ErrorHandler *errh)
{
    // add fake connections for agnostics
    int old_size = conn.size();
    LandmarkT agnostic_landmark("<agnostic>");
    Bitvector bv;
    for (int i = 0; i < ninput_pidx(); i++)
	if (_processing[end_to][i] == pagnostic) {
	    ElementT *e = const_cast<ElementT *>(_elt[end_to][i]);
	    int ei = e->eindex();
	    int port = i - _pidx[end_to][ei];
	    int opidx = _pidx[end_from][ei];
	    int noutputs = _pidx[end_from][ei+1] - opidx;
	    forward_flow(flow_code(e), port, &bv, noutputs, errh);
	    for (int j = 0; j < noutputs; j++)
		if (bv[j] && _processing[end_from][opidx + j] == pagnostic)
		    conn.push_back(ConnectionT(PortT(e, j), PortT(e, port), agnostic_landmark));
	}
    invalid_conn.resize(conn.size());

    // spread personalities
    while (true) {
	bool changed = false;
	for (int c = 0; c < conn.size(); c++) {
	    if (invalid_conn[c])
		continue;

	    int offf = output_pidx(conn[c].from());
	    int offt = input_pidx(conn[c].to());
	    int pf = _processing[end_from][offf];
	    int pt = _processing[end_to][offt];

	    switch (pt & 7) {

	      case pagnostic:
		if (pf != pagnostic) {
		    _processing[end_to][offt] = pagnostic | (pf & 3);
		    changed = true;
		}
		break;

	      case ppush:
	      case ppull:
	      case ppush + pagnostic:
	      case ppull + pagnostic:
		if (pf == pagnostic) {
		    _processing[end_from][offf] = pagnostic | (pt & 3);
		    changed = true;
		} else if (((pf ^ pt) & 3) != 0) {
		    processing_error(conn[c], pf, errh);
		    invalid_conn[c] = true;
		}
		break;

	      default:
		assert(0);

	    }
	}

	if (!changed)
	    break;
    }

    conn.resize(old_size);
    invalid_conn.resize(old_size);
}

static const char *
processing_name(int p)
{
    p &= 7;
    if (p == ProcessingT::pagnostic)
	return "agnostic";
    else if (p & ProcessingT::ppush)
	return "push";
    else if (p & ProcessingT::ppull)
	return "pull";
    else
	return "?";
}

static int
notify_nports_pair(const char *&s, const char *ends, int &lo, int &hi)
{
    if (s == ends || *s == '-')
	lo = 0;
    else if (isdigit((unsigned char) *s))
	s = cp_integer(s, ends, 10, &lo);
    else
	return -1;
    if (s < ends && *s == '-') {
	s++;
	if (s < ends && isdigit((unsigned char) *s))
	    s = cp_integer(s, ends, 10, &hi);
	else
	    hi = INT_MAX;
    } else
	hi = lo;
    return 0;
}

void
ProcessingT::check_nports(Vector<ConnectionT> &conn, const ElementT *e,
			  const int *input_used, const int *output_used, ErrorHandler *errh)
{
    String port_count = e->resolved_type(_scope, errh)->port_count_code();
    const char *s_in = port_count.c_str();
    const char *s = s_in, *ends = s + port_count.length();
    int ninputs, ninlo, ninhi, noutlo, nouthi, equal = 0;
    StringAccum equalmsg;

    if (s == ends)		// no information about element; assume OK
	return;

    if (notify_nports_pair(s, ends, ninlo, ninhi) < 0)
	goto parse_error;

    if (s == ends)
	s = s_in;
    else if (*s == '/')
	s++;
    else
	goto parse_error;

    if (*s == '=') {
	const char *plus = s + 1;
	do {
	    equal++;
	} while (plus != ends && *plus++ == '+');
	if (plus != ends)
	    equal = 0;
    }
    if (!equal)
	if (notify_nports_pair(s, ends, noutlo, nouthi) < 0 || s != ends)
	    goto parse_error;

    ninputs = e->ninputs();
    if (ninputs < ninlo) {
	errh->lerror(e->decorated_landmark(), "too few inputs for %<%s%>, %s%d required", e->name_c_str(), (ninlo == ninhi ? "" : "at least "), ninlo);
	ninputs = ninlo;
    } else if (ninputs > ninhi) {
	errh->lerror(e->decorated_landmark(), "too many inputs for %<%s%>, %s%d allowed", e->name_c_str(), (ninlo == ninhi ? "" : "at most "), ninhi);
	for (int i = ninhi; i < e->ninputs(); i++)
	    if (input_used[i] >= 0)
		errh->lmessage(conn[input_used[i]].decorated_landmark(),
			       "%<%s%> input %d used here", e->name_c_str(), i);
	ninputs = ninhi;
    }

    if (equal) {
	noutlo = nouthi = ninputs + equal - 1;
	if (e->noutputs() != noutlo)
	    equalmsg << " with " << ninputs << " input" << (ninputs == 1 ? "" : "s");
    }
    if (e->noutputs() < noutlo)
	errh->lerror(e->decorated_landmark(), "too few outputs for %<%s%>%s, %s%d required", e->name_c_str(), equalmsg.c_str(), (noutlo == nouthi ? "" : "at least "), noutlo);
    else if (e->noutputs() > nouthi) {
	errh->lerror(e->decorated_landmark(), "too many outputs for %<%s%>%s, %s%d allowed", e->name_c_str(), equalmsg.c_str(), (noutlo == nouthi ? "" : "at most "), nouthi);
	for (int i = nouthi; i < e->noutputs(); i++)
	    if (output_used[i] >= 0)
		errh->lmessage(conn[output_used[i]].decorated_landmark(),
			       "%<%s%> output %d used here", e->name_c_str(), i);
    }

    return;

  parse_error:
    errh->lerror(e->decorated_landmark(), "syntax error in port count code for %<%s%>", e->type()->printable_name_c_str());
}

void
ProcessingT::check_connections(Vector<ConnectionT> &conn, Bitvector &invalid_conn, ErrorHandler *errh)
{
    Vector<int> input_used(ninput_pidx(), -1);
    Vector<int> output_used(noutput_pidx(), -1);

    // Check each hookup to ensure it doesn't reuse a port
    for (int c = 0; c < conn.size(); c++) {
	const PortT &hf = conn[c].from(), &ht = conn[c].to();
	int fp = output_pidx(hf), tp = input_pidx(ht);

	if ((_processing[end_from][fp] & ppush) && output_used[fp] >= 0
	    && conn[output_used[fp]] != ht && !invalid_conn[c]) {
	    errh->lerror(conn[c].decorated_landmark(),
			 "illegal reuse of %<%s%> push output %d",
			 hf.element->name_c_str(), hf.port);
	    errh->lmessage(conn[output_used[fp]].decorated_landmark(),
			   "%<%s%> output %d previously used here",
			   hf.element->name_c_str(), hf.port);
	    _processing[end_from][fp] |= perror;
	} else
	    output_used[fp] = c;

	if ((_processing[end_to][tp] & ppull) && input_used[tp] >= 0
	    && conn[input_used[tp]] != hf && !invalid_conn[c]) {
	    errh->lerror(conn[c].decorated_landmark(),
			 "illegal reuse of %<%s%> pull input %d",
			 ht.element->name_c_str(), ht.port);
	    errh->lmessage(conn[input_used[tp]].decorated_landmark(),
			   "%<%s%> input %d previously used here",
			   ht.element->name_c_str(), ht.port);
	    _processing[end_to][tp] |= perror;
	} else
	    input_used[tp] = c;
    }

    // Check for unused inputs and outputs.
    for (int ei = 0; ei < _router->nelements(); ei++) {
	const ElementT *e = _router->element(ei);
	if (e->dead())
	    continue;
	int ipdx = _pidx[end_to][ei], opdx = _pidx[end_from][ei];
	check_nports(conn, e, input_used.begin() + ipdx, output_used.begin() + opdx, errh);
	for (int i = 0; i < e->ninputs(); i++)
	    if (input_used[ipdx + i] < 0) {
		errh->lerror(e->decorated_landmark(),
			     "%<%s%> %s input %d not connected",
			     e->name_c_str(), processing_name(_processing[end_to][ipdx + i]), i);
		_processing[end_to][ipdx + i] |= perror;
	    }
	for (int i = 0; i < e->noutputs(); i++)
	    if (output_used[opdx + i] < 0) {
		errh->lerror(e->decorated_landmark(),
			     "%<%s%> %s output %d not connected",
			     e->name_c_str(), processing_name(_processing[end_from][opdx + i]), i);
		_processing[end_from][opdx + i] |= perror;
	    }
    }
}

void
ProcessingT::resolve_agnostics()
{
    for (int p = 0; p < 2; ++p)
	for (int *it = _processing[p].begin(); it != _processing[p].end(); ++it)
	    if ((*it & 7) == pagnostic)
		*it += ppush;
}

bool
ProcessingT::same_processing(int a, int b) const
{
    // NB ppush != pagnostic+ppush; this is ok
    int ai = _pidx[end_to][a], bi = _pidx[end_to][b];
    int ao = _pidx[end_from][a], bo = _pidx[end_from][b];
    int ani = _pidx[end_to][a+1] - ai, bni = _pidx[end_to][b+1] - bi;
    int ano = _pidx[end_from][a+1] - ao, bno = _pidx[end_from][b+1] - bo;
    if (ani != bni || ano != bno)
	return false;
    if (ani && memcmp(&_processing[end_to][ai], &_processing[end_to][bi], sizeof(int) * ani) != 0)
	return false;
    if (ano && memcmp(&_processing[end_from][ao], &_processing[end_from][bo], sizeof(int) * ano) != 0)
	return false;
    return true;
}

String
ProcessingT::processing_code(const ElementT *e) const
{
    assert(e->router() == _router);
    int ei = e->eindex();
    StringAccum sa;
    for (int i = _pidx[end_to][ei]; i < _pidx[end_to][ei+1]; i++)
	sa << processing_letters[_processing[end_to][i] & 7];
    sa << '/';
    for (int i = _pidx[end_from][ei]; i < _pidx[end_from][ei+1]; i++)
	sa << processing_letters[_processing[end_from][i] & 7];
    return sa.take_string();
}

String
ProcessingT::decorated_processing_code(const ElementT *e) const
{
    assert(e->router() == _router);
    int ei = e->eindex();
    int ipb = _pidx[end_to][ei], ipe = _pidx[end_to][ei+1];
    int opb = _pidx[end_from][ei], ope = _pidx[end_from][ei+1];

    // avoid memory allocation by returning an existing string
    // (premature optimization?)
    int pin = (ipb == ipe ? ppush : _processing[end_to][ipb]);
    int pout = (opb == ope ? ppush : _processing[end_from][opb]);
    for (int i = ipb + 1; i < ipe; ++i)
	if (_processing[end_to][i] != pin)
	    goto create_code;
    for (int i = opb + 1; i < ope; ++i)
	if (_processing[end_from][i] != pout)
	    goto create_code;
    if ((pin & perror) == 0 && (pout & perror) == 0) {
	if (pin == ppush && pout == ppush)
	    return dpcode_push;
	else if (pin == ppull && pout == ppull)
	    return dpcode_pull;
	else if (pin == pagnostic && pout == pagnostic)
	    return dpcode_agnostic;
	else if (pin == ppush + pagnostic && pout == ppush + pagnostic)
	    return dpcode_apush;
	else if (pin == ppull + pagnostic && pout == ppull + pagnostic)
	    return dpcode_apull;
	else if (pin == ppush && pout == ppull)
	    return dpcode_push_to_pull;
    }

    // no optimization possible; just return the whole code
  create_code:
    StringAccum sa;
    for (const int *it = _processing[end_to].begin() + ipb;
	 it != _processing[end_to].begin() + ipe; ++it) {
	sa << decorated_processing_letters[*it & 7];
	if (*it & perror)
	    sa << '@';
    }
    sa << '/';
    for (const int *it = _processing[end_from].begin() + opb;
	 it != _processing[end_from].begin() + ope; ++it) {
	sa << decorated_processing_letters[*it & 7];
	if (*it & perror)
	    sa << '@';
    }
    return sa.take_string();
}

// FLOW CODES

static void
skip_flow_code(const char *&p, const char *last)
{
    if (p != last && *p != '/') {
	if (*p == '[') {
	    for (p++; p != last && *p != ']'; p++)
		/* nada */;
	    if (p != last)
		p++;
	} else
	    p++;
    }
}

static int
next_flow_code(const char *&p, const char *last,
	       int port, Bitvector &code, ErrorHandler *errh)
{
    if (p == last || *p == '/') {
	// back up to last code character
	if (p[-1] == ']') {
	    for (p -= 2; *p != '['; p--)
		/* nada */;
	} else
	    p--;
    }

    code.assign(256, false);

    if (*p == '[') {
	bool negated = false;
	if (p[1] == '^')
	    negated = true, p++;
	for (p++; p != last && *p != ']'; p++) {
	    // avoid isalpha() to avoid locale/"signed char" dependencies
	    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
		code[*p] = true;
	    else if (*p == '#')
		code[port + 128] = true;
	    else if (errh)
		errh->error("flow code: invalid character %<%c%>", *p);
	}
	if (negated)
	    code.negate();
	if (p == last) {
	    if (errh)
		errh->error("flow code: missing %<]%>");
	    p--;		// don't skip over final '\0'
	}
    } else if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
	code[*p] = true;
    else if (*p == '#')
	code[port + 128] = true;
    else {
	if (errh)
	    errh->error("flow code: invalid character %<%c%>", *p);
	p++;
	return -1;
    }

    p++;
    return 0;
}

int
ProcessingT::code_flow(const String &flow_code, int port, bool isoutput,
		       Bitvector *bv, int size, ErrorHandler *errh)
{
    if (port < 0 || size == 0) {
	bv->assign(size, false);
	return 0;
    } else if (!flow_code || flow_code.equals("x/x", 3)) {
	bv->assign(size, true);
	return 0;
    } else
	bv->assign(size, false);

    const char *fbegin[2], *fend[2], *slash = find(flow_code, '/');
    fbegin[end_to] = flow_code.begin();
    if (slash == flow_code.end()) {
	fbegin[end_from] = fbegin[end_to];
	fend[end_to] = fend[end_from] = flow_code.end();
    } else if (slash + 1 == flow_code.end() || slash[1] == '/')
	return (errh ? errh->error("flow code: bad %</%>") : -1);
    else {
	fend[end_to] = slash;
	fbegin[end_from] = slash + 1;
	fend[end_from] = flow_code.end();
    }

    Bitvector source_code, sink_code;

    for (int i = 0; i < port; ++i)
	skip_flow_code(fbegin[isoutput], fend[isoutput]);
    next_flow_code(fbegin[isoutput], fend[isoutput], port, source_code, errh);

    for (int i = 0; i < size; i++) {
	next_flow_code(fbegin[!isoutput], fend[!isoutput], i, sink_code, errh);
	if (source_code.nonzero_intersection(sink_code))
	    (*bv)[i] = true;
    }

    return 0;
}

void
ProcessingT::debug_print_pidxes(const Bitvector &ports, bool isoutput,
				const String &prefix, ErrorHandler *debug_errh) const
{
    if (debug_errh) {
	assert(ports.size() == npidx(isoutput));
	StringAccum sa;
	for (int i = 0; i < npidx(isoutput); i++)
	    if (ports[i]) {
		if (sa)
		    sa << ' ';
		sa << port(i, isoutput).unparse(isoutput);
	    }
	if (prefix && !sa)
	    sa << "(none)";
	if (prefix)
	    debug_errh->message("%s%s", prefix.c_str(), sa.c_str());
	else if (sa)
	    debug_errh->message("%s", sa.c_str());
    }
}

void
ProcessingT::follow_connections(const Bitvector &source, bool source_isoutput,
				Bitvector &sink) const
{
    assert(source.size() == npidx(source_isoutput)
	   && sink.size() == npidx(!source_isoutput));
    for (RouterT::conn_iterator it = _router->begin_connections();
	 it != _router->end_connections(); ++it)
	if (source[pidx(*it, source_isoutput)])
	    sink[pidx(*it, !source_isoutput)] = true;
}

void
ProcessingT::follow_connections(const PortT &source, bool source_isoutput,
				Bitvector &sink) const
{
    assert(sink.size() == npidx(!source_isoutput));
    for (RouterT::conn_iterator it = _router->find_connections_touching(source, source_isoutput);
	 it != _router->end_connections(); ++it)
	sink[pidx(*it, !source_isoutput)] = true;
}

void
ProcessingT::follow_flow(const Bitvector &source, bool source_isoutput,
			 Bitvector &sink, ErrorHandler *errh) const
{
    assert(source.size() == npidx(source_isoutput)
	   && sink.size() == npidx(!source_isoutput));
    Bitvector bv;
    // for speed with sparse Bitvectors, look into the Bitvector implementation
    const Bitvector::word_type *source_words = source.words();
    const int wb = Bitvector::wbits;
    for (int w = 0; w < source.word_size(); ++w)
	if (source_words[w]) {
	    int m = std::min(source.size(), (w + 1) * wb);
	    for (int pidx = w * wb; pidx < m; ++pidx)
		if (source[pidx]) {
		    PortT p = port(pidx, source_isoutput);
		    (void) port_flow(p, source_isoutput, &bv, errh);
		    sink.offset_or(bv, _pidx[!source_isoutput][p.eindex()]);
		}
	}
}

void
ProcessingT::follow_reachable(Bitvector &ports, bool isoutput, bool forward, ErrorHandler *errh, ErrorHandler *debug_errh) const
{
    assert(ports.size() == npidx(isoutput));
    Bitvector other_ports(npidx(!isoutput), false);
    Bitvector new_ports(npidx(isoutput), false);
    Bitvector diff(ports);
    for (int round = 0; true; ++round) {
	if (debug_errh) {
	    debug_print_pidxes(diff, isoutput, (round ? "round " + String(round) : "initial") + String(": "), debug_errh);
	    other_ports.assign(npidx(!isoutput), false);
	}
	if (isoutput != forward) {
	    follow_flow(diff, isoutput, other_ports, errh);
	    follow_connections(other_ports, !isoutput, new_ports);
	} else {
	    follow_connections(diff, isoutput, other_ports);
	    follow_flow(other_ports, !isoutput, new_ports, errh);
	}
	ports.or_with_difference(new_ports, diff);
	if (!diff)
	    return;
    }
}

String
ProcessingT::compound_port_count_code() const
{
    ElementT *input = _router->element("input");
    ElementT *output = _router->element("output");
    assert(input && output && input->tunnel() && output->tunnel());
    if (input->noutputs() == 0 && output->ninputs() == 0)
	return String::make_stable("0/0", 3);
    else
	return String(input->noutputs()) + "/" + String(output->ninputs());
}

String
ProcessingT::compound_processing_code() const
{
    assert(_pidx_created);
    ElementT *input = _router->element("input");
    ElementT *output = _router->element("output");
    assert(input && output && input->tunnel() && output->tunnel());

    // read input and output codes
    StringAccum icode, ocode;
    for (int i = 0; i < input->noutputs(); i++) {
	int p = output_processing(PortT(input, i));
	icode << processing_letters[p];
    }
    for (int i = 0; i < output->ninputs(); i++) {
	int p = input_processing(PortT(output, i));
	ocode << processing_letters[p];
    }

    // streamline codes, ensure at least one character per half
    while (icode.length() > 1 && icode[icode.length() - 2] == icode.back())
	icode.pop_back();
    while (ocode.length() > 1 && ocode[ocode.length() - 2] == ocode.back())
	ocode.pop_back();
    if (!icode.length())
	icode << 'a';
    if (!ocode.length())
	ocode << 'a';

    icode << '/' << ocode;
    return icode.take_string();
}

String
ProcessingT::compound_flow_code(ErrorHandler *errh) const
{
    assert(_pidx_created);
    ElementT *input = _router->element("input");
    ElementT *output = _router->element("output");
    assert(input && output && input->tunnel() && output->tunnel());

    // skip calculation in common case
    int ninputs = input->noutputs(), noutputs = output->ninputs();
    if (ninputs == 0 || noutputs == 0)
	return "x/y";

    // read flow codes, create 'codes' array
    Bitvector *codes = new Bitvector[noutputs];
    for (int i = 0; i < noutputs; i++)
	codes[i].assign(ninputs, false);
    Bitvector input_vec(ninput_pidx(), false);
    int opidx = input_pidx(PortT(output, 0));
    for (int i = 0; i < ninputs; i++) {
	if (i)
	    input_vec.clear();
	follow_connections(PortT(input, i), true, input_vec);
	follow_reachable(input_vec, false, true, errh);
	for (int p = 0; p < noutputs; p++)
	    if (input_vec[opidx + p])
		codes[p][i] = true;
    }

    // combine flow codes
    Vector<int> codeid;
    const char *cur_code = "xyzabcdefghijklmnopqrstuvwXYZABCDEFGHIJKLMNOPQRSTUVW0123456789_";
    codeid.push_back(*cur_code++);
    for (int i = 1; i < ninputs; i++) {

	// look for flow codes common among all outputs with this code, and
	// flow codes present in any output without this code
	Bitvector common(ninputs, true);
	Bitvector disjoint(ninputs, false);
	int found = 0;
	for (int j = 0; j < noutputs; j++)
	    if (codes[j][i]) {
		common &= codes[j];
		found++;
	    } else
		disjoint |= codes[j];
	disjoint.negate();

	common &= disjoint;
	for (int j = 0; j < i; j++)
	    if (common[j]) {
		int codeid_j = codeid[j]; // ugh, passing a ref that disappears
		codeid.push_back(codeid_j);
		// turn off reference
		for (int k = 0; k < noutputs; k++)
		    codes[k][i] = false;
		goto found;
	    }
	assert(*cur_code);
	codeid.push_back(*cur_code++);

      found: ;
    }

    // generate flow code
    assert(*cur_code);
    StringAccum sa;
    for (int i = 0; i < ninputs; i++)
	sa << (char)codeid[i];
    sa << '/';
    for (int i = 0; i < noutputs; i++)
	if (!codes[i])
	    sa << *cur_code;
	else {
	    int pos = sa.length();
	    sa << '[';
	    for (int j = 0; j < ninputs; j++)
		if (codes[i][j])
		    sa << (char)codeid[j];
	    if (sa.length() == pos + 2) {
		sa[pos] = sa[pos + 1];
		sa.pop_back();
	    } else
		sa << ']';
	}

    // return
    delete[] codes;
    return sa.take_string();
}
