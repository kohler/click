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
  : Element(1,1), _sd(SRC), _pb(COUNT_PACKETS), _offset(0), _base(NULL)
{
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
  if(args.size() < 4 || args.size() > 4+MAX_NRATES)
    return errh->error("too few or too many arguments.");

  // SRC/DST
  if(args[0] == "SRC")
    _sd = SRC;
  else if(args[0] == "DST")
    _sd = DST;
  else
    return errh->error("first argument should be \"SRC\" or \"DST\".");

  // PACKETS/BYTES
  if(args[1] == "PACKETS")
    _pb = COUNT_PACKETS;
  else if(args[1] == "BYTES")
    _pb = COUNT_BYTES;
  else
    return errh->error("second argument should be \"PACKETS\" or \"BYTES\".");

  // OFFSET
  int offset;
  if(!cp_integer(args[2], offset) || offset < 0)
    return errh->error
      ("third argument (OFFSET) should be a non-negative integer.");
  _offset = (unsigned int) offset;

  // THRESH
  // First in row is hidden for the outside world. We use to determine when to
  // split entries in the table.
  if(!set_thresh(args[3]))
    return errh->error("fourth argument (THRESH) should be int/int.");


  // RATES
  int rate;
  int i;
  for (i = 4; i < args.size(); i++) {
    String arg = args[i];
    if(cp_integer(arg, rate) && rate > 0)
      // _rates[0] used for thresh
      _rates[i-4+1] = rate;
    else
      return errh->error("rates should be a positive integer.");
  }

  // _rates[0] used for thresh
  _no_of_rates = i-4+1;
  set_resettime();

  // Make _base
  _base = new struct _stats;
  if(!_base)
    return errh->error("cannot allocate data structure.");
  clean(_base);

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

Packet *
IPRateMonitor::simple_action(Packet *p)
{
  IPAddress a;

  click_ip *ip = (click_ip *) (p->data() + _offset);
  if(_sd == SRC)
    a = IPAddress(ip->ip_src);
  else
    a = IPAddress(ip->ip_dst);

  // Measuring # of packets or # of bytes?
  int val = (_pb == COUNT_PACKETS) ? 1 : ip->ip_len;
  update(a, val);

  return p;
}

//
// Recursively destroys tables.
//
void
IPRateMonitor::destroy(_stats *s)
{
  int jiffs = click_jiffies();
  for(int i = 0; i < MAX_COUNTERS; i++) {
    if(s->counter[i].flags & SPLIT) {
      destroy(s->counter[i].next_level);
      delete s->counter[i].next_level;
      s->counter[i].next_level = NULL;
    }
    s->counter[i].flags = CLEAN;
    s->counter[i].last_update = jiffs;
  }
}

//
// Cleans entry
//
void
IPRateMonitor::clean(_stats *s)
{
  int jiffs = click_jiffies();
  for(int i = 0; i < MAX_COUNTERS; i++) {
    s->counter[i].flags = CLEAN;
    s->counter[i].next_level = NULL;
    s->counter[i].last_update = jiffs;
  }
}


bool
IPRateMonitor::set_thresh(String str)
{
  int len = str.length();
  const char *s = str.data();
  int i = 0;

  // read threshold
  int tmp_thresh = 0;
  while (i < len && isdigit(s[i])) {
    tmp_thresh *= 10;
    tmp_thresh += s[i] - '0';
    i++;
  }
  if (i >= len || s[i] != '/')
    return false;
  i++;

  // read seconds
  int threshrate = 0;
  while (i < len && isdigit(s[i])) {
    threshrate *= 10;
    threshrate += s[i] - '0';
    i++;
  }

  if(threshrate <= 0)
    return false;

  _thresh = tmp_thresh;
  _rates[0] = threshrate;

  return true;
}


//
// Prints out nice data.
//
String
IPRateMonitor::print(_stats *s, String ip = "")
{
  String ret = "";
  for(int i = 0; i < MAX_COUNTERS; i++) {
    bool nonzero = false;
    String rates = "";
    String this_ip;
    if(ip)
      this_ip = ip + "." + String(i);
    else
      this_ip = String(i);


    if (s->counter[i].flags != CLEAN) {
      int j;
  
      // First rate is hidden
      for(j = 1; j < _no_of_rates; j++) {
	// Update the rate first, so we have correct info
        s->counter[i].values[j].update(0);
	if (s->counter[i].values[j].average() > 0)
	  nonzero = true;
      }

      if (nonzero) {
        for(j = 1; j < _no_of_rates; j++) {
          rates += "\t";
	  rates += cp_unparse_real
	    (s->counter[i].values[j].average() * CLICK_HZ,
	     s->counter[i].values[j].scale());
        }
      }
    }
    
    if (nonzero) {
      ret += this_ip + rates + "\n";
      if(s->counter[i].flags & SPLIT) 
	ret += print(s->counter[i].next_level, "\t" + this_ip);
    }
  }
  return ret;
}


inline void
IPRateMonitor::set_resettime()
{
  _resettime = click_jiffies();
}


// First line: number of jiffies since last reset.
// Other lines:
// tabs address ['*' number | number]
//
// tabs = 0 - 3 tabs
// address = string of form v[.w[.x[.y]]] denoting a (partial) IP address
// number = integer denoting the value associated with this IP address group
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
  return String(me->_thresh) + "/" + String(me->_rates[0]) + "\n";
}


String
IPRateMonitor::srcdst_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor *) e;
  return (me->_sd == SRC) ? "SRC\n" : "DST\n";
}


String
IPRateMonitor::what_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor *) e;
  return (me->_pb == COUNT_PACKETS ? "PACKETS\n" : "BYTES\n");
}


String
IPRateMonitor::rates_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor *) e;
  String ret = "";

  ret += String(me->_no_of_rates-1);
  for(int i = 1; i < me->_no_of_rates; i++) {
    ret += "\t";
    ret += String(me->_rates[i]);
  }

  return ret + "\n";
}


int
IPRateMonitor::thresh_write_handler
(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  IPRateMonitor* me = (IPRateMonitor *) e;

  if(args.size() != 1) {
    errh->error("expecting 1 string");
    return -1;
  }
  if(!me->set_thresh(args[0])) {
    errh->error("error parsing threshold. should be int/int.");
    return -1;
  }
  return 0;
}



int
IPRateMonitor::reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  IPRateMonitor* me = (IPRateMonitor *) e;
  me->destroy(me->_base);
  me->set_resettime();
  return 0;
}


void
IPRateMonitor::add_handlers()
{
  add_read_handler("thresh", thresh_read_handler, 0);
  add_write_handler("thresh", thresh_write_handler, 0);

  add_read_handler("srcdst", srcdst_read_handler, 0);
  add_read_handler("what", what_read_handler, 0);
  add_read_handler("look", look_read_handler, 0);
  add_read_handler("rates", rates_read_handler, 0);

  add_write_handler("reset", reset_write_handler, 0);
}

EXPORT_ELEMENT(IPRateMonitor)
