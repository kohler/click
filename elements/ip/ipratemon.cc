/*
 * ipratemon.{cc,hh} -- measures packet rates clustered by src/dst addr.
 * Thomer M. Gil
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "ipratemon.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/sync.hh>
#include <click/llrpc.h>
CLICK_DECLS

IPRateMonitor::IPRateMonitor()
  : _count_packets(true), _anno_packets(true),
    _thresh(1), _memmax(0), _ratio(1),
    _lock(0), _base(0), _alloced_mem(0), _first(0),
    _last(0), _prev_deleted(0)
{
}

IPRateMonitor::~IPRateMonitor()
{
}

int
IPRateMonitor::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String count_what;
    _memmax = 0;
    _anno_packets = true;
    if (Args(conf, this, errh)
	.read_mp("TYPE", WordArg(), count_what)
	.read_mp("RATIO", FixedPointArg(16), _ratio)
	.read_mp("THRESH", _thresh)
	.read_p("MEMORY", _memmax)
	.read_p("ANNO", _anno_packets)
	.complete() < 0)
	return -1;
  if (count_what.upper() == "PACKETS")
    _count_packets = true;
  else if (count_what.upper() == "BYTES")
    _count_packets = false;
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
IPRateMonitor::initialize(ErrorHandler *errh)
{
  set_resettime();

  _lock = new Spinlock();
  if (!_lock)
    return errh->error("cannot create spinlock.");

  // Make _base
  _base = new Stats(this);
  if (!_base)
    return errh->error("cannot allocate data structure.");
  _first = _last = _base;
  return 0;
}

void
IPRateMonitor::cleanup(CleanupStage)
{
  delete _base;
  delete _lock;
  _base = 0;
  _lock = 0;
}

void
IPRateMonitor::push(int port, Packet *p)
{
  // Only inspect 1 in RATIO packets
  bool ewma = ((unsigned) ((click_random() >> 5) & 0xffff) <= _ratio);
  _lock->acquire();
  update_rates(p, port == 0, ewma);
  _lock->release();
  output(port).push(p);
}

Packet *
IPRateMonitor::pull(int port)
{
  Packet *p = input(port).pull();
  if (p) {
    bool ewma = ((unsigned) ((click_random() >> 5) & 0xffff) <= _ratio);
    _lock->acquire();
    update_rates(p, port == 0, ewma);
    _lock->release();
  }
  return p;
}


IPRateMonitor::Counter*
IPRateMonitor::make_counter(Stats *s, unsigned char index, MyEWMA *rate)
{
  Counter *c = NULL;

  // Return NULL if
  // 1. This allocation would violate memory limit
  // 2. Allocation did not succeed
  if (_memmax && (_alloced_mem + sizeof(Counter) > _memmax))
      return NULL;
  if (rate)
      c = s->counter[index] = new Counter(*rate);
  else
      c = s->counter[index] = new Counter;
  if (!c)
      return NULL;
  _alloced_mem += sizeof(Counter);

  return c;
}

void
IPRateMonitor::forced_fold()
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
IPRateMonitor::fold(int thresh)
{
  char forward = (char) click_random(0, 1);
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
    s->_parent->fwd_and_rev_rate.update(0);
    if (s->_parent->fwd_and_rev_rate.scaled_average(0) < thresh) {
      if (s->_parent->fwd_and_rev_rate.scaled_average(1) < thresh) {
        delete s;
        if ((_alloced_mem < memmax) ||
           !(s = (forward ? _next_deleted : _prev_deleted))) // set by ~Stats().
            break;
        goto start;
      }
    }
  } while((s = (forward ? s->_next : s->_prev)));
}


void
IPRateMonitor::show_agelist(void)
{
  click_chatter("\n----------------");
  click_chatter("_base = %p, _first: %p, _last = %p\n", _base, _first, _last);
  for (Stats *r = _first; r; r = r->_next)
    click_chatter("r = %p, r->_prev = %p, r->_next = %p", r, r->_prev, r->_next);
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

  for (int i = 0; i < MAX_COUNTERS; i++)
    counter[i] = 0;
}



//
// Deletes stats structure cleanly.
//
// Removes all children.
// Removes itself from linked list.
// Tells IPRateMonitor where preceding element in age-list is (set_prev).
//
IPRateMonitor::Stats::~Stats()
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

//
// Prints out nice data.
//
String
IPRateMonitor::print(Stats *s, String ip)
{
  String ret = "";
  for (int i = 0; i < Stats::MAX_COUNTERS; i++) {
    Counter *c;
    if (!(c = s->counter[i]))
      continue;

    if (c->fwd_and_rev_rate.scaled_average(1) > 0 ||
	c->fwd_and_rev_rate.scaled_average(0) > 0) {
      String this_ip;
      if (ip)
        this_ip = ip + "." + String(i);
      else
        this_ip = String(i);
      ret += this_ip;

      c->fwd_and_rev_rate.update(0);
      ret += "\t";
      ret += c->fwd_and_rev_rate.unparse_rate(0);
      ret += "\t";
      ret += c->fwd_and_rev_rate.unparse_rate(1);

      ret += "\n";
      if (c->next_level)
        ret += print(c->next_level, "\t" + this_ip);
    }
  }
  return ret;
}


String
IPRateMonitor::look_read_handler(Element *e, void *)
{
  IPRateMonitor *me = (IPRateMonitor*) e;

  String ret = String(EWMAParameters::epoch() - me->_resettime) + "\n";

  if (me->_lock->attempt()) {
    ret = ret + me->print(me->_base);
    me->_lock->release();
    return ret;
  } else {
    return ret + "unavailable\n";
  }
}

int
IPRateMonitor::reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  IPRateMonitor* me = (IPRateMonitor *) e;

  me->_lock->acquire();
  for (int i = 0; i < Stats::MAX_COUNTERS; i++) {
    if (me->_base->counter[i]) {
      if (me->_base->counter[i]->next_level)
        delete me->_base->counter[i]->next_level;
      delete me->_base->counter[i];
      me->_base->counter[i] = 0;
    }
  }
  me->set_resettime();
  me->_lock->release();

  return 0;
}


int
IPRateMonitor::memmax_write_handler
(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  IPRateMonitor* me = (IPRateMonitor *) e;

  if (args.size() != 1) {
    errh->error("expecting 1 integer");
    return -1;
  }
  int memmax;
  if (!IntArg().parse(args[0], memmax)) {
    errh->error("not an integer");
    return -1;
  }

  if (memmax && memmax < (int)MEMMAX_MIN)
    memmax = MEMMAX_MIN;

  me->_lock->acquire();
  me->_memmax = memmax * 1024; // count bytes, not kbytes

  // Fold if necessary
  if (me->_memmax && me->_alloced_mem > me->_memmax)
    me->forced_fold();
  me->_lock->release();

  return 0;
}


int
IPRateMonitor::anno_level_write_handler
(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  IPRateMonitor* me = (IPRateMonitor *) e;
  IPAddress a;
  int level, when;

  if (args.size() != 3) {
    errh->error("expecting 3 arguments");
    return -1;
  }

  if (!IPAddressArg().parse(args[0], a)) {
    errh->error("not an IP address");
    return -1;
  }
  if (!IntArg().parse(args[1], level) || !(level >= 0 && level < 4)) {
    errh->error("2nd argument specifies a level, between 0 and 3, to annotate");
    return -1;
  }
  if (!IntArg().parse(args[2], when) || when < 1) {
    errh->error("3rd argument specifies when this rule expires, must be > 0");
    return -1;
  }

  when *= EWMAParameters::epoch_frequency();
  when += EWMAParameters::epoch();

  me->_lock->acquire();
  unsigned addr = a.addr();
  me->set_anno_level(addr, static_cast<unsigned>(level),
                     static_cast<unsigned>(when));
  me->_lock->release();
  return 0;
}


void
IPRateMonitor::add_handlers()
{
  add_data_handlers("thresh", Handler::OP_READ, &_thresh);
  add_read_handler("look", look_read_handler);
  add_data_handlers("mem", Handler::OP_READ, &_alloced_mem);
  add_data_handlers("memmax", Handler::OP_READ, &_memmax);

  add_write_handler("anno_level", anno_level_write_handler);
  add_write_handler("reset", reset_write_handler, 0, Handler::BUTTON);
  add_write_handler("memmax", memmax_write_handler);
}

int
IPRateMonitor::llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_IPRATEMON_LEVEL_FWD_AVG
      || command == CLICK_LLRPC_IPRATEMON_LEVEL_REV_AVG) {

    // Data	: int data[256]
    // Incoming : data[0] is the level to drill down; 0 is top level,
    //		  values between 0 and 3 inclusive are valid
    //		  data[1] is the network-byte-order IP address to drill down
    //		  on; it is irrelevant if data[0] == 0
    //		  data[2..255] are ignored
    // Outgoing : If there is no data at that level, returns -EAGAIN.
    //		  If there is data at that level, then puts the forward
    //		  or reverse rate for each of the 256 buckets at that level
    //		  into data[]. If a bucket has no rate, puts -1 into that
    //		  element of data[].

    int which = (command == CLICK_LLRPC_IPRATEMON_LEVEL_FWD_AVG ? 0 : 1);
    unsigned *udata = (unsigned *)data;
    unsigned level, ipaddr;
    if (CLICK_LLRPC_GET(level, udata) < 0)
      return -EFAULT;
    if (CLICK_LLRPC_GET(ipaddr, udata + 1) < 0)
      return -EFAULT;
    if (level > 3)
      return -EINVAL;

    int averages[256];

    _lock->acquire();

    // ipaddr is in network order
    Stats *s = _base;
    ipaddr = ntohl(ipaddr);
    for (int bitshift = 24; bitshift > 0 && level > 0; bitshift -= 8, level--) {
      unsigned char b = (ipaddr >> bitshift) & 255;
      if (!s->counter[b] || !s->counter[b]->next_level) {
        _lock->release();
	return -EAGAIN;
      }
      s = s->counter[b]->next_level;
    }

    unsigned freq = EWMAParameters::epoch_frequency();

    for (int i = 0; i < 256; i++) {
      if (s->counter[i]) {
	s->counter[i]->fwd_and_rev_rate.update(0);
	averages[i] =
	  (s->counter[i]->fwd_and_rev_rate.scaled_average(which) * freq) >> scale;
      }
      else
	averages[i] = -1;
    }

    _lock->release();

    return CLICK_LLRPC_PUT_DATA(data, averages, sizeof(averages));

  }

  else if (command == CLICK_LLRPC_IPRATEMON_FWD_N_REV_AVG) {

    // Data	: int data[9]
    // Incoming : data[0] is the network-byte-order IP address to drill down
    //            on. data[1...8] are ignored.
    // Outgoing : data[0] specifies how many level of rates are returned. for
    //            example, if user request data for 18.26.4.10, and only rates
    //            upto 18.26.4 is available, returns 3. data[1...9] contain
    //            rates, starting with the highest order byte (e.g. 18).

    unsigned *udata = (unsigned *)data;
    unsigned ipaddr;
    if (CLICK_LLRPC_GET(ipaddr, udata) < 0)
      return -EFAULT;

    int averages[9];
    int n = 0;

    _lock->acquire();

    // ipaddr is in network order
    Stats *s = _base;
    ipaddr = ntohl(ipaddr);
    for (int bitshift = 24; bitshift >= 0; bitshift -= 8) {
      unsigned char b = (ipaddr >> bitshift) & 255;
      if (!s->counter[b])
	break;

      unsigned freq = EWMAParameters::epoch_frequency();
      s->counter[b]->fwd_and_rev_rate.update(0);
      averages[n*2+1] =
	(s->counter[b]->fwd_and_rev_rate.scaled_average(0) * freq) >> scale;
      averages[n*2+2] =
	(s->counter[b]->fwd_and_rev_rate.scaled_average(1) * freq) >> scale;
      n++;

      if (!s->counter[b]->next_level)
	break;
      s = s->counter[b]->next_level;
    }

    _lock->release();

    averages[0] = n;
    return CLICK_LLRPC_PUT_DATA(data, averages, sizeof(averages));
  }

  else if (command == CLICK_LLRPC_IPRATEMON_SET_ANNO_LEVEL) {

    // Data	: int data[3]
    // Incoming : data[0] is the network-byte-order IP address. data[1] is the
    //            level at which annotations and expansion should stop.
    //            data[2] is the duration of this rule.
    // Outgoing : nada.

    unsigned *udata = (unsigned *)data;
    unsigned ipaddr, level, when;

    if (CLICK_LLRPC_GET(ipaddr, udata) < 0)
      return -EFAULT;

    if (CLICK_LLRPC_GET(level, udata+1) < 0 || level > 3)
      return -EFAULT;

    if (CLICK_LLRPC_GET(when, udata+2) < 0 || when < 1)
      return -EFAULT;

    when *= EWMAParameters::epoch_frequency();
    when += EWMAParameters::epoch();

    _lock->acquire();
    set_anno_level(ipaddr, static_cast<unsigned>(level),
	           static_cast<unsigned>(when));
    _lock->release();
    return 0;
  }

  else
    return Element::llrpc(command, data);
}

EXPORT_ELEMENT(IPRateMonitor)
ELEMENT_REQUIRES(userlevel)
CLICK_ENDDECLS
