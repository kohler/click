#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "dropbroadcasts.hh"
#include <click/glue.hh>

DropBroadcasts::DropBroadcasts()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _drops = 0;
}

DropBroadcasts::~DropBroadcasts()
{
  MOD_DEC_USE_COUNT;
}

void
DropBroadcasts::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

DropBroadcasts *
DropBroadcasts::clone() const
{
  return new DropBroadcasts();
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
  return String(q->drops()) + "\n";
}

void
DropBroadcasts::add_handlers()
{
  add_read_handler("drops", dropbroadcasts_read_drops, 0);
}

EXPORT_ELEMENT(DropBroadcasts)
ELEMENT_MT_SAFE(DropBroadcasts)
