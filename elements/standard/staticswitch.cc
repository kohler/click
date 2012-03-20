/*
 * staticswitch.{cc,hh} -- element routes packets to one output of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "staticswitch.hh"
#include <click/args.hh>
CLICK_DECLS

StaticSwitch::StaticSwitch()
{
}

int
StaticSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _output = 0;
    if (Args(conf, this, errh).read_mp("OUTPUT", _output).complete() < 0)
	return -1;
    if (_output >= noutputs())
	_output = -1;
    return 0;
}

void
StaticSwitch::push(int, Packet *p)
{
  if (_output < 0)
    p->kill();
  else
    output(_output).push(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StaticSwitch)
ELEMENT_MT_SAFE(StaticSwitch)
