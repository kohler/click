/*
 * storecycles.{cc,hh} -- store cycle count
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "storecycles.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

StoreCycles::StoreCycles()
{
  add_input();
  add_output();
  _sum = _pkt_cnt = 0;
}

StoreCycles::~StoreCycles()
{
}

StoreCycles *
StoreCycles::clone() const
{
  return new StoreCycles();
}

int
StoreCycles::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "frist index", &_idx1,
		     cpUnsigned, "second index", &_idx2,
		     cpEnd);
}

int
StoreCycles::initialize(ErrorHandler *errh)
{
  if (_idx1 > 4 || _idx2 > 4)
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
  unsigned long c1 = p->cycle_anno(_idx1);
  unsigned long c2 = p->cycle_anno(_idx2);
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
StoreCycles::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("cycles", StoreCycles_read_cycles, 0);
}

EXPORT_ELEMENT(StoreCycles)
