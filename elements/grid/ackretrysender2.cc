/*
 * ackretrysender2.{cc,hh} -- element buffers packets until acknowledged
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/packet.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include "ackretrysender2.hh"
CLICK_DECLS

ACKRetrySender2::ACKRetrySender2() 
  : Element(2, 1), _timeout(0), _max_retries(0), 
  _num_retries(0), _waiting_packet(0), 
  _verbose (true), _timer(this), _task(this)
{
  MOD_INC_USE_COUNT;
}

ACKRetrySender2::~ACKRetrySender2()
{
  MOD_DEC_USE_COUNT;
}

void
ACKRetrySender2::push(int port, Packet *p)
{
  assert(port == 1);
  check();

  if (!_waiting_packet) {
    // we aren't waiting for ACK
    if (_verbose)
      click_chatter("ACKRetrySender2 %s: got unexpected ACK", id().cc());
    p->kill();
    return;
  }

  // was this response for the packet we have?
  IPAddress src(p->data());
  IPAddress dst(p->data() + 4);
  if (dst != _ip) {
    // no, it wasn't for our packet...
    if (_verbose)
      click_chatter("ACKRetrySender2 %s: got ACK for wrong packet", id().cc());
    p->kill();
    return;
  }
  
  // ahhh, ACK was for us.
  _waiting_packet->kill();
  _waiting_packet = 0;
  _timer.unschedule();  
  p->kill();

  check();
}

bool
ACKRetrySender2::run_task()
{
  check();
  if (_waiting_packet)
    return true;
  
  Packet *p_in = input(0).pull();
  if (!p_in)
    return true;

  WritablePacket *p = p_in->push(8);
  if (!p)
    return true;

  memcpy(p->data(), _ip.data(), 4);
  memcpy(p->data() + 4, p->dst_ip_anno().data(), 4);

  if (_max_retries > 1) {
    _waiting_packet = p->clone();
    _num_retries = 1;
    _timer.schedule_after_ms(_timeout);
  }

  _task.fast_reschedule();
  
  check();

  output(0).push(p);
  return true;
}

int
ACKRetrySender2::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _max_retries = 16;
  _timeout = 10;
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"IP", cpIPAddress, "this node's IP address", &_ip,
			"MAX_RETRIES", cpUnsigned, "max retries per packet (> 0)", &_max_retries,
			"TIMEOUT", cpUnsigned, "time between retried (> 0 msecs)", &_timeout,
			"VERBOSE", cpBool, "noisy?", &_verbose,
			cpEnd);
  
  if (res < 0)
    return res;

  if (_timeout == 0)
    return errh->error("TIMEOUT must be > 0");
  if (_max_retries == 0)
    return errh->error("MAX_RETRIES must be > 0");
  if (!_ip) 
    return errh->error("IP must be specified");

  return 0;
}

int
ACKRetrySender2::initialize(ErrorHandler *errh) 
{
  _timer.initialize(this);
  ScheduleInfo::join_scheduler(this, &_task, errh);
  
  check();
  return 0;
}

void
ACKRetrySender2::run_timer()
{
  assert(_waiting_packet && !_timer.scheduled());
  
  Packet *p = _waiting_packet;
  _num_retries++;
  
  if (_num_retries > _max_retries) {
    _waiting_packet = 0;
    _num_retries = 0;
  }
  else {
    _timer.schedule_after_ms(_timeout);
    _waiting_packet = p->clone();
  }

  check();

  output(0).push(p);
}

void
ACKRetrySender2::check()
{
  // check() should be called *before* most pushes() from element
  // functions, as each push may call back into the element.
  
  // if there is a packet waiting, the timeout timer should be running
  assert(_waiting_packet ? _timer.scheduled() : !_timer.scheduled());

  assert(_num_retries <= _max_retries);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ACKRetrySender2)
