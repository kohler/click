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
    "Random bullshit in a packet, at least 64 bytes long. Well, now it is.";
  unsigned rate = 10;
  int limit = -1;
  bool active = true;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "packet data", &data,
		  cpUnsigned, "sending rate (packets/s)", &rate,
		  cpInteger, "total packet count", &limit,
		  cpBool, "active?", &active,
		  0) < 0)
    return -1;
  
  unsigned one_sec = 1000000 << UGAP_SHIFT;
  if (rate > one_sec) {
    // must have _ugap > 0, so limit rate to 1e6<<UGAP_SHIFT
    errh->error("rate too large; lowered to %u", one_sec);
    rate = one_sec;
  }

  _data = data;
  _rate = rate;
  _ugap = one_sec / (rate > 1 ? rate : 1);
  _limit = (limit >= 0 ? limit : NO_LIMIT);
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
  _count = _sec_count = 0;
  _tv_sec = -1;
  ScheduleInfo::join_scheduler(this, errh);
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
  if (!_active || (_limit != NO_LIMIT && _count >= _limit))
    return;
  
  struct timeval now;
  click_gettimeofday(&now);
  
  if (_tv_sec < 0) {
    _tv_sec = now.tv_sec;
    _sec_count = (now.tv_usec << UGAP_SHIFT) / _ugap;
  } else if (now.tv_sec > _tv_sec) {
    _tv_sec = now.tv_sec;
    if (_sec_count > 0)
      _sec_count -= _rate;
  }

  unsigned need = (now.tv_usec << UGAP_SHIFT) / _ugap;
  if ((int)need > _sec_count) {
#if DEBUG_RATEDSOURCE
    static struct timeval last;
    if (last.tv_sec) {
      struct timeval diff;
      timersub(&now, &last, &diff);
      click_chatter("%d.%06d  (%d)", diff.tv_sec, diff.tv_usec, now.tv_sec);
    }
    last = now;
#endif
    output(0).push(_packet->clone());
    _count++;
    _sec_count++;
  }

  reschedule();
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
    return (rs->_limit != NO_LIMIT ? String(rs->_limit) + "\n" : String("-1\n"));
   case 3:			// active
    return cp_unparse_bool(rs->_active) + "\n";
   case 4:			// count
    return String(rs->_count) + "\n";
   default:
    return "";
  }
}

int
RatedSource::change_param(const String &in_s, Element *e, void *vparam,
			  ErrorHandler *errh)
{
  RatedSource *rs = (RatedSource *)e;
  String s = cp_uncomment(in_s);
  switch ((int)vparam) {

   case 1: {			// rate
     unsigned rate;
     if (!cp_unsigned(s, &rate))
       return errh->error("rate parameter must be integer >= 0");
     unsigned one_sec = 1000000 << UGAP_SHIFT;
     if (rate > one_sec)
       // must have _ugap > 0, so limit rate to 1e6<<UGAP_SHIFT
       return errh->error("rate too large (max is %u)", one_sec);
     rs->set_configuration_argument(1, in_s);
     rs->_rate = rate;
     rs->_ugap = one_sec / (rate > 1 ? rate : 1);
     rs->_tv_sec = -1;
     break;
   }

   case 2: {			// limit
     int limit;
     if (!cp_integer(s, &limit))
       return errh->error("limit parameter must be integer");
     rs->_limit = (limit < 0 ? NO_LIMIT : limit);
     break;
   }
   
   case 3: {			// active
     bool active;
     if (!cp_bool(s, &active))
       return errh->error("active parameter must be boolean");
     rs->_active = active;
     if (!rs->scheduled() && active) {
       rs->_tv_sec = -1;
       rs->reschedule();
     }
     break;
   }

   case 5: {			// reset
     rs->_count = 0;
     rs->_tv_sec = -1;
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
}

EXPORT_ELEMENT(RatedSource)
