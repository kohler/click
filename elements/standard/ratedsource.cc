/*
 * ratedsource.{cc,hh} -- generates configurable rated stream of packets.
 * Benjie Chen, Eddie Kohler (based on udpgen.o)
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ratedsource.hh"
#include "confparse.hh"
#include "error.hh"
#include "timer.hh"
#include "router.hh"
#include "scheduleinfo.hh"
#include "glue.hh"

RatedSource::RatedSource()
{
  _packet = 0;
  add_output();
}

RatedSource *
RatedSource::clone() const
{
  return new RatedSource;
}

int
RatedSource::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String data = 
    "Random bullshit in a packet, at least 64 byte long.  Well, now it is.";
  int rate = 10;
  int time = -1;
  bool active = true;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "packet data", &data,
		  cpUnsigned, "sending rate (packets/s)", &rate,
		  cpInteger, "sending time", &time,
		  cpBool, "active?", &active,
		  0) < 0)
    return -1;

  _data = data;
  _rate = rate;
  _ugap = (rate ? 1000000 / rate : 1000000);
  _limit = (time >= 0 ? time*rate : -1);
  _active = active;
  
  if (_packet) _packet->kill();
  // note: if you change `headroom', change `click-align'
  unsigned int headroom = 16+20+24;
  _packet = Packet::make(headroom, (const unsigned char *)_data.data(), 
      			 _data.length(), 
			 Packet::default_tailroom(_data.length()));
  
  return 0;
}

int
RatedSource::initialize(ErrorHandler *errh)
{
  _count = 0;
  
#ifndef RR_SCHED
  /* start out with default number of tickets, inflate up to max */
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(ScheduleInfo::DEFAULT);
#endif
  join_scheduler();

  click_gettimeofday(&_start_time);
  _inactive_time = _start_time;
  
  return 0;
}

void
RatedSource::uninitialize()
{
  unschedule();
  _packet->kill();
  _packet = 0;
}

void
RatedSource::run_scheduled()
{
  if (!_active) {
    click_gettimeofday(&_inactive_time);
    return;
  }
  
  if (_limit < 0 || _count < _limit) {
    struct timeval now, diff;
    click_gettimeofday(&now);
    timersub(&now, &_start_time, &diff);
  
    // how many packets should we have sent by now?
    int need = diff.tv_sec * _rate;
    need += diff.tv_usec / _ugap;

    // send one if we've fallen behind.
    if (need > _count) {
      output(0).push(_packet->clone());
      _count++;
    }

    // prevent overflow, we reset
    if (_count > (int)(_rate * 600)) {
      _count = 0;
      _start_time = now;
    }

    reschedule();
  }
}

String
RatedSource::read_param(Element *e, void *vparam)
{
  RatedSource *rs = (RatedSource *)e;
  switch ((int)vparam) {
   case 0:			// data
    return rs->_data;
   case 1:			// rate
    return String(rs->_rate) + "\n";
   case 2:			// limit
    return String(rs->_limit) + "\n";
   case 3:			// active
    return cp_unparse_bool(rs->_active) + "\n";
   case 4:			// count
    return String(rs->_count) + "\n";
   case 6: {			// achieved_rate
     struct timeval now, diff;
     click_gettimeofday(&now);
     timersub(&now, &rs->_start_time, &diff);
     unsigned long r = rs->_count / (diff.tv_sec ? diff.tv_sec : 1);
     return String(r) + " (approximate)\n";
   }
   default:
    return "";
  }
}

int
RatedSource::change_param(const String &in_s, Element *e, void *vparam,
			  ErrorHandler *errh)
{
  RatedSource *rs = (RatedSource *)e;
  String s = cp_subst(in_s);
  switch ((int)vparam) {

   case 1: {			// rate
     int rate;
     if (!cp_integer(s, &rate) || rate < 0)
       return errh->error("rate parameter must be integer >= 0");
     rs->set_configuration_argument(1, s);
     rs->_rate = rate;
     rs->_ugap = (rate ? 1000000 / rate : 1000000);
     if (!rate) break;
     // change _start_time to get a smooth transition
     struct timeval now;
     click_gettimeofday(&now);
     int sec = rs->_count / rate;
     int usec = (rs->_count % rate) * 1000000 / rate;
     struct timeval diff;
     diff.tv_sec = sec + (usec / 1000000);
     diff.tv_usec = usec % 1000000;
     timersub(&now, &diff, &rs->_start_time);
     rs->_inactive_time = rs->_start_time;
     break;
   }

   case 2: {			// limit
     int limit;
     if (!cp_integer(s, &limit))
       return errh->error("limit parameter must be integer");
     rs->_limit = limit;
     break;
   }
   
   case 3: {			// active
     bool active;
     if (!cp_bool(s, &active))
       return errh->error("active parameter must be boolean");
     rs->_active = active;
     if (!rs->scheduled() && active) {
       // change _start_time to avoid flood of packets when turned on
       struct timeval now, diff;
       click_gettimeofday(&now);
       timersub(&now, &rs->_inactive_time, &diff);
       timeradd(&rs->_start_time, &diff, &rs->_start_time);
       rs->reschedule();
     }
     break;
   }

   case 5: {			// reset
     rs->_count = 0;
     click_gettimeofday(&rs->_start_time);
     if (!rs->scheduled() && rs->_active)
       rs->reschedule();
     break;
   }

  }
  return 0;
}

void
RatedSource::add_handlers()
{
  add_read_handler("data", read_param, (void *)0);
  add_write_handler("data", reconfigure_write_handler, (void *)0);
  add_read_handler("rate", read_param, (void *)1);
  add_write_handler("rate", change_param, (void *)1);
  add_read_handler("limit", read_param, (void *)2);
  add_write_handler("limit", change_param, (void *)2);
  add_read_handler("active", read_param, (void *)3);
  add_write_handler("active", change_param, (void *)3);
  add_read_handler("count", read_param, (void *)4);
  add_write_handler("reset", change_param, (void *)5);
  add_read_handler("achieved_rate", read_param, (void *)6);
}

EXPORT_ELEMENT(RatedSource)
