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
DebugBridge::configure(const String &conf, Router *, ErrorHandler *)
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
