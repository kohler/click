/*
 * ipratemon.{cc,hh} -- measures packet rates clustered by src/dst addr.
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
#include "kinkyratemon.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "click_ip.h"
#include "error.hh"
#include "glue.hh"

KinkyRateMonitor::KinkyRateMonitor()
  : _pb(COUNT_PACKETS), _offset(0), _thresh(1), _memmax(0), _ratio(1),
    _base(NULL), _alloced_mem(0), _first(0), _last(0), _prev_deleted(0)
{
  click_chatter("size = %d", sizeof(_qbase));
}

KinkyRateMonitor::~KinkyRateMonitor()
{
}

KinkyRateMonitor *
KinkyRateMonitor::clone() const
{
  return new KinkyRateMonitor;
}

void
KinkyRateMonitor::notify_ninputs(int n)
{
  set_ninputs(n == 1 ? 1 : 2);
  set_noutputs(n == 1 ? 1 : 2);
}

int
KinkyRateMonitor::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String count_what;
  if (cp_va_parse(conf, this, errh,
		  cpString, "monitor type", &count_what,
		  cpUnsigned, "offset", &_offset,
		  cpNonnegFixed, "ratio", 16, &_ratio,
		  cpUnsigned, "threshold", &_thresh,
		  cpUnsigned, "memmax", &_memmax,
		  0) < 0)
    return -1;
  if (count_what == "PACKETS")
    _pb = COUNT_PACKETS;
  else if (count_what == "BYTES")
    _pb = COUNT_BYTES;
  else
    return errh->error("monitor type should be \"PACKETS\" or \"BYTES\"");

  if (_memmax && _memmax < MEMMAX_MIN)
    _memmax = MEMMAX_MIN;
  _memmax *= 1024;      // now bytes

  if (_ratio > 0x10000)
    return errh->error("ratio must be between 0 and 1");

  // Set zoom-threshold as if ratio were 1.
  _thresh = (_thresh * _ratio) >> 16;

  return 0;
}

int
KinkyRateMonitor::initialize(ErrorHandler *errh)
{
  set_resettime();

  // Make _base
  _base = new Stats(this);
  if (!_base)
    return errh->error("cannot allocate data structure.");
  _first = _last = _base;
  return 0;
}

void
KinkyRateMonitor::uninitialize()
{ 
  delete _base;
  _base = 0;
}

void
KinkyRateMonitor::push(int port, Packet *p)
{
  // Only inspect 1 in RATIO packets
  bool ewma = ((unsigned) ((random() >> 5) & 0xffff) <= _ratio);
  update_rates(p, port == 0, ewma);
  output(port).push(p);
}

Packet *
KinkyRateMonitor::pull(int port)
{
  Packet *p = input(port).pull();
  if (p)
    update_rates(p, port == 0, true);
  return p;
}


KinkyRateMonitor::Counter*
KinkyRateMonitor::make_counter(Counter* c[], unsigned char index, MyEWMA *fwd, MyEWMA *rev)
{
  Counter *new_c = NULL;

  // Return NULL if
  // 1. This allocation would violate memory limit
  // 2. Allocation did not succeed
  if ((_memmax && (_alloced_mem + sizeof(Counter) > _memmax)) ||
      !(new_c = c[index] = new Counter))
    return NULL;
  _alloced_mem += sizeof(Counter);

  if (!fwd)
    new_c->fwd_rate.initialize();
  else
    new_c->fwd_rate = *fwd;

  if (!rev)
    new_c->rev_rate.initialize();
  else
    new_c->rev_rate = *rev;
  new_c->next_level = 0;

  return new_c;
}

void
KinkyRateMonitor::forced_fold()
{
#define FOLD_INCREASE_FACTOR    5.0 // percent

  int perc = (int) (((float) _thresh) / FOLD_INCREASE_FACTOR);
  for (int thresh = _thresh; _alloced_mem > _memmax; thresh += perc)
    fold(thresh);
}


//
// Folds branches if threshhold is lower than thresh.
//
// List is unordered and we stop folding as soon as we have freed enough memory.
// This means that a cleanup always starting at _first and proceeding forwards
// is unfair to those in front of the list. It might cause starvation-like
// phenomena. Therefore, choose randomly to traverse forwards or backwards
// through list. 
//
// If there is no memory limitation, then don't fold more than FOLD_FACTOR.
// Otherwise it takes too long.
//
#define FOLD_FACTOR     0.9
void
KinkyRateMonitor::fold(int thresh)
{
  char forward = ((char) random()) & 0x01;
  int now = MyEWMA::now();
  _prev_deleted = _next_deleted = 0;
  Stats *s = (forward ? _first : _last);

  // Don't free to 0 if no memmax defined. Would take too long.
  unsigned memmax;
  if (!(memmax = _memmax))
    memmax = (unsigned) (((float) _alloced_mem) * FOLD_FACTOR);

  do {
start:
    // Don't touch _base. Take next in list.
    if (!s->_parent)
      continue;

    // Shitty code, but avoids an update() and average() call if one of both
    // rates is not below thresh.
    s->_parent->fwd_rate.update(now, 0);
    if (s->_parent->fwd_rate.average() < thresh) {
      s->_parent->rev_rate.update(now, 0);
      if (s->_parent->rev_rate.average() < thresh) {
        delete s;
        if ((_alloced_mem < memmax) ||
           !(s = (forward ? _next_deleted : _prev_deleted)))      // set by ~Stats().
            break;
        goto start;
      }
    }
  } while((s = (forward ? s->_next : s->_prev)));
}


void
KinkyRateMonitor::show_agelist(void)
{
  click_chatter("\n----------------");
  click_chatter("_base = %p, _first: %p, _last = %p\n", _base, _first, _last);
  Stats *prev_r = 0;
  for (Stats *r = _first; r; r = r->_next) {
    click_chatter("r = %p, r->_prev = %p, r->_next = %p", r, r->_prev, r->_next);
    prev_r = r;
  }
}

//
// Prints out nice data.
//
String
KinkyRateMonitor::print(Stats *s, String ip = "")
{
  int jiffs = MyEWMA::now();
  String ret = "";
  for (int i = 0; i < MAX_COUNTERS; i++) {
    Counter *c;
    if (!(c = s->counter[i]))
      continue;

    if (c->rev_rate.average() > 0 || c->fwd_rate.average() > 0) {
      String this_ip;
      if (ip)
        this_ip = ip + "." + String(i);
      else
        this_ip = String(i);
      ret += this_ip;

      c->fwd_rate.update(jiffs, 0);
      c->rev_rate.update(jiffs, 0);
      ret += "\t"; 
      ret += String(c->fwd_rate.average());
      ret += "\t"; 
      ret += String(c->rev_rate.average());
      
      ret += "\n";
      if (c->next_level) 
        ret += print(c->next_level, "\t" + this_ip);
    }
  }
  return ret;
}


String
KinkyRateMonitor::look_read_handler(Element *e, void *)
{
  KinkyRateMonitor *me = (KinkyRateMonitor*) e;

  String ret = String(MyEWMA::now() - me->_resettime) + "\n";
  return ret + me->print(me->_base);
}

String
KinkyRateMonitor::thresh_read_handler(Element *e, void *)
{
  KinkyRateMonitor *me = (KinkyRateMonitor *) e;
  return String(me->_thresh);
}

String
KinkyRateMonitor::mem_read_handler(Element *e, void *)
{
  KinkyRateMonitor *me = (KinkyRateMonitor *) e;
  return String(me->_alloced_mem) + "\n";
}

String
KinkyRateMonitor::memmax_read_handler(Element *e, void *)
{
  KinkyRateMonitor *me = (KinkyRateMonitor *) e;
  return String(me->_memmax) + "\n";
}

int
KinkyRateMonitor::reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  KinkyRateMonitor* me = (KinkyRateMonitor *) e;

  for (int i = 0; i < MAX_COUNTERS; i++) {
    if (me->_base->counter[i]) {
      if (me->_base->counter[i]->next_level)
        delete me->_base->counter[i]->next_level;
      delete me->_base->counter[i];
      me->_base->counter[i] = 0;
    }
  }

  me->set_resettime();
  return 0;
}


int
KinkyRateMonitor::memmax_write_handler
(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  KinkyRateMonitor* me = (KinkyRateMonitor *) e;

  if (args.size() != 1) {
    errh->error("expecting 1 integer");
    return -1;
  }
  int memmax;
  if (!cp_integer(args[0], &memmax)) {
    errh->error("not an integer");
    return -1;
  }

  if (memmax && memmax < MEMMAX_MIN)
    memmax = MEMMAX_MIN;
  me->_memmax = memmax * 1024; // count bytes, not kbytes

  // Fold if necessary
  if (me->_memmax && me->_alloced_mem > me->_memmax)
    me->forced_fold();
  return 0;
}

void
KinkyRateMonitor::add_handlers()
{
  add_read_handler("thresh", thresh_read_handler, 0);
  add_read_handler("look", look_read_handler, 0);
  add_read_handler("mem", mem_read_handler, 0);
  add_read_handler("memmax", memmax_read_handler, 0);

  add_write_handler("reset", reset_write_handler, 0);
  add_write_handler("memmax", memmax_write_handler, 0);
}

//
// Recursively destroys tables.
//
KinkyRateMonitor::Stats::Stats(KinkyRateMonitor *m)
{
  _rm = m;
  _rm->update_alloced_mem(sizeof(*this));
  _parent = 0;
  _next = _prev = 0;

  for (int i = 0; i < MAX_COUNTERS; i++)
    counter[i] = 0;
}



//
// Deletes stats structure cleanly.
//
// Removes all children.
// Removes itself from linked list.
// Tells KinkyRateMonitor where preceding element in age-list is (set_prev).
//
KinkyRateMonitor::Stats::~Stats()
{
  for (int i = 0; i < MAX_COUNTERS; i++) {
    if (counter[i]) {
      delete counter[i]->next_level;    // recursive call
      delete counter[i];
      _rm->update_alloced_mem(-sizeof(Counter));
      counter[i] = 0;
      // counter[i]->next_level = 0 is done 1 recursive step deeper.
    }
  }

  // Untangle _prev
  if (this->_prev) {
    this->_prev->_next = this->_next;
    _rm->set_prev(this->_prev);
  } else {
    _rm->set_first(this->_next);
    if(this->_next)
      this->_next->_prev = 0;
    _rm->set_prev(0);
  }

  // Untangle _next
  if (this->_next) {
    this->_next->_prev = this->_prev;
    _rm->set_next(this->_next);
  } else {
    _rm->set_last(this->_prev);
    if(this->_prev)
      this->_prev->_next = 0;
    _rm->set_next(0);
  }

  // Clear pointer to this in parent
  if (this->_parent)
    this->_parent->next_level = 0;

  _rm->update_alloced_mem(-sizeof(*this));
}


KinkyRateMonitor::counter_row::counter_row()
{
  f = 0;
  for(int i=0; i<MAX_COUNTERS; i++)
    counter[i] = 0;
}


KinkyRateMonitor::counter_row::~counter_row()
{
}


EXPORT_ELEMENT(KinkyRateMonitor)

// template instances
#include "ewma.cc"
