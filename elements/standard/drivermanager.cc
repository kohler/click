// -*- c-basic-offset: 4; related-file-name: "../../include/click/standard/drivermanager.hh" -*-
/*
 * drivermanager.{cc,hh} -- element manages router driver stopping
 * Eddie Kohler
 *
 * Copyright (c) 2001 ACIRI
 * Copyright (c) 2001 Mazu Networks, Inc.
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
#include <click/standard/drivermanager.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/straccum.hh>

DriverManager::DriverManager()
    : _timer(this)
{
    MOD_INC_USE_COUNT;
}

DriverManager::~DriverManager()
{
    MOD_DEC_USE_COUNT;
}

void
DriverManager::add_insn(int insn, int arg, const String &arg3)
{
    // first instruction must be WAIT or WAIT_STOP, so add a fake WAIT if
    // necessary
    if (_insns.size() == 0 && insn != INSN_WAIT && insn != INSN_WAIT_STOP)
	add_insn(INSN_WAIT, 0);
    _insns.push_back(insn);
    _args.push_back(arg);
    _args2.push_back(0);
    _args3.push_back(arg3);
}

int
DriverManager::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    if (router()->attachment("DriverManager"))
	return errh->error("router has more than one DriverManager element");
    router()->set_attachment("DriverManager", this);

    int before = errh->nerrors();

    for (int i = 0; i < conf.size(); i++) {
	Vector<String> words;
	cp_spacevec(conf[i], words);

	String insn_name;
	if (words.size() == 0)	// ignore as benign
	    /* nada */;

	else if (!cp_keyword(words[0], &insn_name))
	    errh->error("missing or bad instruction name; should be `INSNNAME [ARG]'");
    
	else if (insn_name == "wait_stop" || insn_name == "wait_pause"
		 || (insn_name == "wait" && words.size() == 1)) {
	    unsigned n = 1;
	    if (words.size() > 2 || (words.size() == 2 && !cp_unsigned(words[1], &n)))
		errh->error("expected `%s [COUNT]'", insn_name.cc());
	    else
		add_insn(INSN_WAIT_STOP, n);

	} else if (insn_name == "write" || insn_name == "write_skip" || insn_name == "call") {
	    int insn = (insn_name == "write_skip" ? INSN_WRITE_SKIP : INSN_WRITE);
	    if (words.size() == 2)
		add_insn(insn, 0, words[1] + " ''");
	    else if (words.size() == 3)
		add_insn(insn, 0, words[1] + " " + words[2]);
	    else
		errh->error("expected `%s ELEMENT.HANDLER [ARG]'", insn_name.cc());

	} else if (insn_name == "read" || insn_name == "print") {
	    if (words.size() == 2)
		add_insn(INSN_READ, 0, words[1]);
	    else
		errh->error("expected `%s ELEMENT.HANDLER'", insn_name.cc());

	} else if (insn_name == "wait_for" || insn_name == "wait") {
	    uint32_t ms;
	    if (words.size() != 2 || !cp_seconds_as_milli(words[1], &ms))
		errh->error("expected `%s TIME'", insn_name.cc());
	    else
		add_insn(INSN_WAIT, ms);

	} else if (insn_name == "stop" || insn_name == "quit") {
	    if (words.size() != 1)
		errh->error("expected `%s'", insn_name.cc());
	    else {
		if (i < conf.size() - 1)
		    errh->warning("arguments after `%s' ignored", insn_name.cc());
		add_insn(INSN_STOP, 0);
	    }

	} else
	    errh->error("unknown instruction `%s'", insn_name.cc());
    }

    if (_insns.size() == 0)
	add_insn(INSN_WAIT_STOP, 1);
    add_insn(INSN_STOP, 0);

    return (errh->nerrors() == before ? 0 : -1);
}

int
DriverManager::initialize(ErrorHandler *errh)
{
    // process 'read' and 'write' instructions
    Element *e;
    int hi;
    for (int i = 0; i < _insns.size(); i++)
	if (_insns[i] == INSN_WRITE || _insns[i] == INSN_WRITE_SKIP) {
	    String text;
	    if (cp_va_space_parse(_args3[i], this, errh,
				  cpWriteHandler, "write handler", &e, &hi,
				  cpString, "data", &text,
				  0) < 0)
		return -1;
	    _args[i] = (e ? e->eindex() : -1);
	    _args2[i] = hi;
	    _args3[i] = text;
	} else if (_insns[i] == INSN_READ) {
	    if (cp_va_space_parse(_args3[i], this, errh,
				  cpReadHandler, "read handler", &e, &hi,
				  0) < 0)
		return -1;
	    _args[i] = (e ? e->eindex() : -1);
	    _args2[i] = hi;
	}

    _insn_pos = 0;
    _stopped_count = 0;
    _timer.initialize(this);

    int insn = _insns[_insn_pos];
    assert(insn == INSN_WAIT || insn == INSN_WAIT_STOP);
    if (insn == INSN_WAIT)
	_timer.schedule_after_ms(_args[_insn_pos]);
    
    return 0;
}

bool
DriverManager::step_insn()
{
    _insn_pos++;

    int insn = _insns[_insn_pos];
    if (insn == INSN_WRITE_SKIP && *(router()->driver_runcount_ptr()) >= 0)
	insn = INSN_WRITE;
    if (insn == INSN_STOP)
	router()->please_stop_driver();
    else if (insn == INSN_WAIT)
	_timer.schedule_after_ms(_args[_insn_pos]);
    else if (insn == INSN_WAIT_STOP)
	_insn_arg = 0;
    else if (insn == INSN_WRITE) {
	StringAccum sa;
	Element *e = router()->element(_args[_insn_pos]);
	const Router::Handler &h = router()->handler(_args2[_insn_pos]);
	sa << "While calling `" << h.unparse_name(e) << "' from " << declaration() << ":";
	ContextErrorHandler cerrh(ErrorHandler::default_handler(), sa.take_string());
	h.call_write(_args3[_insn_pos], e, &cerrh);
	return true;
    } else if (insn == INSN_READ) {
	Element *e = router()->element(_args[_insn_pos]);
	const Router::Handler &h = router()->handler(_args2[_insn_pos]);
	String result = h.call_read(e);
	ErrorHandler *errh = ErrorHandler::default_handler();
	errh->message("%s:\n%s\n", h.unparse_name(e).cc(), result.cc());
	return true;
    } else if (insn == INSN_WRITE_SKIP)
	return true;

    return false;
}

void
DriverManager::handle_stopped_driver()
{
    _stopped_count++;
    int insn = _insns[_insn_pos];
    if (insn != INSN_STOP && insn != INSN_WRITE_SKIP)
	router()->reserve_driver();
    if (insn == INSN_WAIT_STOP) {
	_insn_arg++;
	if (_insn_arg >= _args[_insn_pos])
	    while (step_insn())
		/* nada */;
    } else if (insn == INSN_WAIT) {
	_timer.unschedule();
	while (step_insn())
	    /* nada */;
    }
}

void
DriverManager::run_scheduled()
{
    // called when a timer expires
    assert(_insns[_insn_pos] == INSN_WAIT);
    while (step_insn())
	/* nada */;
}

EXPORT_ELEMENT(DriverManager)
ELEMENT_HEADER(<click/standard/drivermanager.hh>)
