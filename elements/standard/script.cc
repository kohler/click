// -*- c-basic-offset: 4 -*-
/*
 * script.{cc,hh} -- element provides scripting functionality
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2001 Mazu Networks, Inc.
 * Copyright (c) 2005-2008 Regents of the University of California
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
#include "script.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/handlercall.hh>
#include <click/nameinfo.hh>
#if CLICK_USERLEVEL
# include <signal.h>
# include <click/master.hh>
# include <click/userutils.hh>
#endif
CLICK_DECLS

static const StaticNameDB::Entry instruction_entries[] = {
    { "end", Script::INSN_END },
    { "error", Script::insn_error },
    { "errorq", Script::insn_errorq },
    { "exit", Script::INSN_EXIT },
    { "export", Script::insn_export },
    { "exportq", Script::insn_exportq },
    { "goto", Script::INSN_GOTO },
    { "init", Script::insn_init },
    { "initq", Script::insn_initq },
    { "label", Script::INSN_LABEL },
    { "loop", Script::INSN_LOOP_PSEUDO },
    { "pause", Script::INSN_WAIT_STEP },
    { "print", Script::INSN_PRINT },
    { "printn", Script::INSN_PRINTN },
    { "read", Script::INSN_READ },
    { "readq", Script::INSN_READQ },
    { "return", Script::INSN_RETURN },
    { "returnq", Script::insn_returnq },
    { "set", Script::INSN_SET },
    { "setq", Script::insn_setq },
    { "stop", Script::INSN_STOP },
    { "wait", Script::INSN_WAIT_PSEUDO },
    { "wait_for", Script::INSN_WAIT_TIME },
    { "wait_step", Script::INSN_WAIT_STEP },
    { "wait_stop", Script::INSN_WAIT_STEP },
    { "wait_time", Script::INSN_WAIT_TIME },
    { "write", Script::INSN_WRITE },
    { "writeq", Script::INSN_WRITEQ }
};

#if CLICK_USERLEVEL
static const StaticNameDB::Entry signal_entries[] = {
    { "ABRT", SIGABRT },
    { "HUP", SIGHUP },
    { "INT", SIGINT },
    { "PIPE", SIGPIPE },
    { "QUIT", SIGQUIT },
    { "TERM", SIGTERM },
    { "TSTP", SIGTSTP },
    { "USR1", SIGUSR1 },
    { "USR2", SIGUSR2 }
};
#endif

static NameDB *dbs[2];

void
Script::static_initialize()
{
    dbs[0] = new StaticNameDB(NameInfo::T_SCRIPT_INSN, String(), instruction_entries, sizeof(instruction_entries) / sizeof(instruction_entries[0]));
    NameInfo::installdb(dbs[0], 0);
#if CLICK_USERLEVEL
    dbs[1] = new StaticNameDB(NameInfo::T_SIGNO, String(), signal_entries, sizeof(signal_entries) / sizeof(signal_entries[0]));
    NameInfo::installdb(dbs[1], 0);
#endif
}

void
Script::static_cleanup()
{
    delete dbs[0];
#if CLICK_USERLEVEL
    delete dbs[1];
#endif
}

Script::Script()
    : _type(type_active), _write_status(0), _timer(this), _cur_steps(0)
{
}

Script::~Script()
{
}

void
Script::add_insn(int insn, int arg, int arg2, const String &arg3)
{
    // first instruction must be WAIT or WAIT_STEP, so add INITIAL if
    // necessary
    if (_insns.size() == 0 && insn > INSN_WAIT_TIME)
	add_insn(INSN_INITIAL, 0);
    _insns.push_back(insn);
    _args.push_back(arg);
    _args2.push_back(arg2);
    _args3.push_back(arg3);
}

int
Script::find_label(const String &label) const
{
    for (int i = 0; i < _insns.size(); i++)
	if (_insns[i] == INSN_LABEL && _args3[i] == label)
	    return i;
    if (label.equals("exit", 4))
	return LABEL_EXIT;
    if (label.equals("end", 3))
	return LABEL_END;
    if (label.equals("begin", 5))
	return LABEL_BEGIN;
    if (label.equals("error", 5))
	return label_error;
    return _insns.size();
}

int
Script::find_variable(const String &name, bool add)
{
    int i;
    for (i = 0; i < _vars.size(); i += 2)
	if (_vars[i] == name)
	    goto found;
    if (add) {
	_vars.push_back(name);
	_vars.push_back(String());
    }
 found:
    return i;
}

int
Script::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String type_arg;
    if (cp_va_kparse_remove_keywords
	(conf, this, errh,
	 "TYPE", 0, cpArgument, &type_arg,
	 cpEnd) < 0)
	return -1;

    int before = errh->nerrors();

    String type_word = cp_shift_spacevec(type_arg);
    if (type_word == "ACTIVE" && !type_arg)
	_type = type_active;
    else if (type_word == "PASSIVE" && !type_arg)
	_type = type_passive;
    else if ((type_word == "PACKET" || type_word == "PUSH") && !type_arg)
	_type = type_push;
    else if (type_word == "PROXY" && !type_arg)
	_type = type_proxy;
    else if (type_word == "DRIVER" && !type_arg)
	_type = type_driver;
#if CLICK_USERLEVEL
    else if (type_word == "SIGNAL") {
	_type = type_signal;
	int32_t signo;
	while ((type_word = cp_shift_spacevec(type_arg))
	       && NameInfo::query_int(NameInfo::T_SIGNO, this, type_word, &signo)
	       && signo >= 0 && signo < 32)
	    _signos.push_back(signo);
	if (type_word || !_signos.size())
	    return errh->error("bad signal number");
    }
#endif
    else if (type_word)
	return errh->error("bad TYPE");

    if (_type == type_driver) {
	if (router()->attachment("Script"))
	    return errh->error("router has more than one driver script");
	router()->set_attachment("Script", this);
    }

    if (_type == type_push && ninputs() == 0)
	return errh->error("PACKET script must have inputs");
    else if (_type != type_push && (ninputs() || noutputs()))
	return errh->error("ports allowed only on PACKET scripts");

    for (int i = 0; i < conf.size(); i++) {
	String insn_name = cp_shift_spacevec(conf[i]);
	int32_t insn;
	if (!insn_name)		// ignore as benign
	    continue;
	else if (!NameInfo::query_int(NameInfo::T_SCRIPT_INSN, this, insn_name, &insn)) {
	    errh->error("syntax error at %<%s%>", insn_name.c_str());
	    continue;
	}

	switch (insn) {

	case INSN_WAIT_PSEUDO:
	    if (!conf[i])
		goto wait_step;
	    else
		goto wait_time;

	wait_step:
	case INSN_WAIT_STEP: {
	    unsigned n = 1;
	    if (!conf[i] || cp_integer(conf[i], &n))
		add_insn(INSN_WAIT_STEP, n, 0);
	    else
		goto syntax_error;
	    break;
	}

	case INSN_WRITE:
	case INSN_WRITEQ:
	case INSN_READ:
	case INSN_READQ:
	case INSN_PRINT:
	case INSN_PRINTN:
	case INSN_GOTO:
	    add_insn(insn, 0, 0, conf[i]);
	    break;

	case INSN_RETURN:
	case insn_returnq:
	    conf[i] = "_ " + conf[i];
	    /* fall through */
	case insn_init:
	case insn_initq:
	case insn_export:
	case insn_exportq:
	case INSN_SET:
	case insn_setq: {
	    String word = cp_shift_spacevec(conf[i]);
	    if (!word
		|| ((insn == INSN_SET || insn == insn_setq) && !conf[i]))
		goto syntax_error;
	    add_insn(insn, find_variable(word, true), 0, conf[i]);
	    break;
	}

	wait_time:
	case INSN_WAIT_TIME:
	    add_insn(INSN_WAIT_TIME, 0, 0, conf[i]);
	    break;

	case INSN_LABEL: {
	    String word = cp_shift_spacevec(conf[i]);
	    if (!word || conf[i])
		goto syntax_error;
	    add_insn(insn, 0, 0, word);
	    break;
	}

	case INSN_LOOP_PSEUDO:
	    insn = INSN_GOTO;
	    /* fallthru */
	case INSN_END:
	case INSN_EXIT:
	case INSN_STOP:
	case insn_error:
	case insn_errorq:
	    if (conf[i] && insn != insn_error && insn != insn_errorq)
		goto syntax_error;
	    add_insn(insn, 0, 0, conf[i]);
	    break;

	default:
	syntax_error:
	    errh->error("syntax error at %<%s%>", insn_name.c_str());
	    break;

	}
    }

    // fix the gotos
    for (int i = 0; i < _insns.size(); i++)
	if (_insns[i] == INSN_GOTO && _args3[i]) {
	    String word = cp_shift_spacevec(_args3[i]);
	    if ((_args[i] = find_label(word)) >= _insns.size())
		errh->error("no such label %<%s%>", word.c_str());
	}

    if (_insns.size() == 0 && _type == type_driver)
	add_insn(INSN_WAIT_STEP, 1, 0);
    add_insn(_type == type_driver ? INSN_STOP : INSN_END, 0);

    return (errh->nerrors() == before ? 0 : -1);
}

int
Script::initialize(ErrorHandler *errh)
{
    _insn_pos = 0;
    _step_count = 0;
    _timer.initialize(this);

    Expander expander;
    expander.script = this;
    expander.errh = errh;
    for (int i = 0; i < _insns.size(); i++)
	if (_insns[i] == insn_init || _insns[i] == insn_export)
	    _vars[_args[i] + 1] = cp_expand(_args3[i], expander);
	else if (_insns[i] == insn_initq || _insns[i] == insn_exportq)
	    _vars[_args[i] + 1] = cp_unquote(cp_expand(_args3[i], expander));

    int insn = _insns[_insn_pos];
    assert(insn <= INSN_WAIT_TIME);
    if (_type == type_signal || _type == type_passive || _type == type_push
	|| _type == type_proxy)
	/* passive, do nothing */;
    else if (insn == INSN_WAIT_TIME) {
	Timestamp ts;
	if (cp_time(cp_expand(_args3[_insn_pos], expander), &ts))
	    _timer.schedule_after(ts);
	else
	    errh->error("syntax error at %<wait%>");
    } else if (insn == INSN_INITIAL) {
	// get rid of the initial runcount so we get called right away
	if (_type == type_driver)
	    router()->adjust_runcount(-1);
	else
	    _timer.schedule_now();
	_args[0] = 1;
    }

#if CLICK_USERLEVEL
    if (_type == type_signal)
	for (int i = 0; i < _signos.size(); i++)
	    master()->add_signal_handler(_signos[i], router(), name() + ".run");
#endif

    return 0;
}

int
Script::step(int nsteps, int step_type, int njumps, ErrorHandler *errh)
{
    Expander expander;
    expander.script = this;
    expander.errh = errh;

    nsteps += _step_count;
    while ((nsteps - _step_count) >= 0 && _insn_pos < _insns.size()
	   && njumps < max_jumps) {

	// process current instruction
	// increment instruction pointer now, in case we call 'goto' directly
	// or indirectly
	int ipos = _insn_pos++;
	int insn = _insns[ipos];

	switch (insn) {

	case INSN_STOP:
	    _step_count++;
	    _insn_pos--;
	    if (step_type != STEP_ROUTER)
		router()->adjust_runcount(-1);
	    return njumps + 1;

	case INSN_WAIT_STEP:
	    while (_step_count < nsteps && _args2[ipos] < _args[ipos]) {
		_args2[ipos]++;
		_step_count++;
	    }
	    if (_step_count == nsteps && _args2[ipos] < _args[ipos]) {
		_insn_pos--;
		goto done;
	    }
	    break;

	case INSN_WAIT_TIME:
	    if (_step_count == nsteps) {
		Timestamp ts;
		if (cp_time(cp_expand(_args3[ipos], expander), &ts)) {
		    _timer.schedule_after(ts);
		    _insn_pos--;
		} else
		    errh->error("syntax error at %<wait%>");
		goto done;
	    }
	    _step_count++;
	    _timer.unschedule();
	    break;

	case INSN_INITIAL:
	    if (_args[ipos]) {
		_step_count++;
		_args[ipos] = 0;
	    }
	    break;

	case INSN_PRINT:
	case INSN_PRINTN: {
	    String text = _args3[ipos];

#if CLICK_USERLEVEL || CLICK_TOOL
	    FILE *f = stdout;
	    if (text.length() && text[0] == '>') {
		bool append = (text.length() > 1 && text[1] == '>');
		text = text.substring(1 + append);
		String filename = cp_shift_spacevec(text);
		if (filename && filename != "-"
		    && !(f = fopen(filename.c_str(), append ? "ab" : "wb"))) {
		    errh->error("%s: %s", filename.c_str(), strerror(errno));
		    break;
		}
	    }
#else
	    if (text.length() && text[0] == '>') {
		errh->error("file redirection not supported here");
		(void) cp_shift_spacevec(text);
	    }
#endif

	    int before = errh->nerrors();
	    String result;
	    if (text && (isalpha((unsigned char) text[0]) || text[0] == '@' || text[0] == '_')) {
		result = cp_expand(text, expander);
		result = HandlerCall::call_read(result, this, errh);
	    } else
		result = cp_unquote(cp_expand(text, expander, true));
	    if (errh->nerrors() == before
		&& (!result || result.back() != '\n')
		&& insn != INSN_PRINTN)
		result += "\n";

#if CLICK_USERLEVEL || CLICK_TOOL
	    ignore_result(fwrite(result.data(), 1, result.length(), f));
	    if (f == stdout)
		fflush(f);
	    else
		fclose(f);
#else
	    click_chatter("{}%.*s", result.length(), result.data());
#endif
	    break;
	}

	case INSN_READ:
	case INSN_READQ: {
	    String arg = (insn == INSN_READ ? _args3[ipos] : cp_unquote(_args3[ipos]));
	    HandlerCall hc(cp_expand(arg, expander));
	    if (hc.initialize_read(this, errh) >= 0) {
		ContextErrorHandler c_errh(errh, "While calling %<%s%>:", hc.unparse().c_str());
		String result = hc.call_read(&c_errh);
		ErrorHandler *d_errh = ErrorHandler::default_handler();
		d_errh->message("%s:\n%.*s\n", hc.handler()->unparse_name(hc.element()).c_str(), result.length(), result.data());
	    }
	    break;
	}

	case INSN_WRITE:
	case INSN_WRITEQ: {
	    String arg = (insn == INSN_WRITE ? _args3[ipos] : cp_unquote(_args3[ipos]));
	    HandlerCall hc(cp_expand(arg, expander));
	    if (hc.initialize_write(this, errh) >= 0) {
		ContextErrorHandler c_errh(errh, "While calling %<%s%>:", hc.unparse().c_str());
		_write_status = hc.call_write(&c_errh);
	    }
	    break;
	}

	case INSN_RETURN:
	case insn_returnq:
	case INSN_SET:
	case insn_setq: {
	    expander.errh = errh;
	    _vars[_args[ipos] + 1] = cp_expand(_args3[ipos], expander);
	    if (insn == insn_setq || insn == insn_returnq)
		_vars[_args[ipos] + 1] = cp_unquote(_vars[_args[ipos] + 1]);
	    if ((insn == INSN_RETURN || insn == insn_returnq)
		&& _insn_pos == ipos + 1) {
		_insn_pos--;
		goto done;
	    }
	    break;
	}

	case INSN_GOTO: {
	    // reset intervening instructions
	    String cond_text = cp_expand(_args3[ipos], expander);
	    bool cond;
	    if (cond_text && !cp_bool(cond_text, &cond))
		errh->error("bad condition %<%s%>", cond_text.c_str());
	    else if (!cond_text || cond) {
		if (_args[ipos] < 0)
		    goto insn_finish;
		for (int i = _args[ipos]; i < ipos; i++)
		    if (_insns[i] == INSN_WAIT_STEP)
			_args2[i] = 0;
		_insn_pos = _args[ipos];
	    }
	    break;
	}

	case insn_error:
	case insn_errorq: {
	    String msg = cp_expand(_args3[ipos], expander);
	    if (insn == insn_errorq)
		msg = cp_unquote(msg);
	    if (msg)
		errh->error("%.*s", msg.length(), msg.data());
	    /* fallthru */
	}
	case INSN_END:
	case INSN_EXIT:
	insn_finish:
	    _insn_pos--;
	    return njumps + 1;

	}

	if (_insn_pos != ipos + 1)
	    njumps++;
    }

  done:
    if (njumps >= max_jumps) {
	ErrorHandler::default_handler()->error("%{element}: too many jumps, giving up", this);
	_insn_pos = _insns.size();
	_timer.unschedule();
    }
    if (step_type == STEP_ROUTER)
	router()->adjust_runcount(1);
    return njumps + 1;
}

int
Script::complete_step(String *retval)
{
    int last_insn;
    if (_insn_pos < 0 || _insn_pos >= _insns.size())
	last_insn = -1;
    else {
	last_insn = _insns[_insn_pos];
	if (last_insn == INSN_GOTO) {
	    if (_args[_insn_pos] == LABEL_EXIT)
		last_insn = INSN_EXIT;
	    else if (_args[_insn_pos] == LABEL_END)
		last_insn = INSN_END;
	    else if (_args[_insn_pos] == label_error)
		last_insn = insn_error;
	}
    }

#if CLICK_USERLEVEL
    if (last_insn == INSN_END && _type == type_signal)
	for (int i = 0; i < _signos.size(); i++)
	    master()->add_signal_handler(_signos[i], router(), name() + ".run");
#endif

    if (retval) {
	int x;
	*retval = String();
	if (last_insn == INSN_RETURN || last_insn == insn_returnq) {
	    x = find_variable(String::make_stable("_", 1), false);
	    if (x < _vars.size())
		*retval = _vars[x + 1];
	} else if (last_insn == INSN_END && _type == type_push)
	    *retval = String::make_stable("0", 1);
    }

    if (last_insn == insn_error || last_insn == insn_errorq)
	return -1;
    else if (last_insn == INSN_STOP)
	return 1;
    else
	return 0;
}

void
Script::run_timer(Timer *)
{
    // called when a timer expires
    assert(_insns[_insn_pos] == INSN_WAIT_TIME || _insns[_insn_pos] == INSN_INITIAL);
    ErrorHandler *errh = ErrorHandler::default_handler();
    ContextErrorHandler cerrh(errh, "While executing %<%{element}%>:", this);
    step(1, STEP_TIMER, 0, &cerrh);
    complete_step(0);
}

void
Script::push(int port, Packet *p)
{
    ErrorHandler *errh = ErrorHandler::default_handler();
    ContextErrorHandler cerrh(errh, "While executing %<%{element}%>:", this);

    // This is slow, but it probably doesn't need to be fast.
    int i = find_variable(String::make_stable("input", 5), true);
    _vars[i + 1] = String(port);

    _insn_pos = 0;
    step(0, STEP_JUMP, 0, &cerrh);
    String out;
    complete_step(&out);

    port = -1;
    (void) cp_integer(out, &port);
    checked_output_push(port, p);
}

Packet *
Script::pull(int)
{
    Packet *p = input(0).pull();
    if (!p)
	return 0;

    ErrorHandler *errh = ErrorHandler::default_handler();
    ContextErrorHandler cerrh(errh, "While executing %<%{element}%>:", this);

    // This is slow, but it probably doesn't need to be fast.
    int i = find_variable(String::make_stable("input", 5), true);
    _vars[i + 1] = String::make_stable("0", 1);

    _insn_pos = 0;
    step(0, STEP_JUMP, 0, &cerrh);
    String out;
    complete_step(&out);

    int port = -1;
    (void) cp_integer(out, &port);
    if (port == 0)
	return p;
    else {
	checked_output_push(port, p);
	return 0;
    }
}

bool
Script::Expander::expand(const String &vname, int vartype, int quote, StringAccum &sa) const
{
    int x = script->find_variable(vname, false);
    if (x < script->_vars.size()) {
	sa << cp_expand_in_quotes(script->_vars[x + 1], quote);
	return true;
    }

    if (vname.length() == 1 && vname[0] == '?') {
	sa << script->_write_status;
	return true;
    }

    if (vname.equals("args", 4)) {
	sa << cp_expand_in_quotes(script->_run_args, quote);
	return true;
    }

    if (vname.length() == 1 && vname[0] == '0') {
	sa << script->_run_handler_name;
	return true;
    }

    if (vname.length() > 0 && vname[0] >= '1' && vname[0] <= '9'
	&& cp_integer(vname, &x)) {
	String arg, run_args = script->_run_args;
	for (; x > 0; --x)
	    arg = cp_shift_spacevec(run_args);
	sa << cp_expand_in_quotes(arg, quote);
	return true;
    }

    if (vname.length() == 1 && vname[0] == '#') {
	String run_args = script->_run_args;
	for (x = 0; cp_shift_spacevec(run_args); )
	    ++x;
	sa << cp_expand_in_quotes(String(x), quote);
	return true;
    }

    if (vname.equals("write", 5)) {
	sa << cp_unparse_bool(script->_run_op & Handler::OP_WRITE);
	return true;
    }

    if (vartype == '(') {
	HandlerCall hc(vname);
	if (hc.initialize_read(script, errh) >= 0) {
	    sa << cp_expand_in_quotes(hc.call_read(errh), quote);
	    return true;
	}
    }

    return false;
}

int
Script::step_handler(int op, String &str, Element *e, const Handler *h, ErrorHandler *errh)
{
    Script *scr = (Script *) e;
    String data = cp_uncomment(str);
    int nsteps, steptype;
    int what = (uintptr_t) h->user_data1();
    scr->_run_handler_name = h->name();
    scr->_run_args = String();
    scr->_run_op = op;

    if (what == ST_GOTO) {
	int step = scr->find_label(cp_uncomment(data));
	if (step >= scr->_insns.size() || step < 0)
	    return errh->error("jump to nonexistent label");
	for (int i = step; i < scr->_insns.size(); i++)
	    if (scr->_insns[i] == INSN_WAIT_STEP)
		scr->_args2[i] = 0;
	scr->_insn_pos = step;
	nsteps = 0, steptype = STEP_JUMP;
    } else if (what == ST_RUN) {
	scr->_insn_pos = 0;
	scr->_run_args = str;
	nsteps = 0, steptype = STEP_JUMP;
    } else {
	if (data == "router")
	    nsteps = 1, steptype = STEP_ROUTER;
	else if (!data)
	    nsteps = 1, steptype = STEP_NORMAL;
	else if (cp_integer(data, &nsteps))
	    steptype = STEP_NORMAL;
	else
	    return errh->error("syntax error");
    }

    if (!scr->_cur_steps) {
	int cur_steps = nsteps, njumps = 0;
	scr->_cur_steps = &cur_steps;
	while ((nsteps = cur_steps) >= 0) {
	    cur_steps = -1;
	    njumps = scr->step(nsteps, steptype, njumps, errh);
	    steptype = ST_STEP;
	}
	scr->_cur_steps = 0;
    } else if (what == ST_STEP)
	*scr->_cur_steps += nsteps;

    return scr->complete_step(&str);
}

#if HAVE_INT64_TYPES
# define click_intmax_t int64_t
# define click_uintmax_t uint64_t
# define CLICK_INTMAX_CVT "ll"
#else
# define click_intmax_t int32_t
# define click_uintmax_t uint32_t
# define CLICK_INTMAX_CVT 'l'
#endif

int
Script::arithmetic_handler(int, String &str, Element *e, const Handler *h, ErrorHandler *errh)
{
    int what = (uintptr_t) h->user_data1();

    switch (what) {

    case AR_FIRST:
	str = cp_shift_spacevec(str);
	return 0;

    case AR_ADD:
    case AR_SUB:
    case AR_MUL:
    case AR_DIV:
    case AR_IDIV: {
	click_intmax_t accum = (what == AR_ADD || what == AR_SUB ? 0 : 1), arg;
	bool first = true;
#if CLICK_USERLEVEL
	double daccum = (what == AR_ADD || what == AR_SUB ? 0 : 1), darg;
	bool use_daccum = (what == AR_DIV || what == AR_IDIV);
#endif
	while (1) {
	    String word = cp_shift_spacevec(str);
	    if (!word && cp_is_space(str))
		break;
#if CLICK_USERLEVEL
	    if (!use_daccum && !cp_integer(word, &arg)) {
		use_daccum = true;
		daccum = accum;
	    }
	    if (use_daccum && !cp_double(word, &darg))
		return errh->error("expected list of numbers");
	    if (use_daccum) {
		if (first)
		    daccum = darg;
		else if (what == AR_ADD)
		    daccum += darg;
		else if (what == AR_SUB)
		    daccum -= darg;
		else if (what == AR_MUL)
		    daccum *= darg;
		else
		    daccum /= darg;
		goto set_first;
	    }
#else
	    if (!cp_integer(word, &arg))
		return errh->error("expected list of numbers");
#endif
	    if (first)
		accum = arg;
	    else if (what == AR_ADD)
		accum += arg;
	    else if (what == AR_SUB)
		accum -= arg;
	    else if (what == AR_MUL)
		accum *= arg;
	    else {
#if CLICK_USERLEVEL || !HAVE_INT64_TYPES
		accum /= arg;
#else
		// no int64 divide in the kernel
		if (accum > 0x7FFFFFFF || arg > 0x7FFFFFFF)
		    errh->warning("int64 divide truncated");
		accum = (int32_t) accum / (int32_t) arg;
#endif
	    }
#if CLICK_USERLEVEL
	set_first:
#endif
	    first = false;
	}
#if CLICK_USERLEVEL
	if (what == AR_IDIV) {
	    use_daccum = false;
	    accum = (click_intmax_t) daccum;
	}
	str = (use_daccum ? String(daccum) : String(accum));
#else
	str = String(accum);
#endif
	return 0;
    }

    case ar_mod:
    case ar_rem: {
	String astr = cp_shift_spacevec(str), bstr = cp_shift_spacevec(str);
	click_intmax_t a, b;
	if (str || !astr || !bstr)
	    goto expected_two_numbers;
	if (!cp_integer(astr, &a) || !cp_integer(bstr, &b)) {
#if CLICK_USERLEVEL
	    double da, db;
	    if (what == ar_mod || !cp_double(astr, &da) || !cp_double(bstr, &db))
		goto expected_two_numbers;
	    str = String(fmod(da, db));
	    return 0;
#else
	    goto expected_two_numbers;
#endif
	} else {
#if CLICK_LINUXMODULE
	    if ((int32_t) a != a || (int32_t) b != b)
		errh->warning("int64 divide truncated");
	    a = (int32_t) a % (int32_t) b;
#else
	    a %= b;
#endif
	    str = String(a);
	    return 0;
	}
    }

    case AR_LT:
    case AR_EQ:
    case AR_GT:
    case AR_LE:
    case AR_NE:
    case AR_GE: {
	String astr = cp_shift_spacevec(str), bstr = cp_shift_spacevec(str);
	click_intmax_t a, b;
	int comparison;
	if (str || !astr || !bstr)
	    goto expected_two_numbers;
#if CLICK_USERLEVEL
	if (!cp_integer(astr, &a) || !cp_integer(bstr, &b)) {
	    double da, db;
	    if (!cp_double(astr, &da) || !cp_double(bstr, &db))
		goto compare_strings;
	    comparison = (da < db ? AR_LT : (da == db ? AR_EQ : AR_GT));
	    goto compare_return;
	}
#else
	if (!cp_integer(astr, &a) || !cp_integer(bstr, &b))
	    goto compare_strings;
#endif
	comparison = (a < b ? AR_LT : (a == b ? AR_EQ : AR_GT));
    compare_return:
	str = cp_unparse_bool(what == comparison
			      || (what >= AR_GE && what != comparison + 3));
	return 0;
    compare_strings:
	a = String::compare(cp_unquote(astr), cp_unquote(bstr));
	comparison = (a < 0 ? AR_LT : (a == 0 ? AR_EQ : AR_GT));
	goto compare_return;
    }

    case AR_NOT: {
	bool x;
	if (!cp_bool(str, &x))
	    return errh->error("syntax error");
	str = cp_unparse_bool(!x);
	return 0;
    }

    case ar_and:
    case ar_nand:
    case ar_or:
    case ar_nor: {
	bool zero = (what == ar_and || what == ar_nand), current_value = zero;
	while (current_value == zero && str) {
	    bool x;
	    if (!cp_bool(cp_shift_spacevec(str), &x))
		return errh->error("syntax error");
	    current_value = (what == ar_and ? current_value && x : current_value || x);
	}
	if (what == ar_nand || what == ar_nor)
	    current_value = !current_value;
	str = cp_unparse_bool(current_value);
	return 0;
    }

    case ar_if: {
	bool x;
	if (!cp_bool(cp_shift_spacevec(str), &x))
	    return errh->error("syntax error");
	if (x)
	    str = cp_shift_spacevec(str);
	else {
	    (void) cp_shift_spacevec(str);
	    str = cp_shift_spacevec(str);
	}
	return 0;
    }

    case ar_in: {
	String word = cp_unquote(cp_shift_spacevec(str));
	bool answer = false;
	while (word && str && !answer)
	    answer = (word == cp_unquote(cp_shift_spacevec(str)));
	str = cp_unparse_bool(answer);
	return 0;
    }

    case ar_now:
	str = Timestamp::now().unparse();
	return 0;

    case AR_SPRINTF: {
	String format = cp_unquote(cp_shift_spacevec(str));
	const char *s = format.begin(), *pct, *end = format.end();
	StringAccum result;
	while ((pct = find(s, end, '%')) < end) {
	    result << format.substring(s, pct);
	    StringAccum pf;
	    // flags
	    do {
		pf << *pct++;
	    } while (pct < end && (*pct == '0' || *pct == '#'
				|| *pct == '-' || *pct == ' ' || *pct == '+'));
	    // field width
	    int fw;
	    if (pct < end && *pct == '*') {
		if (!cp_integer(cp_shift_spacevec(str), &fw))
		    return errh->error("syntax error");
		pf << fw;
	    } else
		while (pct < end && *pct >= '0' && *pct <= '9')
		    pf << *pct++;
	    // precision
	    if (pct < end && *pct == '.') {
		pct++;
		if (pct < end && *pct == '*') {
		    if (!cp_integer(cp_shift_spacevec(str), &fw) || fw < 0)
			return errh->error("syntax error");
		    pf << '.' << fw;
		} else if (pct < end && *pct >= '0' && *pct <= '9') {
		    pf << '.';
		    while (pct < end && *pct >= '0' && *pct <= '9')
			pf << *pct++;
		}
	    }
	    // width
	    int width_flag = 0;
	    while (1) {
		if (pct < end && *pct == 'h')
		    width_flag = 'h', pct++;
		else if (pct < end && *pct == 'l')
		    width_flag = (width_flag == 'l' ? 'q' : 'l'), pct++;
		else if (pct < end && (*pct == 'L' || *pct == 'q'))
		    width_flag = 'q', pct++;
		else
		    break;
	    }
	    // conversion
	    if (pct < end && (*pct == 'o' || *pct == 'x' || *pct == 'X' || *pct == 'u')) {
		click_uintmax_t ival;
		String x = cp_shift_spacevec(str);
		if (!cp_integer(x, &ival))
		    return errh->error("syntax error");
		if (width_flag == 'h')
		    ival = (unsigned short) ival;
		else if (width_flag == 0 || width_flag == 'l')
		    ival = (unsigned long) ival;
		pf << CLICK_INTMAX_CVT << *pct;
		result << ErrorHandler::xformat(pf.c_str(), ival);
	    } else if (pct < end && (*pct == 'd' || *pct == 'i')) {
		click_intmax_t ival;
		if (!cp_integer(cp_shift_spacevec(str), &ival))
		    return errh->error("syntax error");
		if (width_flag == 'h')
		    ival = (short) ival;
		else if (width_flag == 0 || width_flag == 'l')
		    ival = (long) ival;
		pf << CLICK_INTMAX_CVT << *pct;
		result << ErrorHandler::xformat(pf.c_str(), ival);
	    } else if (pct < end && *pct == '%') {
		pf << '%';
		result << ErrorHandler::xformat(pf.c_str());
	    } else if (pct < end && *pct == 's') {
		String s;
		if (!cp_string(cp_shift_spacevec(str), &s))
		    return errh->error("syntax error");
		pf << *pct;
		result << ErrorHandler::xformat(pf.c_str(), s.c_str());
	    } else
		return errh->error("syntax error");
	    s = pct + 1;
	}
	if (str)
	    return errh->error("syntax error");
	result << format.substring(s, pct);
	str = result.take_string();
	return 0;
    }

    case ar_random: {
	if (!str)
	    str = String(click_random());
	else {
	    uint32_t n1, n2;
	    bool have_n2 = false;
	    if (cp_va_space_kparse(str, e, errh,
				   "N1", cpkP+cpkM, cpUnsigned, &n1,
				   "N2", cpkP+cpkM+cpkC, &have_n2, cpUnsigned, &n2,
				   cpEnd) < 0)
		return -1;
	    if ((have_n2 && n2 < n1) || (!have_n2 && n1 == 0))
		return errh->error("bad N");
	    if (have_n2)
		str = String(click_random(n1, n2));
	    else
		str = String(click_random(0, n1 - 1));
	}
	return 0;
    }

#if CLICK_USERLEVEL
    case ar_cat: {
	String filename;
	if (!cp_filename(str, &filename))
	    return errh->error("bad FILE");
	int nerrors = errh->nerrors();
	str = file_string(filename, errh);
	return (errh->nerrors() == nerrors ? 0 : -errno);
    }
#endif

    case ar_readable:
    case ar_writable: {
	Element *el;
	const Handler *h;
	int f = (what == ar_readable ? Handler::OP_READ : Handler::OP_WRITE);
	while (String hname = cp_shift_spacevec(str))
	    if (!cp_handler(hname, f, &el, &h, e)) {
		str = String(false);
		return 0;
	    }
	str = String(true);
	return 0;
    }

    case ar_readexport: {
	Script *scr = static_cast<Script *>(e);
	int v = (intptr_t) h->user_data2();
	str = scr->_vars[v + 1];
	return 0;
    }

    expected_two_numbers:
	return errh->error("expected two numbers");

    }

    return -1;
}

int
Script::star_write_handler(const String &str, Element *e, void *, ErrorHandler *)
{
    Script *s = static_cast<Script *>(e);
    s->set_handler(str, Handler::OP_READ | Handler::READ_PARAM | Handler::OP_WRITE, step_handler, ST_RUN, 0);
    return Router::hindex(s, str);
}

void
Script::add_handlers()
{
    set_handler("step", Handler::OP_WRITE, step_handler, ST_STEP, 0);
    set_handler("goto", Handler::OP_WRITE, step_handler, ST_GOTO, 0);
    set_handler("run", Handler::OP_READ | Handler::READ_PARAM | Handler::OP_WRITE, step_handler, ST_RUN, 0);
    set_handler("add", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_ADD, 0);
    set_handler("sub", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_SUB, 0);
    set_handler("mul", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_MUL, 0);
    set_handler("div", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_DIV, 0);
    set_handler("idiv", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_IDIV, 0);
    set_handler("mod", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_mod, 0);
    set_handler("rem", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_rem, 0);
    set_handler("eq", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_EQ, 0);
    set_handler("ne", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_NE, 0);
    set_handler("gt", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_GT, 0);
    set_handler("ge", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_GE, 0);
    set_handler("lt", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_LT, 0);
    set_handler("le", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_LE, 0);
    set_handler("not", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_NOT, 0);
    set_handler("sprintf", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_SPRINTF, 0);
    set_handler("first", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, AR_FIRST, 0);
    set_handler("random", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_random, 0);
    set_handler("and", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_and, 0);
    set_handler("or", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_or, 0);
    set_handler("nand", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_nand, 0);
    set_handler("nor", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_nor, 0);
    set_handler("if", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_if, 0);
    set_handler("in", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_in, 0);
    set_handler("now", Handler::OP_READ, arithmetic_handler, ar_now, 0);
    set_handler("readable", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_readable, 0);
    set_handler("writable", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_writable, 0);
#if CLICK_USERLEVEL
    set_handler("cat", Handler::OP_READ | Handler::READ_PARAM | Handler::READ_PRIVATE, arithmetic_handler, ar_cat, 0);
#endif
    if (_type == type_proxy)
	add_write_handler("*", star_write_handler, 0);
    for (int i = 0; i < _insns.size(); ++i)
	if (_insns[i] == insn_export || _insns[i] == insn_exportq)
	    set_handler(_vars[_args[i]], Handler::OP_READ, arithmetic_handler, ar_readexport, _args[i]);
}

EXPORT_ELEMENT(Script)
CLICK_ENDDECLS
