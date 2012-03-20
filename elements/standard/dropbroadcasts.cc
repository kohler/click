#include <click/config.h>
#include "dropbroadcasts.hh"
#include <click/glue.hh>
CLICK_DECLS

DropBroadcasts::DropBroadcasts()
{
  _drops = 0;
}

void
DropBroadcasts::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("DropBroadcasts: dropped a packet");
  _drops++;
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

Packet *
DropBroadcasts::simple_action(Packet *p)
{
  if (p->packet_type_anno() == Packet::BROADCAST || p->packet_type_anno() == Packet::MULTICAST) {
    drop_it(p);
    return 0;
  } else
    return p;
}

static String
dropbroadcasts_read_drops(Element *f, void *)
{
  DropBroadcasts *q = (DropBroadcasts *)f;
  return String(q->drops());
}

void
DropBroadcasts::add_handlers()
{
  add_read_handler("drops", dropbroadcasts_read_drops, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DropBroadcasts)
ELEMENT_MT_SAFE(DropBroadcasts)
