// -*- c-basic-offset: 4 -*-
/*
 * script.{cc,hh} -- element provides scripting functionality
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2001 Mazu Networks, Inc.
 * Copyright (c) 2005-2008 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
 * Copyright (c) 2012 Eddie Kohler
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
#if CLICK_USERLEVEL
    { "append", Script::insn_append },
#endif
    { "end", (uint32_t) Script::insn_end },
    { "error", (uint32_t) Script::insn_error },
    { "errorq", (uint32_t) Script::insn_errorq },
    { "exit", (uint32_t) Script::insn_exit },
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
    { "printnq", Script::INSN_PRINTNQ },
    { "printq", Script::INSN_PRINTQ },
    { "read", Script::INSN_READ },
    { "readq", Script::INSN_READQ },
    { "return", Script::INSN_RETURN },
    { "returnq", Script::insn_returnq },
#if CLICK_USERLEVEL
    { "save", Script::insn_save },
#endif
    { "set", Script::INSN_SET },
    { "setq", Script::insn_setq },
    { "stop", (uint32_t) Script::insn_stop },
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
    { "ALRM", SIGALRM },
    { "HUP", SIGHUP },
    { "INT", SIGINT },
# ifdef SIGIO
    { "IO", SIGIO },
# endif
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

void
Script::add_insn(int insn, int arg, int arg2, const String &arg3)
{
    // first instruction must be WAIT or WAIT_STEP, so add INITIAL if
    // necessary
    if (_insns.size() == 0 && insn != INSN_INITIAL && insn != INSN_WAIT_STEP
	&& insn != INSN_WAIT_TIME)
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
    int32_t insn;
    if (NameInfo::query_int(NameInfo::T_SCRIPT_INSN, this, label, &insn)
	&& insn < 0)
	return insn;		// negative instructions are also labels
    if (label.equals("begin", 5) || label.equals("loop", 4))
	return 0;
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
    if (Args(this, errh).bind(conf)
	.read("TYPE", AnyArg(), type_arg)
	.consume() < 0)
	return -1;

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
	    if (!conf[i] || IntArg().parse(conf[i], n))
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
	case INSN_PRINTQ:
	case INSN_PRINTN:
	case INSN_PRINTNQ:
#if CLICK_USERLEVEL
	case insn_save:
	case insn_append:
#endif
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
	case insn_end:
	case insn_exit:
	case insn_stop:
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
    add_insn(_type == type_driver ? insn_stop : insn_end, 0);

    return errh->nerrors() ? -1 : 0;
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
    assert(insn == INSN_INITIAL || insn == INSN_WAIT_STEP || INSN_WAIT_TIME);
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

	case insn_stop:
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

#if CLICK_USERLEVEL
	case insn_save:
	case insn_append: {
	    String word = cp_shift_spacevec(_args3[ipos]);
	    String file = (_args3[ipos] ? _args3[ipos] : "-");
	    _args3[ipos] = (">>" + (insn == insn_save)) + file + " " + word;
	    /* FALLTHRU */
	}
#endif
	case INSN_PRINT:
	case INSN_PRINTQ:
	case INSN_PRINTN:
	case INSN_PRINTNQ: {
	    String text = _args3[ipos];

#if CLICK_USERLEVEL
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
		HandlerCall hc(cp_expand(text, expander));
		int flags = HandlerCall::OP_READ + ((insn == INSN_PRINTQ || insn == INSN_PRINTNQ) ? HandlerCall::UNQUOTE_PARAM : 0);
		if (hc.initialize(flags, this, errh) >= 0) {
		    ContextErrorHandler c_errh(errh, "While calling %<%s%>:", hc.unparse().c_str());
		    result = hc.call_read(&c_errh);
		}
	    } else
		result = cp_unquote(cp_expand(text, expander, true));
	    if (errh->nerrors() == before
		&& (!result || result.back() != '\n')
		&& insn != INSN_PRINTN
		&& insn != INSN_PRINTNQ)
		result += "\n";

#if CLICK_USERLEVEL
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
	    HandlerCall hc(cp_expand(_args3[ipos], expander));
	    int flags = HandlerCall::OP_READ + (insn == INSN_READQ ? HandlerCall::UNQUOTE_PARAM : 0);
	    if (hc.initialize(flags, this, errh) >= 0) {
		ContextErrorHandler c_errh(errh, "While calling %<%s%>:", hc.unparse().c_str());
		String result = hc.call_read(&c_errh);
		ErrorHandler *d_errh = ErrorHandler::default_handler();
		d_errh->message("%s:\n%.*s\n", hc.handler()->unparse_name(hc.element()).c_str(), result.length(), result.data());
	    }
	    break;
	}

	case INSN_WRITE:
	case INSN_WRITEQ: {
	    HandlerCall hc(cp_expand(_args3[ipos], expander));
	    int flags = HandlerCall::OP_WRITE + (insn == INSN_WRITEQ ? HandlerCall::UNQUOTE_PARAM : 0);
	    if (hc.initialize(flags, this, errh) >= 0) {
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
	    if (cond_text && !BoolArg().parse(cond_text, cond))
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
	case insn_end:
	case insn_exit:
	insn_finish:
	    _insn_pos--;
	    return njumps + 1;

	}

	if (_insn_pos != ipos + 1)
	    njumps++;
    }

  done:
    if (njumps >= max_jumps) {
	ErrorHandler::default_handler()->error("%p{element}: too many jumps, giving up", this);
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
	if (last_insn == INSN_GOTO && _args[_insn_pos] < 0)
	    last_insn = _args[_insn_pos];
    }

#if CLICK_USERLEVEL
    if (last_insn == insn_end && _type == type_signal)
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
	} else if (last_insn == insn_end && _type == type_push)
	    *retval = String::make_stable("0", 1);
    }

    if (last_insn == insn_error || last_insn == insn_errorq)
	return -1;
    else if (last_insn == insn_stop)
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
    ContextErrorHandler cerrh(errh, "While executing %<%p{element}%>:", this);
    step(1, STEP_TIMER, 0, &cerrh);
    complete_step(0);
}

void
Script::push(int port, Packet *p)
{
    ErrorHandler *errh = ErrorHandler::default_handler();
    ContextErrorHandler cerrh(errh, "While executing %<%p{element}%>:", this);

    // This is slow, but it probably doesn't need to be fast.
    int i = find_variable(String::make_stable("input", 5), true);
    _vars[i + 1] = String(port);

    _insn_pos = 0;
    step(0, STEP_JUMP, 0, &cerrh);
    String out;
    complete_step(&out);

    port = -1;
    (void) IntArg().parse(out, port);
    checked_output_push(port, p);
}

Packet *
Script::pull(int)
{
    Packet *p = input(0).pull();
    if (!p)
	return 0;

    ErrorHandler *errh = ErrorHandler::default_handler();
    ContextErrorHandler cerrh(errh, "While executing %<%p{element}%>:", this);

    // This is slow, but it probably doesn't need to be fast.
    int i = find_variable(String::make_stable("input", 5), true);
    _vars[i + 1] = String::make_stable("0", 1);

    _insn_pos = 0;
    step(0, STEP_JUMP, 0, &cerrh);
    String out;
    complete_step(&out);

    int port = -1;
    (void) IntArg().parse(out, port);
    if (port == 0)
	return p;
    else {
	checked_output_push(port, p);
	return 0;
    }
}

int
Script::Expander::expand(const String &vname, String &out, int vartype, int) const
{
    int x = script->find_variable(vname, false);
    if (x < script->_vars.size()) {
	out = script->_vars[x + 1];
	return true;
    }

    if (vname.length() == 1 && vname[0] == '?') {
	out = String(script->_write_status);
	return true;
    }

    if (vname.equals("args", 4)) {
	out = script->_run_args;
	return true;
    }

    if (vname.length() == 1 && vname[0] == '$') {
#if CLICK_USERLEVEL
	out = String(getpid());
#elif CLICK_BSDMODULE
	out = String(curproc->p_pid);
#else
	out = String(current->pid);
#endif
	return true;
    }

    if (vname.length() == 1 && vname[0] == '0') {
	out = script->_run_handler_name;
	return true;
    }

    if (vname.length() > 0 && vname[0] >= '1' && vname[0] <= '9'
	&& IntArg().parse(vname, x)) {
	String arg, run_args = script->_run_args;
	for (; x > 0; --x)
	    arg = cp_shift_spacevec(run_args);
	out = arg;
	return true;
    }

    if (vname.length() == 1 && vname[0] == '#') {
	String run_args = script->_run_args;
	for (x = 0; cp_shift_spacevec(run_args); )
	    ++x;
	out = String(x);
	return true;
    }

    if (vname.equals("write", 5)) {
	out = BoolArg::unparse(script->_run_op & Handler::OP_WRITE);
	return true;
    }

    if (vartype == '(') {
	HandlerCall hc(vname);
	if (hc.initialize_read(script, errh) >= 0) {
	    out = hc.call_read(errh);
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
    int what = (uintptr_t) h->write_user_data();
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
	else if (IntArg().parse(data, nsteps))
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
Script::arithmetic_handler(int, String &str, Element *, const Handler *h, ErrorHandler *errh)
{
    int what = (uintptr_t) h->read_user_data();

    click_intmax_t accum = (what == ar_add || what == ar_sub ? 0 : 1), arg;
    bool first = true;
#if CLICK_USERLEVEL
    double daccum = (what == ar_add || what == ar_sub ? 0 : 1), darg;
    bool use_daccum = (what == ar_div || what == ar_idiv);
#endif
    while (1) {
	String word = cp_shift_spacevec(str);
	if (!word && cp_is_space(str))
	    break;
#if CLICK_USERLEVEL
	if (!use_daccum && !IntArg().parse(word, arg)) {
	    use_daccum = true;
	    daccum = accum;
	}
	if (use_daccum && !DoubleArg().parse(word, darg))
	    return errh->error("expected list of numbers");
	if (use_daccum) {
	    if (first)
		daccum = darg;
	    else if (what == ar_add)
		daccum += darg;
	    else if (what == ar_sub)
		daccum -= darg;
            else if (what == ar_min)
                daccum = (daccum < darg ? daccum : darg);
            else if (what == ar_max)
                daccum = (daccum > darg ? daccum : darg);
	    else if (what == ar_mul)
		daccum *= darg;
	    else
		daccum /= darg;
	    goto set_first;
	}
#else
	if (!IntArg().parse(word, arg))
	    return errh->error("expected list of numbers");
#endif
	if (first)
	    accum = arg;
	else if (what == ar_add)
	    accum += arg;
	else if (what == ar_sub)
	    accum -= arg;
	else if (what == ar_min)
	    accum = (accum < arg ? accum : arg);
	else if (what == ar_max)
	    accum = (accum > arg ? accum : arg);
	else if (what == ar_mul)
	    accum *= arg;
	else {
#if CLICK_USERLEVEL || !HAVE_INT64_TYPES
	    accum /= arg;
#elif CLICK_LINUXMODULE && BITS_PER_LONG >= 64
	    accum /= arg;
#elif CLICK_LINUXMODULE && defined(do_div)
	    if ((click_uintmax_t) arg > 0x7FFFFFFF) {
		errh->warning("int64 divide truncated");
		accum = 0;
	    } else
		accum = int_divide(accum, arg);
#else
	    // no int64 divide in the kernel
	    if ((click_uintmax_t) accum > 0x7FFFFFFF
		|| (click_uintmax_t) arg > 0x7FFFFFFF)
		accum = 0;
	    else
		accum = (int32_t) accum / (int32_t) arg;
#endif
	}
#if CLICK_USERLEVEL
    set_first:
#endif
	first = false;
    }
#if CLICK_USERLEVEL
    if (what == ar_idiv) {
	use_daccum = false;
	accum = (click_intmax_t) daccum;
    }
    str = (use_daccum ? String(daccum) : String(accum));
#else
    str = String(accum);
#endif
    return 0;
}

int
Script::normal_error(int message, ErrorHandler *errh)
{
    static const char * const messages[] = {
	"expected one number", "expected two numbers"
    };
    return errh->error(messages[message]);
}

int
Script::modrem_handler(int, String &str, Element *, const Handler *h, ErrorHandler *errh)
{
    int what = (uintptr_t) h->read_user_data();
    (void) what;

    String astr = cp_shift_spacevec(str), bstr = cp_shift_spacevec(str);
    click_intmax_t a, b;
    if (str || !astr || !bstr)
	return normal_error(error_two_numbers, errh);
    if (!IntArg().parse(astr, a) || !IntArg().parse(bstr, b)) {
#if CLICK_USERLEVEL
	double da, db;
	if (what == ar_mod || !DoubleArg().parse(astr, da)
	    || !DoubleArg().parse(bstr, db))
	    return normal_error(error_two_numbers, errh);
	str = String(fmod(da, db));
	return 0;
#else
	return normal_error(error_two_numbers, errh);
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

int
Script::negabs_handler(int, String &str, Element *, const Handler *h, ErrorHandler *errh)
{
    int what = (uintptr_t) h->read_user_data();

    click_intmax_t x;
    if (!IntArg().parse(str, x)) {
#if CLICK_USERLEVEL
	double dx;
	if (!DoubleArg().parse(str, dx))
	    return normal_error(error_one_number, errh);
	str = String(what == ar_neg ? -dx : fabs(dx));
	return 0;
#else
	return normal_error(error_one_number, errh);
#endif
    } else {
	str = String(what == ar_neg || x < 0 ? -x : x);
	return 0;
    }
}

int
Script::compare_handler(int, String &str, Element *, const Handler *h, ErrorHandler *errh)
{
    int what = (uintptr_t) h->read_user_data();

    String astr = cp_shift_spacevec(str), bstr = cp_shift_spacevec(str);
    click_intmax_t a, b;
    int comparison;
    if (str || !astr || !bstr)
	return normal_error(error_two_numbers, errh);
#if CLICK_USERLEVEL
    if (!IntArg().parse(astr, a) || !IntArg().parse(bstr, b)) {
	double da, db;
	if (!DoubleArg().parse(astr, da) || !DoubleArg().parse(bstr, db))
	    goto compare_strings;
	comparison = (da < db ? AR_LT : (da == db ? AR_EQ : AR_GT));
	goto compare_return;
    }
#else
    if (!IntArg().parse(astr, a) || !IntArg().parse(bstr, b))
	goto compare_strings;
#endif
    comparison = (a < b ? AR_LT : (a == b ? AR_EQ : AR_GT));
 compare_return:
    str = BoolArg::unparse(what == comparison
			   || (what >= AR_GE && what != comparison + 3));
    return 0;
 compare_strings:
    a = String::compare(cp_unquote(astr), cp_unquote(bstr));
    comparison = (a < 0 ? AR_LT : (a == 0 ? AR_EQ : AR_GT));
    goto compare_return;
}

int
Script::sprintf_handler(int, String &str, Element *, const Handler *, ErrorHandler *errh)
{
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
	int fw = 0;
	if (pct < end && *pct == '*') {
	    if (!IntArg().parse(cp_shift_spacevec(str), fw))
		return errh->error("syntax error");
	    pf << fw;
	} else
	    while (pct < end && *pct >= '0' && *pct <= '9')
		pf << *pct++;
	// precision
	if (pct < end && *pct == '.') {
	    pct++;
	    if (pct < end && *pct == '*') {
		if (!IntArg().parse(cp_shift_spacevec(str), fw) || fw < 0)
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
	    if (!IntArg().parse(x, ival))
		return errh->error("syntax error");
	    if (width_flag == 'h')
		ival = (unsigned short) ival;
	    else if (width_flag == 0 || width_flag == 'l')
		ival = (unsigned long) ival;
	    pf << CLICK_INTMAX_CVT << *pct;
	    result << ErrorHandler::xformat(pf.c_str(), ival);
	} else if (pct < end && (*pct == 'd' || *pct == 'i')) {
	    click_intmax_t ival = 0;
	    if (!IntArg().parse(cp_shift_spacevec(str), ival))
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

int
Script::basic_handler(int, String &str, Element *e, const Handler *h, ErrorHandler *errh)
{
    int what = (uintptr_t) h->read_user_data();

    switch (what) {

    case AR_FIRST:
	str = cp_shift_spacevec(str);
	return 0;

    case AR_NOT: {
	bool x;
	if (!BoolArg().parse(str, x))
	    return errh->error("syntax error");
	str = BoolArg::unparse(!x);
	return 0;
    }

    case ar_and:
    case ar_nand:
    case ar_or:
    case ar_nor: {
	bool zero = (what == ar_and || what == ar_nand), current_value = zero;
	while (current_value == zero && str) {
	    bool x;
	    if (!BoolArg().parse(cp_shift_spacevec(str), x))
		return errh->error("syntax error");
	    current_value = (what == ar_and ? current_value && x : current_value || x);
	}
	if (what == ar_nand || what == ar_nor)
	    current_value = !current_value;
	str = BoolArg::unparse(current_value);
	return 0;
    }

    case ar_if: {
	bool x;
	if (!BoolArg().parse(cp_shift_spacevec(str), x))
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
	str = BoolArg::unparse(answer);
	return 0;
    }

    case ar_now:
	str = Timestamp::now().unparse();
	return 0;

    case ar_random: {
	if (!str)
	    str = String(click_random());
	else {
	    uint32_t n1, n2;
	    bool have_n2 = false;
	    if (Args(e, errh).push_back_words(str)
		.read_mp("N1", n1)
		.read_mp("N2", n2).read_status(have_n2)
		.complete() < 0)
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

#define ARITH_BYTE_ORDER(o) case ar_##o: {		\
	int x;						\
	if (!cp_integer(str, &x))			\
	    return errh->error("syntax error");		\
	str = String( o (x));				\
	return 0;					\
    }
    ARITH_BYTE_ORDER(htons)
    ARITH_BYTE_ORDER(htonl)
    ARITH_BYTE_ORDER(ntohs)
    ARITH_BYTE_ORDER(ntohl)
#undef ARITH_BYTE_ORDER

#if CLICK_USERLEVEL
    case ar_cat:
    case ar_catq: {
	String filename;
	if (!FilenameArg().parse(str, filename))
	    return errh->error("bad FILE");
	int nerrors = errh->nerrors();
	str = file_string(filename, errh);
	if (what == ar_catq)
	    str = cp_quote(str);
	return (errh->nerrors() == nerrors ? 0 : -errno);
    }

    case ar_kill: {
	String word = cp_shift_spacevec(str);
	int32_t signo;
	if (!word || !NameInfo::query_int(NameInfo::T_SIGNO, e, word, &signo)
	    || signo < 0 || signo > 32)
	    return errh->error("bad SIGNO");
	while (str) {
	    pid_t pid = 0;
	    if (!IntArg().parse(cp_shift_spacevec(str), pid))
		return errh->error("bad PID");
	    if (kill(pid, signo) != 0) {
		int e = errno;
		errh->error("kill(%ld, %d): %s", (long) pid, (int) signo, strerror(e));
		return e;
	    }
	}
	return 0;
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

    case ar_length:
	str = String(str.length());
	return 0;

    case ar_unquote:
	str = cp_unquote(str);
	return 0;

    }

    return -1;
}

int
Script::star_write_handler(const String &str, Element *e, void *, ErrorHandler *)
{
    Script *s = static_cast<Script *>(e);
    s->set_handler(str, Handler::OP_READ | Handler::READ_PARAM | Handler::OP_WRITE, step_handler, 0, ST_RUN);
    return Router::hindex(s, str);
}

String
Script::read_export_handler(Element *e, void *user_data)
{
    Script *scr = static_cast<Script *>(e);
    return scr->_vars[(intptr_t) user_data + 1];
}

int
Script::var_handler(int, String &str, Element *e, const Handler *h, ErrorHandler *errh)
{
    Script *s = static_cast<Script *>(e);
    int what = (uintptr_t) h->read_user_data();
    int r = 0;
    String *varval = 0;

    String varname = cp_shift_spacevec(str);
    if (!varname && what == vh_shift)
	varname = "args";
    if (!varname)
	r = errh->error("no variable");
    else if (str && (what == vh_get || what == vh_shift))
	r = errh->error("too many arguments");
    else {
	int i;
	if (varname.equals("args", 4))
	    varval = &s->_run_args;
	else if ((i = s->find_variable(varname, what == vh_set)),
		 i != s->_vars.size())
	    varval = &s->_vars[i + 1];
	else
	    r = errh->error("undefined variable %<%#s%>", varname.c_str());
    }

    if (r >= 0) {
	if (what == vh_get)
	    str = *varval;
	else if (what == vh_set) {
	    *varval = str;
	    str = String();
	} else
	    str = cp_shift_spacevec(*varval);
    } else
	str = String();

    return r;
}

void
Script::add_handlers()
{
    set_handler("step", Handler::OP_WRITE, step_handler, 0, ST_STEP);
    set_handler("goto", Handler::OP_WRITE, step_handler, 0, ST_GOTO);
    set_handler("run", Handler::OP_READ | Handler::READ_PARAM | Handler::OP_WRITE, step_handler, 0, ST_RUN);
    set_handler("add", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_add, 0);
    set_handler("sub", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_sub, 0);
    set_handler("min", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_min, 0);
    set_handler("max", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_max, 0);
    set_handler("mul", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_mul, 0);
    set_handler("div", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_div, 0);
    set_handler("idiv", Handler::OP_READ | Handler::READ_PARAM, arithmetic_handler, ar_idiv, 0);
    set_handler("mod", Handler::OP_READ | Handler::READ_PARAM, modrem_handler, ar_mod, 0);
    set_handler("rem", Handler::OP_READ | Handler::READ_PARAM, modrem_handler, ar_rem, 0);
    set_handler("neg", Handler::OP_READ | Handler::READ_PARAM, negabs_handler, ar_neg, 0);
    set_handler("abs", Handler::OP_READ | Handler::READ_PARAM, negabs_handler, ar_abs, 0);
    set_handler("eq", Handler::OP_READ | Handler::READ_PARAM, compare_handler, AR_EQ, 0);
    set_handler("ne", Handler::OP_READ | Handler::READ_PARAM, compare_handler, AR_NE, 0);
    set_handler("gt", Handler::OP_READ | Handler::READ_PARAM, compare_handler, AR_GT, 0);
    set_handler("ge", Handler::OP_READ | Handler::READ_PARAM, compare_handler, AR_GE, 0);
    set_handler("lt", Handler::OP_READ | Handler::READ_PARAM, compare_handler, AR_LT, 0);
    set_handler("le", Handler::OP_READ | Handler::READ_PARAM, compare_handler, AR_LE, 0);
    set_handler("not", Handler::OP_READ | Handler::READ_PARAM, basic_handler, AR_NOT, 0);
    set_handler("sprintf", Handler::OP_READ | Handler::READ_PARAM, sprintf_handler, AR_SPRINTF, 0);
    set_handler("first", Handler::OP_READ | Handler::READ_PARAM, basic_handler, AR_FIRST, 0);
    set_handler("random", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_random, 0);
    set_handler("and", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_and, 0);
    set_handler("or", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_or, 0);
    set_handler("nand", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_nand, 0);
    set_handler("nor", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_nor, 0);
    set_handler("if", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_if, 0);
    set_handler("in", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_in, 0);
    set_handler("now", Handler::OP_READ, basic_handler, ar_now, 0);
    set_handler("readable", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_readable, 0);
    set_handler("writable", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_writable, 0);
    set_handler("length", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_length, 0);
    set_handler("unquote", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_unquote, 0);
    set_handler("htons", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_htons, 0);
    set_handler("htonl", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_htonl, 0);
    set_handler("ntohs", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_ntohs, 0);
    set_handler("ntohl", Handler::OP_READ | Handler::READ_PARAM, basic_handler, ar_ntohl, 0);
#if CLICK_USERLEVEL
    set_handler("cat", Handler::OP_READ | Handler::READ_PARAM | Handler::READ_PRIVATE, basic_handler, ar_cat, 0);
    set_handler("catq", Handler::OP_READ | Handler::READ_PARAM | Handler::READ_PRIVATE, basic_handler, ar_catq, 0);
    set_handler("kill", Handler::OP_READ | Handler::READ_PARAM | Handler::READ_PRIVATE, basic_handler, ar_kill, 0);
#endif
    set_handler("get", Handler::OP_READ | Handler::READ_PARAM, var_handler, vh_get, 0);
    set_handler("set", Handler::OP_WRITE, var_handler, vh_set, 0);
    set_handler("shift", Handler::OP_READ | Handler::READ_PARAM, var_handler, vh_shift, 0);
    if (_type == type_proxy)
	add_write_handler("*", star_write_handler, 0);
    for (int i = 0; i < _insns.size(); ++i)
	if (_insns[i] == insn_export || _insns[i] == insn_exportq)
	    add_read_handler(_vars[_args[i]], read_export_handler, _args[i]);
}

EXPORT_ELEMENT(Script)
CLICK_ENDDECLS
