/*
 * ipratemon.{cc,hh} -- counts packets clustered by src/dst addr.
 * Thomer M. Gil
 * Benjie Chen, Eddie Kohler (minor changes)
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
  : _pb(COUNT_PACKETS), _offset(0), _thresh(1), _base(NULL), _alloced_mem(0),
    _first(0), _last(0), _packet_counter(0), _prev_deleted(0)
{
}

IPRateMonitor::~IPRateMonitor()
{
}

IPRateMonitor *
IPRateMonitor::clone() const
{
  return new IPRateMonitor;
}

void
IPRateMonitor::notify_ninputs(int n)
{
  set_ninputs(n == 1 ? 1 : 2);
  set_noutputs(n == 1 ? 1 : 2);
}

int
IPRateMonitor::configure(const String &conf, ErrorHandler *errh)
{
  String count_what;
  if (cp_va_parse(conf, this, errh,
		  cpString, "monitor type", &count_what,
		  cpUnsigned, "offset", &_offset,
		  cpUnsigned, "threshold", &_thresh,
		  cpUnsigned, "refresh", &_refresh,
		  0) < 0)
    return -1;
  if (count_what == "PACKETS")
    _pb = COUNT_PACKETS;
  else if (count_what == "BYTES")
    _pb = COUNT_BYTES;
  else
    return errh->error("monitor type should be \"PACKETS\" or \"BYTES\"");
  return 0;
}

int
IPRateMonitor::initialize(ErrorHandler *errh)
{
  set_resettime();

  // Make _base
  _base = new Stats(this);
  if(!_base)
    return errh->error("cannot allocate data structure.");
  _first = _last = _base;
  return 0;
}

void
IPRateMonitor::uninitialize()
{ 
  delete _base;
  _base = 0;
}

void
IPRateMonitor::push(int port, Packet *p)
{
  update_rates(p, port == 0);
  output(port).push(p);
}

Packet *
IPRateMonitor::pull(int port)
{
  Packet *p = input(port).pull();
  if (p)
    update_rates(p, port == 0);
  return p;
}


void
IPRateMonitor::fold(void)
{
  // Go backwards through the age list, starting at longest non-touched.
  _prev_deleted = 0;
  Stats *s = _last;
  while(true) {
    int now = MyEWMA::now();
    if(!s->_parent) {
      assert(s == _base);
      _prev_deleted = s->_prev;
      goto done;
    }

    // Below threshold?
    s->_parent->fwd_rate.update(now, 0);
    s->_parent->rev_rate.update(now, 0);
    if(s->_parent->fwd_rate.average() < _thresh &&
       s->_parent->rev_rate.average() < _thresh)
      delete s;   // sets _prev_deleted
    else
      _prev_deleted = s->_prev;

done:
    if(!(s = _prev_deleted))
      break;
  }
}


void
IPRateMonitor::show_agelist(void)
{
  click_chatter("SHOW ALL LIST\n----------------");
  click_chatter("_first: %p, _last = %p\n", _first, _last);
  Stats *prev_r = 0;
  for(Stats *r = _first; r; r = r->_next) {
    click_chatter("r = %p, r->_prev = %p, r->_next = %p", r, r->_prev, r->_next);
    prev_r = r;
  }

  if(prev_r != _last)
    click_chatter("FUCKY");
}


//
// Recursively destroys tables.
//
IPRateMonitor::Stats::Stats(IPRateMonitor *m)
{
  _rm = m;
  _rm->update_alloced_mem(sizeof(*this));
  _parent = 0;
  _next = _prev = 0;

  for (int i = 0; i < MAX_COUNTERS; i++) {
    counter[i].fwd_rate.initialize();
    counter[i].rev_rate.initialize();
    counter[i].next_level = 0;
  }
}

IPRateMonitor::Stats::Stats(IPRateMonitor *m, 
			    const MyEWMA &fwd, const MyEWMA &rev)
{
  _rm = m;
  _rm->update_alloced_mem(sizeof(*this));
  _parent = 0;
  _next = _prev = 0;

  for (int i = 0; i < MAX_COUNTERS; i++) {
    counter[i].fwd_rate = fwd;
    counter[i].rev_rate = rev;
    counter[i].next_level = 0;
  }
}



// Deletes stats structure cleanly.
//
// Removes all children.
// Removes itself from linked list.
// Tells IPRateMonitor where preceding element in list is.
IPRateMonitor::Stats::~Stats()
{
  for (int i = 0; i < MAX_COUNTERS; i++)
    if(counter[i].next_level)
      delete counter[i].next_level;

  // Untangle _prev
  if(this->_prev) {
    this->_prev->_next = this->_next;
    _rm->set_prev(this->_prev);
  } else {
    _rm->set_first(this->_next);
    _rm->set_prev(0);
  }

  // Untangle _next
  if(this->_next)
    this->_next->_prev = this->_prev;
  else
    _rm->set_last(this->_prev);

  // Unset pointer to this in parent
  if(this->_parent)
    this->_parent->next_level = 0;
  _rm->update_alloced_mem(-sizeof(*this));
}

void
IPRateMonitor::Stats::clear()
{
  for (int i = 0; i < MAX_COUNTERS; i++) {
    if(counter[i].next_level) {
      delete counter[i].next_level;
      counter[i].next_level = 0;
    }

    counter[i].rev_rate.initialize();
    counter[i].fwd_rate.initialize();
  }
}

//
// Prints out nice data.
//
String
IPRateMonitor::print(Stats *s, String ip = "")
{
  int jiffs = MyEWMA::now();
  String ret = "";
  for(int i = 0; i < MAX_COUNTERS; i++) {
    Counter &c = s->counter[i];
    if (c.rev_rate.average() > 0 || c.fwd_rate.average() > 0) {
      String this_ip;
      if (ip)
        this_ip = ip + "." + String(i);
      else
        this_ip = String(i);
      ret += this_ip;

      c.fwd_rate.update(jiffs, 0);
      c.rev_rate.update(jiffs, 0);
      ret += "\t"; 
      ret += String(c.fwd_rate.average());
      //      ret += cp_unparse_real(c.fwd_rate.average()*CLICK_HZ, c.fwd_rate.scale);
      ret += "\t"; 
      ret += String(c.rev_rate.average());
      //      ret += cp_unparse_real(c.rev_rate.average()*CLICK_HZ, c.rev_rate.scale);
      
      ret += "\n";
      if (c.next_level) 
        ret += print(c.next_level, "\t" + this_ip);
    }
  }
  return ret;
}


String
IPRateMonitor::look_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor*) e;

  String ret = String(MyEWMA::now() - me->_resettime) + "\n";
  return ret + me->print(me->_base);
}

String
IPRateMonitor::thresh_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor *) e;
  return String(me->_thresh);
}

String
IPRateMonitor::mem_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor *) e;
  return String(me->_alloced_mem) + "\n";
}

String
IPRateMonitor::refresh_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor *) e;
  return String(me->_refresh) + "\n";
}

int
IPRateMonitor::reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  IPRateMonitor* me = (IPRateMonitor *) e;
  me->_base->clear();
  me->set_resettime();
  return 0;
}


int
IPRateMonitor::refresh_write_handler
(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  IPRateMonitor* me = (IPRateMonitor *) e;

  if(args.size() != 1) {
    errh->error("expecting 1 integer");
    return -1;
  }
  int refresh;
  if(!cp_integer(args[0], refresh)) {
    errh->error("not an integer");
    return -1;
  }
  me->_refresh = refresh;
  return 0;
}

void
IPRateMonitor::add_handlers()
{
  add_read_handler("thresh", thresh_read_handler, 0);
  add_read_handler("look", look_read_handler, 0);
  add_read_handler("mem", mem_read_handler, 0);
  add_read_handler("refresh", refresh_read_handler, 0);

  add_write_handler("reset", reset_write_handler, 0);
  add_write_handler("refresh", refresh_write_handler, 0);
}

EXPORT_ELEMENT(IPRateMonitor)

// template instances
#include "ewma.cc"
