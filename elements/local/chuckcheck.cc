/*
 * chuckcheck.{cc,hh} -- element timestamps for Chuck
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
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
  unsigned *buf = new unsigned[1 + BUCKETS * 4];
  if (!buf)
    return String("out of memory");
  
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

  buf[0] = num - cc->_first;
  return String::claim_string((const char *)buf, sizeof(unsigned) * j);
}

void
ChuckCheck::add_handlers()
{
  add_read_handler("info", read_handler, 0);
}

EXPORT_ELEMENT(ChuckCheck)
