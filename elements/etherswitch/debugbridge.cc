/*
 * debugbridge.{cc,hh} -- Ethernet bridge debugging element
 * John Jannotti
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "debugbridge.hh"
#include <click/glue.hh>
#include <click/confparse.hh>

#include "bridgemessage.hh"

DebugBridge::DebugBridge()
  : Element(1, 1)
{
}

DebugBridge::DebugBridge(const String &label)
  : Element(1, 1), _label(label)
{
}

DebugBridge::~DebugBridge()
{
}

DebugBridge *
DebugBridge::clone() const
{
  return new DebugBridge(_label);
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
