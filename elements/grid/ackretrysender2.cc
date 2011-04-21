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
#include <click/args.hh>
#include <click/packet.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/straccum.hh>
#include "ackretrysender2.hh"
CLICK_DECLS

ACKRetrySender2::ACKRetrySender2()
  : _timeout(0), _max_tries(0),
    _num_tries(0), _history_length(500), _waiting_packet(0),
    _verbose (true), _timer(this), _task(this),
    sum_tx(0), num_pkts(0), num_fail(0),
    max_txc(0), min_txc(0)
{
}

ACKRetrySender2::~ACKRetrySender2()
{
}

void
ACKRetrySender2::push(int port, Packet *p)
{
  assert(port == 1);
  check();

  if (!_waiting_packet) {
    // we aren't waiting for ACK
    if (_verbose)
      click_chatter("ACKRetrySender2 %s: got unexpected ACK", name().c_str());
    p->kill();
    return;
  }

  // was this response for the packet we have?
  // IPAddress src(p->data());
  IPAddress dst(p->data() + 4);
  if (dst != _ip) {
    // no, it wasn't for our packet...
    if (_verbose)
      click_chatter("ACKRetrySender2 %s: got ACK for wrong packet", name().c_str());
    p->kill();
    return;
  }

  // ahhh, ACK was for us.
  add_stat(_waiting_packet->timestamp_anno(), _num_tries, true);
  _waiting_packet->kill();
  _waiting_packet = 0;
  _num_tries = 0;
  _timer.unschedule();
  p->kill();

  check();
}

bool
ACKRetrySender2::run_task(Task *)
{
  check();

  _task.fast_reschedule();

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

  if (_max_tries > 1) {
    _waiting_packet = p->clone();
    _num_tries = 1;
    _timer.schedule_after_msec(_timeout);
  }

  check();

  output(0).push(p);
  return true;
}

int
ACKRetrySender2::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _max_tries = 16;
  _timeout = 10;
  _verbose = true;
  _history_length = 500;
  int res = Args(conf, this, errh)
      .read("IP", _ip)
      .read("MAX_TRIES", _max_tries)
      .read("TIMEOUT", _timeout)
      .read("VERBOSE", _verbose)
      .read("HISTORY_LEN", _history_length)
      .complete();

  if (res < 0)
    return res;

  if (_timeout == 0)
    return errh->error("TIMEOUT must be > 0");
  if (_max_tries == 0)
    return errh->error("MAX_TRIES must be > 0");
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
ACKRetrySender2::run_timer(Timer *)
{
  assert(_waiting_packet && !_timer.scheduled());

  Packet *p = _waiting_packet;

  if (_num_tries >= _max_tries) {
    add_stat(p->timestamp_anno(), _num_tries, false);
    _waiting_packet->kill();
    _waiting_packet = p = 0;
    _num_tries = 0;
  }
  else {
    _timer.schedule_after_msec(_timeout);
    _waiting_packet = p->clone();
    _num_tries++;
  }

  check();

  if (p)
    output(0).push(p);
}

void
ACKRetrySender2::check()
{
  // check() should be called *before* most pushes() from element
  // functions, as each push may call back into the element.

  // if there is a packet waiting, the timeout timer should be running
  assert(_waiting_packet ? _timer.scheduled() : !_timer.scheduled());

  // no packet has been sent more than the max number of times
  assert(_num_tries <= _max_tries);

  // any waiting packet has been sent at least once
  assert(_waiting_packet ? _num_tries >= 1 : _num_tries == 0);
}

void
ACKRetrySender2::add_handlers()
{
  add_read_handler("history", print_history, 0);
  add_read_handler("summary", print_summary, 0);
  add_write_handler("clear", clear_history, 0);
}

String
ACKRetrySender2::print_history(Element *e, void *)
{
  ACKRetrySender2 *a = (ACKRetrySender2 *) e;
  StringAccum s;
  for (ACKRetrySender2::HistQ::const_iterator i = a->_history.begin();
       i != a->_history.end(); i++)
    s << i->pkt_time << "\t" << i->num_tx << "\t"
      << (i->success ? "succ" : "fail") << "\n";
  return s.take_string();
}

String
ACKRetrySender2::print_summary(Element *e, void *)
{
  ACKRetrySender2 *a = (ACKRetrySender2 *) e;

  unsigned txc = 0; // scale by 1000 to get fraction
  if (a->num_pkts > 0)
    txc = (1000 * a->sum_tx) / a->num_pkts;

  StringAccum s;
  s << "packets: " << a->num_pkts << "\n"
    << "success: " << a->num_pkts - a->num_fail << "\n"
    << "fail: " << a->num_fail << "\n"
    << "min_txc: " << a->min_txc << "\n"
    << "max_txc: " << a->max_txc << "\n"
    << "avg_txc: " << cp_unparse_real10(txc, 3) << "\n";
  return s.take_string();
}

int
ACKRetrySender2::clear_history(const String &, Element *e, void *, ErrorHandler *)
{
  ACKRetrySender2 *a = (ACKRetrySender2 *) e;
  a->_history.clear();
  return 0;
}

int
ACKRetrySender2::reset_stats(const String &, Element *e, void *, ErrorHandler *)
{
  ACKRetrySender2 *a = (ACKRetrySender2 *) e;
  a->sum_tx = 0;
  a->num_pkts = 0;
  a->num_fail = 0;
  a->max_txc = 0;
  a->min_txc = 0;
  return 0;
}

void
ACKRetrySender2::add_stat(const Timestamp &t, unsigned num_tx, bool succ)
{
  _history.push_back(tx_result_t(t, num_tx, succ));
  while (_history.size() > (int) _history_length)
    _history.pop_front();

  if (num_pkts == 0)
    max_txc = min_txc = num_tx;
  else {
    max_txc = (max_txc < num_tx) ? num_tx : max_txc;
    min_txc = (min_txc > num_tx) ? num_tx : min_txc;
  }

  num_pkts++;
  sum_tx += num_tx;
  if (!succ)
    num_fail++;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ACKRetrySender2)
