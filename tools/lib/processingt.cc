// -*- c-basic-offset: 4 -*-
/*
 * processingt.{cc,hh} -- decide on a Click configuration's processing
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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

#include "processingt.hh"
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include "elementmap.hh"
#include <ctype.h>
#include <string.h>

const char * const ProcessingT::processing_letters = "ahl";

ProcessingT::ProcessingT()
    : _router(0)
{
}

ProcessingT::ProcessingT(const RouterT *r, ErrorHandler *errh)
    : _router(0)
{
    reset(r, ElementMap::default_map(), false, errh);
}

ProcessingT::ProcessingT(const RouterT *r, ElementMap *em, ErrorHandler *errh)
    : _router(0)
{
    reset(r, em, false, errh);
}

ProcessingT::ProcessingT(const RouterT *r, ElementMap *em, bool flatten,
			 ErrorHandler *errh)
    : _router(0)
{
    reset(r, em, flatten, errh);
}

void
ProcessingT::create_pidx(ErrorHandler *errh)
{
    int ne = _router->nelements();
    _input_pidx.assign(ne, 0);
    _output_pidx.assign(ne, 0);

    // count used input and output ports for each element
    int ci = 0, co = 0;
    for (int i = 0; i < ne; i++) {
	_input_pidx[i] = ci;
	_output_pidx[i] = co;
	ci += _router->element(i)->ninputs();
	co += _router->element(i)->noutputs();
    }
    _input_pidx.push_back(ci);
    _output_pidx.push_back(co);

    // create eidxes
    _input_elt.clear();
    _output_elt.clear();
    ci = 0, co = 0;
    for (int i = 1; i <= ne; i++) {
	const ElementT *e = _router->element(i - 1);
	for (; ci < _input_pidx[i]; ci++)
	    _input_elt.push_back(e);
	for (; co < _output_pidx[i]; co++)
	    _output_elt.push_back(e);
    }

    // complain about dead elements with live connections
    if (errh) {
	for (RouterT::const_iterator x = _router->begin_elements(); x; x++)
	    if (x->dead() && (x->ninputs() > 0 || x->noutputs() > 0))
		errh->lwarning(x->landmark(), "dead element %s has live connections", x->name_cc());
    }
}

static int
next_processing_code(const String &str, int &pos, ErrorHandler *errh,
		     const String &landmark, ElementClassT *etype)
{
    const char *s = str.data();
    int len = str.length();
    if (pos >= len)
	return -2;

    switch (s[pos]) {

      case 'h': case 'H':
	pos++;
	return ProcessingT::VPUSH;
    
      case 'l': case 'L':
	pos++;
	return ProcessingT::VPULL;
	
      case 'a': case 'A':
	pos++;
	return ProcessingT::VAGNOSTIC;

      case '/':
	return -2;

      default:
	errh->lerror(landmark, "bad character `%c' in processing code for `%s'", s[pos], etype->printable_name_cc());
	pos++;
	return -1;
    
    }
}

void
ProcessingT::initial_processing_for(int ei, ErrorHandler *errh)
{
    // don't handle uprefs or tunnels
    const ElementT *e = _router->element(ei);
    ElementClassT *etype = e->type();
    if (!etype || etype == ElementClassT::tunnel_type())
	return;

    String landmark = e->landmark();
    String pc = etype->traits().processing_code();
    if (!pc) {
	errh->lwarning(landmark, "`%s' has no processing code; assuming agnostic", etype->printable_name_cc());
	return;
    }

    int pos = 0;
    int len = pc.length();

    int start_in = _input_pidx[ei];
    int start_out = _output_pidx[ei];

    int val = 0;
    int last_val = 0;
    for (int i = 0; i < e->ninputs(); i++) {
	if (last_val >= 0)
	    last_val = next_processing_code(pc, pos, errh, landmark, etype);
	if (last_val >= 0)
	    val = last_val;
	_input_processing[start_in + i] = val;
    }

    while (pos < len && pc[pos] != '/')
	pos++;
    if (pos >= len)
	pos = 0;
    else
	pos++;

    last_val = 0;
    for (int i = 0; i < e->noutputs(); i++) {
	if (last_val >= 0)
	    last_val = next_processing_code(pc, pos, errh, landmark, etype);
	if (last_val >= 0)
	    val = last_val;
	_output_processing[start_out + i] = val;
    }
}

void
ProcessingT::initial_processing(ErrorHandler *errh)
{
    _input_processing.assign(ninput_pidx(), VAGNOSTIC);
    _output_processing.assign(noutput_pidx(), VAGNOSTIC);
    for (int i = 0; i < nelements(); i++)
	initial_processing_for(i, errh);
}

void
ProcessingT::processing_error(const ConnectionT &conn, int processing_from,
			      ErrorHandler *errh)
{
  const char *type1 = (processing_from == VPUSH ? "push" : "pull");
  const char *type2 = (processing_from == VPUSH ? "pull" : "push");
  if (conn.landmark() == "<agnostic>")
    errh->lerror(conn.from_elt()->landmark(),
		 "agnostic `%s' in mixed context: %s input %d, %s output %d",
		 conn.from_elt()->name_cc(), type2, conn.to_port(),
		 type1, conn.from_port());
  else
    errh->lerror(conn.landmark(),
		 "`%s' %s output %d connected to `%s' %s input %d",
		 conn.from_elt()->name_cc(), type1, conn.from_port(),
		 conn.to_elt()->name_cc(), type2, conn.to_port());
}

void
ProcessingT::check_processing(ErrorHandler *errh)
{
    // add fake connections for agnostics
    Vector<ConnectionT> conn = _router->connections();
    Bitvector bv;
    for (int i = 0; i < ninput_pidx(); i++)
	if (_input_processing[i] == VAGNOSTIC) {
	    ElementT *e = const_cast<ElementT *>(_input_elt[i]);
	    int ei = e->idx();
	    int port = i - _input_pidx[ei];
	    int opidx = _output_pidx[ei];
	    int noutputs = _output_pidx[ei+1] - opidx;
	    forward_flow(e->type()->traits().flow_code(),
			 port, noutputs, &bv, errh);
	    for (int j = 0; j < noutputs; j++)
		if (bv[j] && _output_processing[opidx + j] == VAGNOSTIC)
		    conn.push_back(ConnectionT(PortT(e, j), PortT(e, port), "<agnostic>"));
	}

    // spread personalities
    while (true) {
    
	bool changed = false;
	for (int c = 0; c < conn.size(); c++) {
	    if (conn[c].dead())
		continue;

	    int offf = output_pidx(conn[c].from());
	    int offt = input_pidx(conn[c].to());
	    int pf = _output_processing[offf];
	    int pt = _input_processing[offt];

	    switch (pt) {
	
	      case VAGNOSTIC:
		if (pf != VAGNOSTIC) {
		    _input_processing[offt] = pf;
		    changed = true;
		}
		break;
	
	      case VPUSH:
	      case VPULL:
		if (pf == VAGNOSTIC) {
		    _output_processing[offf] = pt;
		    changed = true;
		} else if (pf != pt) {
		    processing_error(conn[c], pf, errh);
		    conn[c].kill();
		}
		break;
	
	    }
	}

	if (!changed)
	    break;
    }
}

static const char *
processing_name(int p)
{
  if (p == ProcessingT::VAGNOSTIC)
    return "agnostic";
  else if (p == ProcessingT::VPUSH)
    return "push";
  else if (p == ProcessingT::VPULL)
    return "pull";
  else
    return "?";
}

void
ProcessingT::check_connections(ErrorHandler *errh)
{
    Vector<int> input_used(ninput_pidx(), -1);
    Vector<int> output_used(noutput_pidx(), -1);

    // Check each hookup to ensure it doesn't reuse a port
    const Vector<ConnectionT> &conn = _router->connections();
    for (int c = 0; c < conn.size(); c++) {
	if (conn[c].dead())
	    continue;

	const PortT &hf = conn[c].from(), &ht = conn[c].to();
	int fp = output_pidx(hf), tp = input_pidx(ht);

	if (_output_processing[fp] == VPUSH && output_used[fp] >= 0) {
	    errh->lerror(conn[c].landmark(),
			 "reuse of `%s' push output %d",
			 hf.elt->name_cc(), hf.port);
	    errh->lmessage(conn[output_used[fp]].landmark(),
			   "  `%s' output %d previously used here",
			   hf.elt->name_cc(), hf.port);
	} else
	    output_used[fp] = c;

	if (_input_processing[tp] == VPULL && input_used[tp] >= 0) {
	    errh->lerror(conn[c].landmark(),
			 "reuse of `%s' pull input %d",
			 ht.elt->name_cc(), ht.port);
	    errh->lmessage(conn[input_used[tp]].landmark(),
			   "  `%s' input %d previously used here",
			   ht.elt->name_cc(), ht.port);
	} else
	    input_used[tp] = c;
    }

    // Check for unused inputs and outputs, set _connected_* properly.
    for (int i = 0; i < ninput_pidx(); i++)
	if (input_used[i] < 0) {
	    const ElementT *e = _input_elt[i];
	    if (e->dead())
		continue;
	    int port = i - _input_pidx[e->idx()];
	    errh->lerror(e->landmark(),
			 "`%s' %s input %d not connected",
			 e->name_cc(), processing_name(_input_processing[i]), port);
	}

    for (int i = 0; i < noutput_pidx(); i++)
	if (output_used[i] < 0) {
	    const ElementT *e = _output_elt[i];
	    if (e->dead())
		continue;
	    int port = i - _output_pidx[e->idx()];
	    errh->lerror(e->landmark(),
			 "`%s' %s output %d not connected",
			 e->name_cc(), processing_name(_output_processing[i]), port);
	}

    // Set _connected_* properly.
    PortT crap(0, -1);
    _connected_input.assign(ninput_pidx(), crap);
    _connected_output.assign(noutput_pidx(), crap);
    for (int i = 0; i < ninput_pidx(); i++)
	if (_input_processing[i] == VPULL && input_used[i] >= 0)
	    _connected_input[i] = conn[ input_used[i] ].from();
    for (int i = 0; i < noutput_pidx(); i++)
	if (_output_processing[i] == VPUSH && output_used[i] >= 0)
	    _connected_output[i] = conn[ output_used[i] ].to();
}

int
ProcessingT::reset(const RouterT *r, ElementMap *emap, bool flatten,
		   ErrorHandler *errh)
{
    _router = r;
    if (!errh)
	errh = ErrorHandler::silent_handler();
    int before = errh->nerrors();

    ElementMap::push_default(emap);

    // create pidx and elt arrays, warn about dead elements
    create_pidx(errh);
    
    initial_processing(errh);
    check_processing(errh);

    // 'flat' configurations have no agnostic ports; change them to push
    if (flatten)
	resolve_agnostics();
    
    check_connections(errh);

    ElementMap::pop_default();

    if (errh->nerrors() != before)
	return -1;
    return 0;
}

void
ProcessingT::resolve_agnostics()
{
    for (int i = 0; i < _input_processing.size(); i++)
	if (_input_processing[i] == VAGNOSTIC)
	    _input_processing[i] = VPUSH;
    for (int i = 0; i < _output_processing.size(); i++)
	if (_output_processing[i] == VAGNOSTIC)
	    _output_processing[i] = VPUSH;
}

bool
ProcessingT::same_processing(int a, int b) const
{
    int ai = _input_pidx[a], bi = _input_pidx[b];
    int ao = _output_pidx[a], bo = _output_pidx[b];
    int ani = _input_pidx[a+1] - ai, bni = _input_pidx[b+1] - bi;
    int ano = _output_pidx[a+1] - ao, bno = _output_pidx[b+1] - bo;
    if (ani != bni || ano != bno)
	return false;
    if (ani && memcmp(&_input_processing[ai], &_input_processing[bi], sizeof(int) * ani) != 0)
	return false;
    if (ano && memcmp(&_output_processing[ao], &_output_processing[bo], sizeof(int) * ano) != 0)
	return false;
    return true;
}

String
ProcessingT::processing_code(const ElementT *e) const
{
    assert(e->router() == _router);
    int ei = e->idx();
    StringAccum sa;
    for (int i = _input_pidx[ei]; i < _input_pidx[ei+1]; i++)
	sa << processing_letters[_input_processing[i]];
    sa << '/';
    for (int i = _output_pidx[ei]; i < _output_pidx[ei+1]; i++)
	sa << processing_letters[_output_processing[i]];
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
	    if (isalpha(*p))
		code[*p] = true;
	    else if (*p == '#')
		code[port + 128] = true;
	    else if (errh)
		errh->error("flow code: invalid character `%c'", *p);
	}
	if (negated)
	    code.negate();
	if (p == last) {
	    if (errh)
		errh->error("flow code: missing `]'");
	    p--;		// don't skip over final '\0'
	}
    } else if (isalpha(*p))
	code[*p] = true;
    else if (*p == '#')
	code[port + 128] = true;
    else {
	if (errh)
	    errh->error("flow code: invalid character `%c'", *p);
	p++;
	return -1;
    }

    p++;
    return 0;
}

int
ProcessingT::forward_flow(const String &flow_code, int input_port,
			  int noutputs, Bitvector *bv, ErrorHandler *errh)
{
    if (input_port < 0) {
	bv->assign(noutputs, false);
	return 0;
    } else if (!flow_code || (flow_code.length() == 3 && flow_code == "x/x")) {
	bv->assign(noutputs, true);
	return 0;
    }

    bv->assign(noutputs, false);

    int slash_pos = flow_code.find_left('/');
    if (slash_pos <= 0 || slash_pos == flow_code.length() - 1 || flow_code[slash_pos + 1] == '/')
	return (errh ? errh->error("flow code: missing or bad `/'") : -1);

    const char *f_in = flow_code.data();
    const char *f_out = f_in + slash_pos + 1;
    const char *f_last = f_in + flow_code.length();

    Bitvector in_code;
    for (int i = 0; i < input_port; i++)
	skip_flow_code(f_in, f_last);
    next_flow_code(f_in, f_last, input_port, in_code, errh);

    Bitvector out_code;
    for (int i = 0; i < noutputs; i++) {
	next_flow_code(f_out, f_last, i, out_code, errh);
	if (in_code.nonzero_intersection(out_code))
	    (*bv)[i] = true;
    }

    return 0;
}

int
ProcessingT::backward_flow(const String &flow_code, int output_port,
			   int ninputs, Bitvector *bv, ErrorHandler *errh)
{
    if (output_port < 0) {
	bv->assign(ninputs, false);
	return 0;
    } else if (!flow_code || (flow_code.length() == 3 && flow_code == "x/x")) {
	bv->assign(ninputs, true);
	return 0;
    }

    bv->assign(ninputs, false);

    int slash_pos = flow_code.find_left('/');
    if (slash_pos <= 0 || slash_pos == flow_code.length() - 1 || flow_code[slash_pos + 1] == '/')
	return (errh ? errh->error("flow code: missing or bad `/'") : -1);

    const char *f_in = flow_code.data();
    const char *f_out = f_in + slash_pos + 1;
    const char *f_last = f_in + flow_code.length();

    Bitvector out_code;
    for (int i = 0; i < output_port; i++)
	skip_flow_code(f_out, f_last);
    next_flow_code(f_out, f_last, output_port, out_code, errh);

    Bitvector in_code;
    for (int i = 0; i < ninputs; i++) {
	next_flow_code(f_in, f_last, i, in_code, errh);
	if (in_code.nonzero_intersection(out_code))
	    (*bv)[i] = true;
    }

    return 0;
}

void
ProcessingT::set_connected_inputs(const Bitvector &outputs, Bitvector &inputs) const
{
    assert(outputs.size() == noutput_pidx() && inputs.size() == ninput_pidx());
    const Vector<ConnectionT> &conn = _router->connections();
    for (int i = 0; i < conn.size(); i++)
	if (outputs[output_pidx(conn[i])])
	    inputs[input_pidx(conn[i])] = true;
}

void
ProcessingT::set_connected_outputs(const Bitvector &inputs, Bitvector &outputs) const
{
    assert(outputs.size() == noutput_pidx() && inputs.size() == ninput_pidx());
    const Vector<ConnectionT> &conn = _router->connections();
    for (int i = 0; i < conn.size(); i++)
	if (inputs[input_pidx(conn[i])])
	    outputs[output_pidx(conn[i])] = true;
}

void
ProcessingT::set_connected_inputs(const PortT &port, Bitvector &inputs) const
{
    assert(port.router() == _router && inputs.size() == ninput_pidx());
    const Vector<ConnectionT> &conn = _router->connections();
    for (int i = 0; i < conn.size(); i++)
	if (conn[i].from() == port)
	    inputs[input_pidx(conn[i])] = true;
}

void
ProcessingT::set_connected_outputs(const PortT &port, Bitvector &outputs) const
{
    assert(port.router() == _router && outputs.size() == noutput_pidx());
    const Vector<ConnectionT> &conn = _router->connections();
    for (int i = 0; i < conn.size(); i++)
	if (conn[i].to() == port)
	    outputs[output_pidx(conn[i])] = true;
}

void
ProcessingT::set_flowed_inputs(const Bitvector &outputs, Bitvector &inputs, ErrorHandler *errh) const
{
    assert(outputs.size() == noutput_pidx() && inputs.size() == ninput_pidx());
    Bitvector bv;
    // for speed with sparse Bitvectors, look into the Bitvector implementation
    const uint32_t *output_udata = outputs.data_words();
    for (int i = 0; i <= outputs.max_word(); i++)
	if (output_udata[i]) {
	    int m = (i*8 + 8 > outputs.size() ? outputs.size() : i*8 + 8);
	    for (int j = i*8; j < m; j++)
		if (outputs[j]) {
		    PortT p = output_port(j);
		    (void) backward_flow(p, &bv, errh);
		    inputs.or_at(bv, _input_pidx[p.elt->idx()]);
		}
	}
}

void
ProcessingT::set_flowed_outputs(const Bitvector &inputs, Bitvector &outputs, ErrorHandler *errh) const
{
    assert(outputs.size() == noutput_pidx() && inputs.size() == ninput_pidx());
    Bitvector bv;
    // for speed with sparse Bitvectors, look into the Bitvector implementation
    const uint32_t *input_udata = inputs.data_words();
    for (int i = 0; i <= inputs.max_word(); i++)
	if (input_udata[i]) {
	    int m = (i*8 + 8 > inputs.size() ? inputs.size() : i*8 + 8);
	    for (int j = i*8; j < m; j++)
		if (inputs[j]) {
		    PortT p = input_port(j);
		    (void) forward_flow(p, &bv, errh);
		    outputs.or_at(bv, _output_pidx[p.elt->idx()]);
		}
	}
}

void
ProcessingT::forward_reachable_inputs(Bitvector &inputs, ErrorHandler *errh) const
{
    assert(inputs.size() == ninput_pidx());
    Bitvector outputs(noutput_pidx(), false);
    Bitvector new_inputs(ninput_pidx(), false), diff(inputs);
    while (1) {
	set_flowed_outputs(diff, outputs, errh);
	set_connected_inputs(outputs, new_inputs);
	inputs.or_with_difference(new_inputs, diff);
	if (!diff)
	    return;
    }
}

String
ProcessingT::compound_processing_code() const
{
    ElementT *input = const_cast<ElementT *>(_router->element("input"));
    ElementT *output = const_cast<ElementT *>(_router->element("output"));
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
    ElementT *input = const_cast<ElementT *>(_router->element("input"));
    ElementT *output = const_cast<ElementT *>(_router->element("output"));
    assert(input && output && input->tunnel() && output->tunnel());

    // skip calculation in common case
    int ninputs = input->noutputs(), noutputs = output->ninputs();
    if (ninputs == 0 || noutputs == 0)
	return "x/y";

    // read flow codes, create `codes' array
    Bitvector *codes = new Bitvector[noutputs];
    for (int i = 0; i < noutputs; i++)
	codes[i].assign(ninputs, false);
    Bitvector input_vec(ninput_pidx(), false);
    int opidx = input_pidx(PortT(output, 0));
    for (int i = 0; i < ninputs; i++) {
	if (i)
	    input_vec.clear();
	set_connected_inputs(PortT(input, i), input_vec);
	forward_reachable_inputs(input_vec, errh);
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
		codeid.push_back(codeid[j]);
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
