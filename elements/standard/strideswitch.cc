/*
 * stridesched.{cc,hh} -- stride-scheduling packet scheduler
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "strideswitch.hh"
#include "confparse.hh"
#include "error.hh"

StrideSwitch::StrideSwitch()
{
}

StrideSwitch::~StrideSwitch()
{
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
