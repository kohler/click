/*
 * drivermanager.{cc,hh} -- element manages router driver stopping
 * Eddie Kohler
 *
 * Copyright (c) 2001 ACIRI
 * Copyright (c) 2001 Mazu Networks, Inc.
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

#include <click/config.h>
#include <click/package.hh>
#include "drivermanager.hh"
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
    if (words.size() == 0 || !cp_keyword(words[0], &insn_name))
      errh->error("missing or bad instruction name; should be `INSNNAME [ARG]'");
    
    else if (insn_name == "wait_stop" || insn_name == "wait_pause") {
      unsigned n = 1;
      if (words.size() > 2 || (words.size() == 2 && !cp_unsigned(words[1], &n)))
	errh->error("expected `%s [COUNT]'", insn_name.cc());
      else
	add_insn(INSN_WAIT_STOP, n);
      
    } else if (insn_name == "call") {
      if (words.size() == 2)
	add_insn(INSN_CALL, 0, words[1] + " ''");
      else if (words.size() == 3)
	add_insn(INSN_CALL, 0, words[1] + " " + words[2]);
      else
	errh->error("expected `call ELEMENT.HANDLER [ARG]'");

    } else if (insn_name == "wait") {
      int ms;
      if (words.size() != 2 || !cp_milliseconds(words[1], &ms))
	errh->error("expected `wait TIME'");
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
  // process 'call' instructions
  for (int i = 0; i < _insns.size(); i++)
    if (_insns[i] == INSN_CALL) {
      Element *e;
      int hi;
      String text;
      if (cp_va_space_parse(_args3[i], this, errh,
			    cpWriteHandler, "write handler", &e, &hi,
			    cpString, "data", &text,
			    0) < 0)
	return -1;
      _args[i] = e->eindex();
      _args2[i] = hi;
      _args3[i] = text;
    }
  
  _insn_pos = 0;
  _timer.initialize(this);
  
  int insn = _insns[_insn_pos];
  if (insn == INSN_STOP || insn == INSN_CALL)
    _timer.schedule_now();
  else if (insn == INSN_WAIT)
    _timer.schedule_after_ms(_args[_insn_pos]);
  
  return 0;
}

bool
DriverManager::step_insn()
{
  _insn_pos++;
  
  int insn = _insns[_insn_pos];
  if (insn == INSN_STOP)
    router()->please_stop_driver();
  else if (insn == INSN_WAIT)
    _timer.schedule_after_ms(_args[_insn_pos]);
  else if (insn == INSN_WAIT_STOP)
    _insn_arg = 0;
  else if (insn == INSN_CALL) {
    StringAccum sa;
    Element *e = router()->element(_args[_insn_pos]);
    const Router::Handler &h = router()->handler(_args2[_insn_pos]);
    sa << "While calling `" << e->id() << '.' << h.name << "' from " << declaration() << ":";
    ContextErrorHandler cerrh(ErrorHandler::default_handler(), sa.take_string());
    h.write(_args3[_insn_pos], e, h.write_thunk, &cerrh);
    return true;
  }

  return false;
}

void
DriverManager::handle_stopped_driver()
{
  int insn = _insns[_insn_pos];
  if (insn != INSN_STOP) {
    router()->reserve_driver();
    if (insn == INSN_WAIT_STOP) {
      _insn_arg++;
      if (_insn_arg >= _args[_insn_pos])
	while (step_insn()) ;
    } else if (insn == INSN_WAIT) {
      _timer.unschedule();
      while (step_insn()) ;
    }
  }
}

void
DriverManager::run_scheduled()
{
  // called when a timer expires
  int insn = _insns[_insn_pos];
  if (insn == INSN_WAIT || insn == INSN_CALL)
    while (step_insn()) ;
  else if (insn == INSN_STOP)
    router()->please_stop_driver();
}

EXPORT_ELEMENT(DriverManager)
