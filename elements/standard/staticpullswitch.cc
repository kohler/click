/*
 * pullswitch.{cc,hh} -- element routes packets from one input of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "staticpullswitch.hh"
#include <click/confparse.hh>

StaticPullSwitch *
StaticPullSwitch::clone() const
{
  return new StaticPullSwitch;
}

void
StaticPullSwitch::notify_ninputs(int n)
{
  set_ninputs(n);
}

int
StaticPullSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _input = 0;
  if (cp_va_parse(conf, this, errh,
		  cpInteger, "active input", &_input,
		  0) < 0)
    return -1;
  if (_input >= ninputs())
    _input = -1;
  return 0;
}

Packet *
StaticPullSwitch::pull(int)
{
  if (_input < 0)
    return 0;
  else
    return input(_input).pull();
}

EXPORT_ELEMENT(StaticPullSwitch)
