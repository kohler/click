/*
 * stridesched.{cc,hh} -- stride-scheduling packet scheduler
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "strideswitch.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

StrideSwitch::StrideSwitch()
{
  // no MOD_INC_USE_COUNT; rely on StrideSched
}

StrideSwitch::~StrideSwitch()
{
  // no MOD_DEC_USE_COUNT; rely on StrideSched
}

int
StrideSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int x = StrideSched::configure(conf, errh);
  set_ninputs(1);
  set_noutputs(conf.size());
  return x;
}

void
StrideSwitch::push(int, Packet *p)
{
  // go over list until we find a packet, striding as we go
  Client *stridden = _list->_n;
  int o = stridden->id();
  stridden->stride();

  // remove stridden portion from list
  _list->_n = stridden->_n;
  stridden->_n->_p = _list;
  _list->insert(stridden);

  output(o).push(p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(StrideSched)
EXPORT_ELEMENT(StrideSwitch)
