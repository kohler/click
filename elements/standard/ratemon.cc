/*
 * ratemon.{cc,hh} -- counts packets clustered by src/dst addr.
 * Thomer M. Gil
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
#include "ratemon.hh"
#include "confparse.hh"
#include "click_ip.h"
#include "error.hh"
#include "glue.hh"

RateMonitor::RateMonitor()
  : Element(1,1), _sd(SRC), _pb(COUNT_PACKETS), _offset(0), _base(NULL)
{
}

RateMonitor::~RateMonitor()
{
}

int
RateMonitor::configure(const String &conf, ErrorHandler *errh)
{
#if IPVERSION == 4
  Vector<String> args;
  cp_argvec(conf, args);

  // Enough args?
  if(args.size() < 4) {
    errh->error("too few arguments");
    return -1;
  }

  // SRC/DST
  if(args[0] == "SRC")
    _sd = SRC;
  else if(args[0] == "DST")
    _sd = DST;
  else {
    errh->error("first argument should be \"SRC\" or \"DST\"");
    return -1;
  }

  // PACKETS/BYTES
  if(args[1] == "PACKETS")
    _pb = COUNT_PACKETS;
  else if(args[1] == "BYTES")
    _pb = COUNT_BYTES;
  else {
    errh->error("second argument should be \"PACKETS\" or \"BYTES\"");
    return -1;
  }

  // OFFSET
  int offset;
  if(!cp_integer(args[2], offset) || offset < 0) {
    errh->error("third argument (OFFSET) should be a non-negative integer");
    return -1;
  }
  _offset = (unsigned int) offset;

  // THRESH
  int len = args[3].length();
  const char *s = args[3].data();
  int i = 0;

  // read threshold
  _thresh = 0;
  while (i < len && isdigit(s[i])) {
    _thresh *= 10;
    _thresh += s[i] - '0';
    i++;
  }
  if (i >= len || s[i] != '/')
    return errh->error("expected `/' in threshold");
  i++;

  // read seconds
  int _threshrate = 0;
  while (i < len && isdigit(s[i])) {
    _threshrate *= 10;
    _threshrate += s[i] - '0';
    i++;
  }

  // First in row is hidden for the outside world. We use to determine when to
  // split entries in the table.
  _rates.push_back(_threshrate);

  // RATES
  int rate;
  for (int i = 4; i < args.size(); i++) {
    String arg = args[i];
    if(cp_integer(arg, rate) && rate > 0)
      _rates.push_back(rate);
    else {
      errh->error("Rates should be a positive integer");
      return -1;
    }
  }

  _no_of_rates = _rates.size();
  set_resettime();

  // Make _base
  _base = new struct _stats;
  if(!_base) {
    errh->error("oops");
    return -1;
  }
  clean(_base);

  return 0;
#else
  click_chatter("RateMonitor doesn't know how to handle non-IPv4!");
  return -1;
#endif
}


RateMonitor *
RateMonitor::clone() const
{
  return new RateMonitor;
}

Packet *
RateMonitor::simple_action(Packet *p)
{
  IPAddress a;

  click_ip *ip = (click_ip *) (p->data() + _offset);
  if(_sd == SRC)
    a = IPAddress(ip->ip_src);
  else
    a = p->dst_ip_anno();

  // Measuring # of packets or # of bytes?
  int val = (_pb == COUNT_PACKETS) ? 1 : ip->ip_len;
  update(a, val);

  return p;
}


//
// Dives in tables based on a and raises the appropriate entry by val.
//
// XXX: Make this interrupt driven.
//
void
RateMonitor::update(IPAddress a, int val)
{
  unsigned int saddr = a.saddr();

  struct _stats *s = _base;
  struct _counter *c = NULL;
  int bitshift;
  for(bitshift = 0; bitshift <= MAX_SHIFT; bitshift += 8) {
    unsigned char byte = (saddr >> bitshift) & 0x000000ff;
    c = &(s->counter[byte]);

    if(c->flags & SPLIT)
      s = c->next_level;
    else
      break;
  }

  // Is vector allocated already?
  if(c->values == NULL) {
    c->values = new Vector<EWMA>;
    c->values->resize(_no_of_rates);
    for(int i = 0; i < _no_of_rates; i++)
      (*(c->values))[i].initialize();
  }

  // For all rates: increase value.
  for(int i = 0; i < _no_of_rates; i++)
    (*(c->values))[i].update(val*(_rates[i]));

  // Did value get larger than THRESH within one second?
  // XXX: not that simple.
  if((*(c->values))[0].average() >= _thresh) {
    if(bitshift < MAX_SHIFT) {
      c->flags |= SPLIT;
      struct _stats *tmp = new struct _stats;
      clean(tmp);
      c->next_level = tmp;
    } else {
      // c->last_update = click_jiffies();
    }
  }
}


void
RateMonitor::destroy(_stats *s)
{
  for(int i = 0; i < 256; i++) {
    if(s->counter[i].flags & SPLIT) {
      destroy(s->counter[i].next_level);
      delete s->counter[i].next_level;
      s->counter[i].next_level = NULL;
    }

    if(s->counter[i].values != NULL)
      delete s->counter[i].values;
  }
}

void
RateMonitor::clean(_stats *s)
{
  int jiffs = click_jiffies();
  for(int i = 0; i < 256; i++) {
    s->counter[i].flags = 0;
    s->counter[i].next_level = NULL;
    s->counter[i].last_update = jiffs;
  }
}

String
RateMonitor::print(_stats *s, String ip = "")
{
  String ret = "";
  for(int i = 0; i < 256; i++) {
    String this_ip;
    if(ip)
      this_ip = ip + "." + String(i);
    else
      this_ip = String(i);


    if(s->counter[i].flags & SPLIT) {
      ret += this_ip + "\t*\n";
      ret += print(s->counter[i].next_level, "\t" + this_ip);
    } else if(s->counter[i].values != 0) {
      ret += this_ip;

      // First rate is hidden
      for(int j = 1; j < _no_of_rates; j++)
        ret += "\t" + String((*(s->counter[i].values))[j].average());
      ret += "\n";
    }
  }
  return ret;
}


inline void
RateMonitor::set_resettime()
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
RateMonitor::look_read_handler(Element *e, void *)
{
  RateMonitor *me = (RateMonitor*) e;

  String ret = String(click_jiffies() - me->_resettime) + "\n";
  return ret + me->print(me->_base);
}


String
RateMonitor::thresh_read_handler(Element *e, void *)
{
  RateMonitor *me = (RateMonitor *) e;
  return String(me->_thresh) + "\n";
}

String
RateMonitor::what_read_handler(Element *e, void *)
{
  RateMonitor *me = (RateMonitor *) e;
  return (me->_pb == COUNT_PACKETS ? "PACKETS\n" : "BYTES\n");
}


int
RateMonitor::thresh_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  RateMonitor* me = (RateMonitor *) e;

  if(args.size() != 1) {
    errh->error("expecting 1 integer");
    return -1;
  }
  int thresh;
  if(!cp_integer(args[0], thresh)) {
    errh->error("not an integer");
    return -1;
  }
  me->_thresh = thresh;
  return 0;
}



int
RateMonitor::reset_write_handler(const String &, Element *e, void *, ErrorHandler *)
{
  RateMonitor* me = (RateMonitor *) e;
  me->destroy(me->_base);
  me->set_resettime();
  return 0;
}


void
RateMonitor::add_handlers()
{
  add_read_handler("thresh", thresh_read_handler, 0);
  add_write_handler("thresh", thresh_write_handler, 0);

  add_read_handler("what", what_read_handler, 0);
  add_read_handler("look", look_read_handler, 0);

  add_write_handler("reset", reset_write_handler, 0);
}

#include "vector.cc"
template class Vector<EWMA>;

EXPORT_ELEMENT(RateMonitor)
