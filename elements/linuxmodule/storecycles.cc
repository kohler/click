/*
 * storecycles.{cc,hh} -- store cycle count
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#include <click/config.h>
#include <click/package.hh>
#include "storecycles.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

StoreCycles::StoreCycles()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _sum = _pkt_cnt = 0;
}

StoreCycles::~StoreCycles()
{
  MOD_DEC_USE_COUNT;
}

StoreCycles *
StoreCycles::clone() const
{
  return new StoreCycles();
}

int
StoreCycles::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "first index", &_idx1,
		     cpUnsigned, "second index", &_idx2,
		     cpEnd);
}

int
StoreCycles::initialize(ErrorHandler *errh)
{
  if (_idx1 > 3 || _idx2 > 3)
    return errh->error("indexes must be between 0 and 3");
  return 0;
}

void
StoreCycles::uninitialize()
{
}

inline void
StoreCycles::smaction(Packet *p)
{
  cycles_t c1 = p->cycle_anno(_idx1);
  cycles_t c2 = p->cycle_anno(_idx2);
  if (c2 != 0 && c1 != 0) {
    _sum += c2 - c1;
    _pkt_cnt++;
  }
}

void
StoreCycles::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
StoreCycles::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    smaction(p);
  return(p);
}

static String
StoreCycles_read_cycles(Element *f, void *)
{
  StoreCycles *s = (StoreCycles *)f;
  return
    String(s->_sum) + " cycles\n" +
    String(s->_pkt_cnt) + " packets\n";
}

void
StoreCycles::add_handlers()
{
  add_read_handler("cycles", StoreCycles_read_cycles, 0);
}

ELEMENT_REQUIRES(linuxmodule false)
EXPORT_ELEMENT(StoreCycles)
