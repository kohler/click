// -*- c-basic-offset: 4; related-file-name: "../../include/click/standard/drivermanager.hh" -*-
/*
 * drivermanager.{cc,hh} -- element manages router driver stopping
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
CLICK_DECLS

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
    // first instruction must be WAIT or WAIT_STOP, so add INITIAL if
    // necessary
    if (_insns.size() == 0 && insn > INSN_WAIT_TIME)
	add_insn(INSN_INITIAL, 0);
    _insns.push_back(insn);
    _args.push_back(arg);
    _args2.push_back(0);
    _args3.push_back(arg3);
}

int
DriverManager::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (router()->attachment("DriverManager"))
	return errh->error("router has more than one DriverManager element");
    router()->set_attachment("DriverManager", this);

    _check_handlers = true;
    if (cp_va_parse_remove_keywords(conf, 0, this, errh, "CHECK_HANDLERS", cpBool, "check handlers for correctness?", &_check_handlers, 0) < 0)
	return -1;
    
    int before = errh->nerrors();

    for (int i = 0; i < conf.size(); i++) {
	Vector<String> words;
	cp_spacevec(conf[i], words);

	String insn_name;
	if (words.size() == 0)	// ignore as benign
	    /* nada */;

	else if (!cp_keyword(words[0], &insn_name))
	    errh->error("missing or bad instruction name; should be 'INSNNAME [ARG]'");
    
	else if (insn_name == "wait_stop" || insn_name == "wait_pause"
		 || (insn_name == "wait" && words.size() == 1)) {
	    unsigned n = 1;
	    if (words.size() > 2 || (words.size() == 2 && !cp_unsigned(words[1], &n)))
		errh->error("expected '%s [COUNT]'", insn_name.cc());
	    else
		add_insn(INSN_WAIT_STOP, n);

	} else if (insn_name == "write" || insn_name == "write_skip" || insn_name == "call") {
	    int insn = (insn_name == "write_skip" ? INSN_WRITE_SKIP : INSN_WRITE);
	    if (words.size() == 2)
		add_insn(insn, 0, words[1]);
	    else if (words.size() == 3)
		add_insn(insn, 0, words[1] + " " + cp_unquote(words[2]));
	    else if (words.size() > 3)
		add_insn(insn, 0, cp_unspacevec(words.begin()+1, words.end()));
	    else
		errh->error("expected '%s ELEMENT.HANDLER [ARGS]'", insn_name.cc());

	} else if (insn_name == "read" || insn_name == "print") {
	    if (words.size() == 2)
		add_insn(INSN_READ, 0, words[1]);
	    else
		errh->error("expected '%s ELEMENT.HANDLER'", insn_name.cc());

#if CLICK_USERLEVEL || CLICK_TOOL
	} else if (insn_name == "save" || insn_name == "append") {
	    if (words.size() == 3)
		add_insn((insn_name == "save" ? INSN_SAVE : INSN_APPEND), 0, words[1] + " " + cp_unquote(words[2]));
	    else
		errh->error("expected '%s ELEMENT.HANDLER FILE'", insn_name.c_str());
#endif
	    
	} else if (insn_name == "wait_time" || insn_name == "wait_for" || insn_name == "wait") {
	    uint32_t ms;
	    if (words.size() != 2 || !cp_seconds_as_milli(words[1], &ms))
		errh->error("expected '%s TIME'", insn_name.cc());
	    else
		add_insn(INSN_WAIT_TIME, ms);

	} else if (insn_name == "stop" || insn_name == "quit") {
	    if (words.size() != 1)
		errh->error("expected '%s'", insn_name.cc());
	    else {
		if (i < conf.size() - 1)
		    errh->warning("arguments after '%s' ignored", insn_name.cc());
		add_insn(INSN_STOP, 0);
	    }

	} else
	    errh->error("unknown instruction '%s'", insn_name.cc());
    }

    if (_insns.size() == 0)
	add_insn(INSN_WAIT_STOP, 1);
    add_insn(INSN_STOP, 0);

    return (errh->nerrors() == before ? 0 : -1);
}

int
DriverManager::initialize(ErrorHandler *errh)
{
    if (!_check_handlers)
	errh = ErrorHandler::silent_handler();
    int before = errh->nerrors();
    
    // process 'read' and 'write' instructions
    Element *e;
    for (int i = 0; i < _insns.size(); i++) {
	String text;
	bool read;
	switch (_insns[i]) {
	  case INSN_WRITE:
	  case INSN_WRITE_SKIP:
	    read = false;
	    goto parse;
	  case INSN_READ:
#if CLICK_USERLEVEL || CLICK_TOOL
	  case INSN_SAVE:
	  case INSN_APPEND:
#endif
	    read = true;
	    goto parse;
	  parse:
	    if (cp_handler(cp_pop_spacevec(_args3[i]), this, read, !read, &e, &_args2[i], errh))
		_args[i] = e->eindex();
	    else
		_insns[i] = INSN_IGNORE;
	    break;
	}
    }

    _insn_pos = 0;
    _stopped_count = 0;
    _timer.initialize(this);

    int insn = _insns[_insn_pos];
    assert(insn <= INSN_WAIT_TIME);
    if (insn == INSN_WAIT_TIME)
	_timer.schedule_after_ms(_args[_insn_pos]);
    else if (insn == INSN_INITIAL)
	// get rid of the initial runcount so we get called right away
	router()->adjust_runcount(-1);
    
    return (errh->nerrors() == before || !_check_handlers ? 0 : -1);
}

bool
DriverManager::step_insn()
{
    _insn_pos++;

    int insn = _insns[_insn_pos];

    // change insn if appropriate
    if (insn == INSN_WRITE_SKIP && router()->runcount() >= 0)
	insn = INSN_WRITE;
    
    if (insn == INSN_STOP)
	router()->adjust_runcount(-1);
    else if (insn == INSN_WAIT_TIME)
	_timer.schedule_after_ms(_args[_insn_pos]);
    else if (insn == INSN_WAIT_STOP)
	/* nada */;
    else if (insn == INSN_WRITE) {
	StringAccum sa;
	Element *e = router()->element(_args[_insn_pos]);
	const Router::Handler *h = router()->handler(_args2[_insn_pos]);
	sa << "While calling '" << h->unparse_name(e) << "' from " << declaration() << ":";
	ContextErrorHandler cerrh(ErrorHandler::default_handler(), sa.take_string());
	h->call_write(_args3[_insn_pos], e, &cerrh);
	return true;
    } else if (insn == INSN_READ) {
	Element *e = router()->element(_args[_insn_pos]);
	const Router::Handler *h = router()->handler(_args2[_insn_pos]);
	String result = h->call_read(e);
	ErrorHandler *errh = ErrorHandler::default_handler();
	errh->message("%s:\n%s\n", h->unparse_name(e).cc(), result.cc());
	return true;
#if CLICK_USERLEVEL || CLICK_TOOL
    } else if (insn == INSN_SAVE || insn == INSN_APPEND) {
	Element *e = router()->element(_args[_insn_pos]);
	const Router::Handler *h = router()->handler(_args2[_insn_pos]);
	String result = h->call_read(e);
	FILE *f;
	if (_args3[_insn_pos] == "-")
	    f = stdout;
	else if (!(f = fopen(_args3[_insn_pos].c_str(), (insn == INSN_SAVE ? "wb" : "ab")))) {
	    int saved_errno = errno;
	    StringAccum sa;
	    sa << "While calling '" << h->unparse_name(e) << "' from " << declaration() << ":";
	    ContextErrorHandler cerrh(ErrorHandler::default_handler(), sa.take_string());
	    cerrh.error("%s: %s", _args3[_insn_pos].c_str(), strerror(saved_errno));
	    return true;
	}
	fwrite(result.data(), 1, result.length(), f);
	if (f != stdout)
	    fclose(f);
	return true;
#endif
    } else if (insn == INSN_WRITE_SKIP || insn == INSN_IGNORE)
	return true;

    return false;
}

bool
DriverManager::handle_stopped_driver()
{
    _stopped_count++;

    // process current instruction
    int insn = _insns[_insn_pos];
    assert(insn == INSN_STOP || insn == INSN_WAIT_STOP || insn == INSN_WAIT_TIME || insn == INSN_INITIAL);
    if (insn == INSN_STOP)
	return false;
    else if (insn == INSN_WAIT_STOP && _args[_insn_pos] > 0) {
	router()->adjust_runcount(1);
	_args[_insn_pos]--;
	if (_args[_insn_pos] > 0)
	    return true;
    } else if (insn == INSN_WAIT_TIME)
	_timer.unschedule();
    else if (insn == INSN_INITIAL)
	router()->adjust_runcount(1);

    // process following instructions
    while (step_insn())
	/* nada */;

    return true;
}

void
DriverManager::run_timer()
{
    // called when a timer expires
    assert(_insns[_insn_pos] == INSN_WAIT_TIME);
    while (step_insn())
	/* nada */;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DriverManager)
ELEMENT_HEADER(<click/standard/drivermanager.hh>)
