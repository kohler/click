/*
 * ipratemon.{cc,hh} -- counts packets clustered by src/dst addr.
 * Thomer M. Gil
 * Benjie Chen (minor changes)
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
#include "ipratemon.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "click_ip.h"
#include "error.hh"
#include "glue.hh"

IPRateMonitor::IPRateMonitor()
  : Element(1,1), _pb(COUNT_PACKETS), _offset(0), _annobydst(true), 
    _period(1), _thresh(1), _base(NULL)
{
  for (int i=0; i<MAX_PORT_PAIRS; i++)
    _anno[i] = false;
}

IPRateMonitor::~IPRateMonitor()
{
}

int
IPRateMonitor::configure(const String &conf, ErrorHandler *errh)
{
#if IPVERSION == 4
  Vector<String> args;
  cp_argvec(conf, args);

  // Enough args?
  if(args.size() < 6)
    return errh->error("too few arguments.");

  // N of PORT PAIRS
  int n;
  if(!cp_integer(args[0], n) || n < 1 || n > MAX_PORT_PAIRS)
    return errh->error
      ("number of port pairs should be between 1 and %d", MAX_PORT_PAIRS);
  set_ninputs(n);
  set_noutputs(n);

  // PACKETS/BYTES
  if(args[1] == "PACKETS")
    _pb = COUNT_PACKETS;
  else if(args[1] == "BYTES")
    _pb = COUNT_BYTES;
  else
    return errh->error("second argument should be \"PACKETS\" or \"BYTES\".");

  // OFFSET
  if(!cp_integer(args[2], _offset) || _offset < 0)
    return errh->error
      ("offset should be a non-negative integer.");

  // THRESH
  if(!cp_integer(args[3], _thresh) || _thresh < 0)
    return errh->error
      ("thresh should be non-negative integer.");
 
  // PERIOD
  if(!cp_integer(args[4], _period) || _period < 0)
    return errh->error
      ("period should be non-negative integer.");

  // ANNOBY
  if(args[5] == "DST")
    _annobydst = true;
  else if(args[5] == "SRC")
    _annobydst = false;
  else
    return errh->error("ANNOBY should be \"DST\" or \"SRC\".");

  for(int i=6; i < args.size() && i < n+6; i++) {
    if (!cp_bool(args[i], _anno[i-6]))
      return errh->error("ANNO_PORT arguments must be bool.");
  }

  set_resettime();

  // Make _base
  _base = new Stats(_period);
  if(!_base)
    return errh->error("cannot allocate data structure.");

  return 0;
#else
  click_chatter("IPRateMonitor doesn't know how to handle non-IPv4!");
  return -1;
#endif
}

IPRateMonitor *
IPRateMonitor::clone() const
{
  return new IPRateMonitor;
}

void
IPRateMonitor::push(int port, Packet *p)
{
  p = update_rates(p, port);
  if (p) output(port).push(p);
}

Packet *
IPRateMonitor::pull(int port)
{
  Packet *p = input(port).pull();
  if (p) p = update_rates(p, port);
  return p;
}


//
// Recursively destroys tables.
//

IPRateMonitor::Stats::Stats(int period)
{
  for (int i = 0; i < MAX_COUNTERS; i++) {
    counter[i].dst_rate.initialize(period);
    counter[i].src_rate.initialize(period);
  }
}

IPRateMonitor::Stats::~Stats()
{
  for (int i = 0; i < MAX_COUNTERS; i++)
    delete counter[i].next_level;
}

void
IPRateMonitor::Stats::clear(int period)
{
  for (int i = 0; i < MAX_COUNTERS; i++) {
    delete counter[i].next_level;
    counter[i].next_level = 0;
    counter[i].dst_rate.initialize(period);
    counter[i].src_rate.initialize(period);
  }
}

//
// Prints out nice data.
//
String
IPRateMonitor::print(Stats *s, String ip = "")
{
  int jiffs = click_jiffies();
  String ret = "";
  for(int i = 0; i < MAX_COUNTERS; i++) {
    Counter &c = s->counter[i];
    if (c.dst_rate.average() > 0 || c.src_rate.average() > 0) {
      String this_ip;
      if (ip)
        this_ip = ip + "." + String(i);
      else
        this_ip = String(i);
      ret += this_ip;

      c.src_rate.update(0, jiffs);
      c.dst_rate.update(0, jiffs);
      ret += "\t"; 
      ret += cp_unparse_real(c.src_rate.average()*CLICK_HZ,
			     c.src_rate.scale());
      ret += "\t"; 
      ret += cp_unparse_real(c.dst_rate.average()*CLICK_HZ, 
			     c.dst_rate.scale());
      
      ret += "\n";
      if (c.next_level) 
        ret += print(c.next_level, "\t" + this_ip);
    }
  }
  return ret;
}


inline void
IPRateMonitor::set_resettime()
{
  _resettime = click_jiffies();
}

String
IPRateMonitor::look_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor*) e;

  String ret = String(click_jiffies() - me->_resettime) + "\n";
  return ret + me->print(me->_base);
}

String
IPRateMonitor::thresh_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor *) e;
  return String(me->_thresh);
}

String
IPRateMonitor::period_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor *) e;
  return String(me->_period);
}

int
IPRateMonitor::reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  IPRateMonitor* me = (IPRateMonitor *) e;
  me->_base->clear(me->_period);
  me->set_resettime();
  return 0;
}

void
IPRateMonitor::add_handlers()
{
  add_read_handler("thresh", thresh_read_handler, 0);
  add_read_handler("look", look_read_handler, 0);
  add_read_handler("period", period_read_handler, 0);

  add_write_handler("reset", reset_write_handler, 0);
}

EXPORT_ELEMENT(IPRateMonitor)
