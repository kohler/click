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

Packet *
DropBroadcasts::simple_action(Packet *p)
{
  if(p->mac_broadcast_anno()){
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
    return(0);
  } else {
    return(p);
  }
}

#if 0
inline Packet *
DropBroadcasts::smaction(Packet *p)
{
  if(p->mac_broadcast_anno()){
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
    return(0);
  } else {
    return(p);
  }
}

void
DropBroadcasts::push(int, Packet *p)
{
  if((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
DropBroadcasts::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    p = smaction(p);
  return(p);
}
#endif

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
