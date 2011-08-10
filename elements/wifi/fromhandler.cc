// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdump.{cc,hh} -- element reads packets from handler
 * John Bicket
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
#include "fromhandler.hh"
#include <click/args.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/handlercall.hh>
#include <click/packet_anno.hh>
#include <click/userutils.hh>
CLICK_DECLS

FromHandler::FromHandler()
    :  _task(this)
{
}

FromHandler::~FromHandler()
{

}

int
FromHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read_mp("HANDLER", _handler)
	.complete() < 0)
	return -1;

    _active = false;
    return 0;
}

int
FromHandler::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, errh);
    return 0;
}

u_int8_t
char_to_hex(char c)
{
    switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
    }
    return 0;
}
bool
FromHandler::get_packet()
{
    String s = HandlerCall::call_read(_handler, this, ErrorHandler::default_handler());
    if (s.length() <= 1) {
	return false;
    }
    Timestamp t = Timestamp::now();
    int n = s.find_left('|', 0);
    if (n > 0) {
	String ts = s.substring(0, n-1);
	if (!cp_time(ts, &t)) {
	    click_chatter("parsing time failed\n");
	}
	s = s.substring(n+2, s.length() - n - 1);
    }
    WritablePacket *p = Packet::make(s.length()/2);
    if (!p) {
	return false;
    }
    p->set_timestamp_anno(t);
    memset(p->data(), 0, p->length());
    for (int x = 0; x + 1 < s.length(); x += 2) {
	p->data()[x/2] = (char_to_hex(s[x]) << 4) | char_to_hex(s[x + 1]);
    }
    output(0).push(p);
    return true;
}

bool
FromHandler::run_task(Task *)
{
    if (!_active) {
	return false;
    }
    bool a = get_packet();
    if (a) {
	_task.fast_reschedule();
	return true;
    }
    if (_active) {
	_end = Timestamp::now();
    }
    _active = false;
    return false;
}


void
FromHandler::set_active(bool b) {
    _active = b;
    if (_active) {
	_start = Timestamp::now();
	_end = _start;
	_task.reschedule();
    }
}
enum { H_ACTIVE, H_TIME};

String
FromHandler::read_handler(Element *e, void *thunk)
{
    FromHandler *fd = static_cast<FromHandler *>(e);
    switch ((intptr_t)thunk) {
      case H_ACTIVE:
	return BoolArg::unparse(fd->_active);
    case H_TIME: {
	StringAccum sa;
	sa << fd->_end - fd->_start << "\n";
	return sa.take_string();
    }
      default:
	return "<error>\n";
    }
}

int
FromHandler::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromHandler *fd = static_cast<FromHandler *>(e);
    String s = cp_uncomment(s_in);
    switch ((intptr_t)thunk) {
      case H_ACTIVE: {
	  bool active;
	  if (BoolArg().parse(s, active)) {
	      fd->set_active(active);
	      return 0;
	  } else
	      return errh->error("'active' should be Boolean");
      }
      default:
	return -EINVAL;
    }
}

void
FromHandler::add_handlers()
{
    add_read_handler("time", read_handler, H_TIME);
    add_read_handler("active", read_handler, H_ACTIVE);
    add_write_handler("active", write_handler, H_ACTIVE);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromHandler)
