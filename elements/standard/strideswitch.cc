/*
 * stridesched.{cc,hh} -- stride-scheduling packet scheduler
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include <click/config.h>
#include <click/package.hh>
#include "strideswitch.hh"
#include <click/confparse.hh>
#include <click/error.hh>

StrideSwitch::StrideSwitch()
{
  // no MOD_INC_USE_COUNT; rely on StrideSched
}

StrideSwitch::~StrideSwitch()
{
  // no MOD_DEC_USE_COUNT; rely on StrideSched
}

int
StrideSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
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

ELEMENT_REQUIRES(StrideSched)
EXPORT_ELEMENT(StrideSwitch)
