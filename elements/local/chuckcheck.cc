/*
 * chuckcheck.{cc,hh} -- element timestamps for Chuck
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "chuckcheck.hh"
#include "straccum.hh"
#include "click_ip.h"

int
ChuckCheck::initialize(ErrorHandler *)
{
  _head = _tail = _first = 0;
  return 0;
}

void
ChuckCheck::count(Packet *p)
{
  Stat &s = _info[_tail];
  click_gettimeofday(&s.time);
  s.saddr = p->ip_header()->ip_src.s_addr;
  _tail = (_tail + 1) % BUCKETS;
  if (_tail == _head) {
    _head = (_head + 1) % BUCKETS;
    _first++;
  }
  if (_tail >= BUCKETS || _head >= BUCKETS)
    click_chatter("fucker!");
}

void
ChuckCheck::push(int, Packet *p)
{
  count(p);
  output(0).push(p);
}

Packet *
ChuckCheck::pull(int)
{
  Packet *p = input(0).pull();
  if (p) count(p);
  return p;
}

String
ChuckCheck::read_handler(Element *e, void *)
{
  ChuckCheck *cc = (ChuckCheck *)e;
  unsigned buf[1 + BUCKETS * 4];
  unsigned j = 1;
  unsigned num = cc->_first;
  unsigned i = cc->_head;

  while (i != cc->_tail) {
    buf[j++] = num++;
    buf[j++] = cc->_info[i].time.tv_sec;
    buf[j++] = cc->_info[i].time.tv_usec;
    buf[j++] = cc->_info[i].saddr;
    i = (i + 1) % BUCKETS;
  }
  if (j > 1 + BUCKETS * 4)
    click_chatter("fucker!");

  buf[0] = num - cc->_first;
  return String((const char *)buf, sizeof(unsigned) * j);
}

void
ChuckCheck::add_handlers()
{
  add_read_handler("info", read_handler, 0);
}

EXPORT_ELEMENT(ChuckCheck)
