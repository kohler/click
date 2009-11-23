/*
 * chuckcheck.{cc,hh} -- element timestamps for Chuck
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "chuckcheck.hh"
#include <click/straccum.hh>
#include <clicknet/ip.h>
CLICK_DECLS

ChuckCheck::ChuckCheck()
{
}

ChuckCheck::~ChuckCheck()
{
}

int
ChuckCheck::initialize(ErrorHandler *)
{
  _head = _tail = _head_id = 0;
  return 0;
}

void
ChuckCheck::count(Packet *p)
{
    Stat &s = _info[_tail];
    s.time.assign_now();
    s.saddr = p->ip_header()->ip_src.s_addr;
    _tail = (_tail + 1) % BUCKETS;
    if (_tail == _head) {
	_head = (_head + 1) % BUCKETS;
	_head_id++;
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
    // XXX multiprocessors

    ChuckCheck *cc = (ChuckCheck *)e;
    String str = String::make_garbage((1 + BUCKETS*4) * sizeof(unsigned));
    if (!str)
	return String::make_stable("out of memory\n");

    unsigned *buf = (unsigned *)(str.mutable_data());
    unsigned j = 1;
    unsigned num = cc->_head_id;
    unsigned head = cc->_head;
    unsigned tail = cc->_tail;

    for (unsigned i = head; i != tail; i = (i + 1) % BUCKETS) {
	buf[j++] = num++;
	buf[j++] = cc->_info[i].time.sec();
	buf[j++] = cc->_info[i].time.usec();
	buf[j++] = cc->_info[i].saddr;
    }

    buf[0] = num - head;
    return str;
}

void
ChuckCheck::add_handlers()
{
    add_read_handler("info", read_handler, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(ChuckCheck)
