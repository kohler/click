/*
 * ratedsource.{cc,hh} -- generates configurable rated stream of packets.
 * Benjie Chen (some code stolen from udpgen.o by Eddie Kohler)
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
#include "router.hh"
#include "scheduleinfo.hh"
#include "glue.hh"


static inline void
sub_timer(struct timeval *diff, struct timeval *tv2, struct timeval *tv1)
{
  diff->tv_sec = tv2->tv_sec - tv1->tv_sec;
  diff->tv_usec = tv2->tv_usec - tv1->tv_usec;
  if (diff->tv_usec < 0) {
    diff->tv_sec--;
    diff->tv_usec += 1000000;
  }
}

RatedSource::RatedSource()
  : _data("Random bullshit in a packet, at least 64 byte long.  Well, now it is."),
    _time(10), _persec(10)
{
  add_output();
}

RatedSource *
RatedSource::clone() const
{
  return new RatedSource;
}

int
RatedSource::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpOptional,
		     cpString, "packet data", &_data,
		     cpUnsigned, "packets per second", &_persec,
		     cpUnsigned, "number of seconds", &_time,
		     0);
}

int
RatedSource::initialize(ErrorHandler *errh)
{
  unsigned int headroom = 14+20+8;
  _total_sent = 0;
  _total = _time * _persec;
  _ngap = 1000000000 / _persec;
  _packet = Packet::make(headroom, (const unsigned char *)_data.data(), 
      			 _data.length(), 
			 Packet::default_tailroom(_data.length()));
#ifndef RR_SCHED
  /* start out with default number of tickets, inflate up to max */
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(1);
#endif
  join_scheduler();
  // ScheduleInfo::join_scheduler(this, errh);

  click_gettimeofday(&_tv1); 
  _tv2 = _tv1; 
  sub_timer(&_diff, &_tv2, &_tv1); 
  
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
  unsigned sent_this_time = 0;

  while (_total_sent < _total) {
    /* how many packets should we have sent by now? */
    unsigned need = _diff.tv_sec * _persec;
    need += (_diff.tv_usec * 1000) / _ngap;

    /* send one if we've fallen behind. */
    if (need > _total_sent) {
      output(0).push(_packet->clone());
      _total_sent++;
      sent_this_time++;
    }

    /* get time at least once through every loop */
    click_gettimeofday(&_tv2);
    sub_timer(&_diff, &_tv2, &_tv1);
   
    reschedule();
    return;
  }
  // router()->please_stop_driver();
}

static String
read_count(Element *e, void *)
{
  RatedSource *is = (RatedSource *)e;
  return String(is->total_sent()) + "\n";
}

void
RatedSource::add_handlers()
{
  add_read_handler("count", read_count, 0);
}

EXPORT_ELEMENT(RatedSource)

