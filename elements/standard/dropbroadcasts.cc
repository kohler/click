#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "dropbroadcasts.hh"
#include "glue.hh"

DropBroadcasts::DropBroadcasts()
{
  _drops = 0;
  add_input();
  add_output();
}

DropBroadcasts::~DropBroadcasts()
{
}

DropBroadcasts *
DropBroadcasts::clone() const
{
  return new DropBroadcasts();
}

void
DropBroadcasts::drop_it(Packet *p)
{
  if(_drops == 0){
    unsigned char *q = p->data();
    static char buf[512];
    int i, j=0;
    for(i = 0; i < 20; i++){
      sprintf(buf+j, "%02x ", q[i]);
      j += 3;
    }
    click_chatter("DropBroadcasts: dropped a packet: %s", buf);
  }                    
  _drops++;
  p->kill();
}

Packet *
DropBroadcasts::simple_action(Packet *p)
{
  if(p->mac_broadcast_anno()){
    drop_it(p);
    return(0);
  } else {
    return(p);
  }
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
