/*
 * debugbridge.{cc,hh} -- Ethernet bridge debugging element
 * John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "debugbridge.hh"

#include <click/glue.hh>
#include <click/confparse.hh>

#include "bridgemessage.hh"

DebugBridge::DebugBridge()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

DebugBridge::~DebugBridge()
{
  MOD_DEC_USE_COUNT;
}

DebugBridge *
DebugBridge::clone() const
{
  return new DebugBridge;
}

int
DebugBridge::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpString, "label", &_label,
		     0);
}

Packet *
DebugBridge::simple_action(Packet *p)
{
  const BridgeMessage::wire *msg =
    reinterpret_cast<const BridgeMessage::wire *>(p->data());
  click_chatter("%s",msg->s(_label).cc());
  return p;
}

EXPORT_ELEMENT(DebugBridge)
ELEMENT_REQUIRES(EtherSwitchBridgeMessage)
