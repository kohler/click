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
#include <click/handlercall.hh>
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
DriverManager::add_insn(int insn, int arg, int arg2, const String &arg3)
{
    // first instruction must be WAIT or WAIT_STOP, so add INITIAL if
    // necessary
    if (_insns.size() == 0 && insn > INSN_WAIT_TIME)
	add_insn(INSN_INITIAL, 0);
    _insns.push_back(insn);
    _args.push_back(arg);
    _args2.push_back(arg2);
    _args3.push_back(arg3);
}

int
DriverManager::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (router()->attachment("DriverManager"))
	return errh->error("router has more than one DriverManager element");
    router()->set_attachment("DriverManager", this);

    _check_handlers = true;
    if (cp_va_parse_remove_keywords(conf, 0, this, errh, "CHECK_HANDLERS", cpBool, "check handlers for correctness?", &_check_handlers, cpEnd) < 0)
	return -1;
    
    int before = errh->nerrors();

    for (int i = 0; i < conf.size(); i++) {
	String insn_name = cp_pop_spacevec(conf[i]);
	if (!insn_name)		// ignore as benign
	    /* nada */;

	else if (!cp_keyword(insn_name, &insn_name))
	    errh->error("missing or bad instruction name; should be 'INSNNAME [ARG]'");
    
	else if (insn_name == "wait_stop" || insn_name == "wait_pause"
		 || (insn_name == "wait" && !conf[i])) {
	    unsigned n = 1;
	    if (!conf[i] || cp_unsigned(conf[i], &n))
		add_insn(INSN_WAIT_STOP, n, 0);
	    else
		errh->error("expected '%s [COUNT]'", insn_name.c_str());

	} else if (insn_name == "write" || insn_name == "call") {
	    add_insn(INSN_WRITE, 0, 0, conf[i]);

	} else if (insn_name == "write_skip") {
	    add_insn(INSN_WRITE_SKIP, 0, 0, conf[i]);

	} else if (insn_name == "read" || insn_name == "print") {
	    add_insn(INSN_READ, 0, 0, conf[i]);

#if CLICK_USERLEVEL || CLICK_TOOL
	} else if (insn_name == "save" || insn_name == "append") {
	    Vector<String> args;
	    cp_spacevec(conf[i], args);
	    String filename;
	    if (args.size() >= 2 && cp_filename(args.back(), &filename)) {
		args.pop_back();
		add_insn((insn_name == "save" ? INSN_SAVE : INSN_APPEND), 0, 0, filename + " " + cp_unspacevec(args));
	    } else
		errh->error("expected '%s ELEMENT.HANDLER FILE'", insn_name.c_str());
#endif
	    
	} else if (insn_name == "wait_time" || insn_name == "wait_for" || insn_name == "wait") {
	    Timestamp ts;
	    if (cp_time(conf[i], &ts))
		add_insn(INSN_WAIT_TIME, ts.sec(), ts.subsec());
	    else
		errh->error("expected '%s TIME'", insn_name.cc());

	} else if (insn_name == "stop" || insn_name == "quit") {
	    if (!conf[i]) {
		if (i < conf.size() - 1)
		    errh->warning("arguments after '%s' ignored", insn_name.cc());
		add_insn(INSN_STOP, 0);
	    } else
		errh->error("expected '%s'", insn_name.cc());

#if CLICK_USERLEVEL || CLICK_TOOL
	} else if (insn_name == "loop") {
	    if (!conf[i])
		add_insn(INSN_GOTO, 0);
	    else
		errh->error("expected '%s'", insn_name.cc());
#endif

	} else
	    errh->error("unknown instruction '%s'", insn_name.cc());
    }

    if (_insns.size() == 0)
	add_insn(INSN_WAIT_STOP, 1, 0);
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
    for (int i = 0; i < _insns.size(); i++) {
	int flags;
	String extra;
	switch (_insns[i]) {
	  case INSN_WRITE:
	  case INSN_WRITE_SKIP:
	    flags = HandlerCall::CHECK_WRITE;
	    goto parse;
	  case INSN_READ:
	    flags = HandlerCall::CHECK_READ;
	    goto parse;
#if CLICK_USERLEVEL || CLICK_TOOL
	  case INSN_SAVE:
	  case INSN_APPEND:
	    extra = cp_pop_spacevec(_args3[i]) + " ";
	    flags = HandlerCall::CHECK_READ;
	    goto parse;
#endif
	  parse: {
		HandlerCall hc(_args3[i]);
		if (hc.initialize(flags, this, errh) >= 0) {
		    _args[i] = hc.element()->eindex();
		    _args2[i] = (intptr_t) (hc.handler());
		    _args3[i] = extra + hc.value();
		} else
		    _insns[i] = INSN_IGNORE;
		break;
	    }
	}
    }

    _insn_pos = 0;
    _stopped_count = 0;
    _timer.initialize(this);

    int insn = _insns[_insn_pos];
    assert(insn <= INSN_WAIT_TIME);
    if (insn == INSN_WAIT_TIME)
	_timer.schedule_after(Timestamp(_args[_insn_pos], _args2[_insn_pos]));
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

    switch (insn) {
      case INSN_WRITE: {
	  StringAccum sa;
	  Element *e = router()->element(_args[_insn_pos]);
	  const Handler *h = (const Handler*) _args2[_insn_pos];
	  sa << "While calling '" << h->unparse_name(e) << "' from " << declaration() << ":";
	  ContextErrorHandler cerrh(ErrorHandler::default_handler(), sa.take_string());
	  h->call_write(_args3[_insn_pos], e, &cerrh);
	  return true;
      }
      case INSN_READ: {
	  Element *e = router()->element(_args[_insn_pos]);
	  const Handler *h = (const Handler*) _args2[_insn_pos];
	  ErrorHandler *errh = ErrorHandler::default_handler();
	  String result = h->call_read(e, _args3[_insn_pos], errh);
	  errh->message("%s:\n%s\n", h->unparse_name(e).cc(), result.cc());
	  return true;
      }
#if CLICK_USERLEVEL || CLICK_TOOL
      case INSN_SAVE:
      case INSN_APPEND: {
	  Element *e = router()->element(_args[_insn_pos]);
	  const Handler *h = (const Handler*) _args2[_insn_pos];
	  StringAccum sa;
	  sa << "While calling '" << h->unparse_name(e) << "' from " << declaration() << ":";
	  ContextErrorHandler cerrh(ErrorHandler::default_handler(), sa.take_string());
	  String arg = _args3[_insn_pos];
	  String filename = cp_pop_spacevec(arg);
	  String result = h->call_read(e, arg, &cerrh);
	  FILE *f;
	  if (filename == "-")
	      f = stdout;
	  else if (!(f = fopen(filename.c_str(), (insn == INSN_SAVE ? "wb" : "ab")))) {
	      cerrh.error("%s: %s", filename.c_str(), strerror(errno));
	      return true;
	  }
	  fwrite(result.data(), 1, result.length(), f);
	  if (f != stdout)
	      fclose(f);
	  return true;
      }
      case INSN_GOTO:
	// reset intervening instructions
	for (int i = _args[_insn_pos]; i < _insn_pos; i++)
	    if (_insns[i] == INSN_WAIT_STOP)
		_args2[i] = 0;
	_insn_pos = _args[_insn_pos] - 1;
	return true;
#endif
      case INSN_WRITE_SKIP:
      case INSN_IGNORE:
	return true;
      case INSN_STOP:
	router()->adjust_runcount(-1);
	return false;
      case INSN_WAIT_TIME:
	_timer.schedule_after(Timestamp(_args[_insn_pos], _args2[_insn_pos]));
	return false;
      case INSN_WAIT_STOP:
      default:
	return false;
    }
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
    else if (insn == INSN_WAIT_STOP && _args2[_insn_pos] < _args[_insn_pos]) {
	router()->adjust_runcount(1);
	_args2[_insn_pos]++;
	if (_args2[_insn_pos] < _args[_insn_pos])
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
