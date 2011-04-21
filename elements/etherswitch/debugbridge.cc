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
#include <click/glue.hh>
#include <click/args.hh>
#include "debugbridge.hh"
#include "bridgemessage.hh"
CLICK_DECLS

DebugBridge::DebugBridge()
{
}

DebugBridge::~DebugBridge()
{
}

int
DebugBridge::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("LABEL", _label).complete();
}

Packet *
DebugBridge::simple_action(Packet *p)
{
  const BridgeMessage::wire *msg =
    reinterpret_cast<const BridgeMessage::wire *>(p->data());
  click_chatter("%s",msg->s(_label).c_str());
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DebugBridge)
ELEMENT_REQUIRES(EtherSwitchBridgeMessage)
