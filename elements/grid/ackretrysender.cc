/*
 * ackretrysender.{cc,hh} -- element buffers packets until acknowledged
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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
#include "ackretrysender.hh"
#include "ackresponder.hh"

ACKRetrySender::ACKRetrySender() 
  : Element(2, 1), _timeout(0), _max_retries(0), 
  _num_retries(0), _waiting_packet(0), 
  _verbose (true), _timer(static_timer_hook, this), _task(this)
{
  MOD_INC_USE_COUNT;
}

ACKRetrySender::~ACKRetrySender()
{
  MOD_DEC_USE_COUNT;
}

void
ACKRetrySender::push(int port, Packet *p)
{
  assert(port == 1);
  check();

  if (!_waiting_packet) {
    // we aren't waiting for ACK
    if (_verbose)
      click_chatter("ACKRetrySender %s: got unexpected ACK", id().cc());
    p->kill();
    return;
  }

  click_ether *e_ack = (click_ether *) p->data();
  if (ntohs(e_ack->ether_type) != ETHERTYPE_GRID_ACK) {
    click_chatter("ACKRetrySender %d: got non-ACK packet on second input", id().cc());
    p->kill();
    return;
  }

  // was this response for the packet we have?
  click_ether *e_waiting = (click_ether *) _waiting_packet->data();
  if (memcmp(e_ack->ether_shost, e_waiting->ether_dhost, 6) || 
      memcmp(e_ack->ether_dhost, e_waiting->ether_shost, 6)) {
    // no, it wasn't for our packet...
    if (_verbose)
      click_chatter("ACKRetrySender %s: got ACK for wrong packet", id().cc());
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

void
ACKRetrySender::run_scheduled()
{
  check();

  if (_waiting_packet)
    return;
  
  Packet *p = input(0).pull();

  if (_max_retries > 1) {
    _waiting_packet = p->clone();
    _num_retries = 1;
    _timer.schedule_after_ms(_timeout);
  }

  _task.fast_reschedule();
  
  check();

  output(0).push(p);
}

int
ACKRetrySender::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _max_retries = 16;
  _timeout = 10;
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
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

  return 0;
}

int
ACKRetrySender::initialize(ErrorHandler *errh) 
{
  _timer.initialize(this);
  ScheduleInfo::join_scheduler(this, &_task, errh);
  
  check();
  return 0;
}

void
ACKRetrySender::timer_hook()
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
ACKRetrySender::check()
{
  // check() should be called *before* most pushes() from element
  // functions, as each push may call back into the element.
  
  // if there is a packet waiting, the timeout timer should be running
  assert(_waiting_packet ? _timer.scheduled() : !_timer.scheduled());

  assert(_num_retries <= _max_retries);
}

EXPORT_ELEMENT(ACKRetrySender)
