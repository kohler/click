/*
 * ratepowercontrol.{cc,hh} -- element encapsulates packet in Grid data header
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <elements/wifi/ratepowercontrol.hh>
#include <click/confparse.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/glue.hh>

CLICK_DECLS

RatePowerControl::RatePowerControl()
  : Element(2, 1)
{
  MOD_INC_USE_COUNT;
}

RatePowerControl::~RatePowerControl()
{
  MOD_DEC_USE_COUNT;
}

RatePowerControl *
RatePowerControl::clone() const
{
  return new RatePowerControl;
}

int
RatePowerControl::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpEtherAddress, "Ethernet address", &_eth,
		  0) < 0) {
    return -1;
  }

  return 0;
}

int
RatePowerControl::initialize(ErrorHandler *)
{
  return 0;
}


Packet * 
RatePowerControl::simple_action(Packet *p)
{
  return p;
}

void
RatePowerControl::add_handlers()
{
  add_default_handlers(true);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RatePowerControl)

