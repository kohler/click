/*
 * debugbridge.{cc,hh} -- Ethernet bridge debugging element
 * John Jannotti
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
#include "debugbridge.hh"
#include "glue.hh"
#include "confparse.hh"

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
DebugBridge::configure(const String &conf, ErrorHandler *)
{
  _label = conf;
  return 0;
}

Packet *
DebugBridge::simple_action(Packet *p)
{
  BridgeMessage::wire* msg =
    reinterpret_cast<BridgeMessage::wire*>(p->data());
  click_chatter("%s",msg->s(_label).cc());
  return p;
}

EXPORT_ELEMENT(DebugBridge)
ELEMENT_REQUIRES(EtherSwitchBridgeMessage)
