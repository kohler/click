#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include "copyrxstats.hh"
CLICK_DECLS

CopyRXStats::CopyRXStats()
  : Element(1, 1)
{
}

CopyRXStats::~CopyRXStats()
{
}

int
CopyRXStats::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "write offset", &_offset,
		  cpEnd) < 0) {
    return -1;
  }
  return 0;
}

Packet *
CopyRXStats::simple_action(Packet *p_in)
{
  struct click_wifi_extra *eh = (struct click_wifi_extra *) p_in->all_user_anno();
  
  uint8_t rate = eh->rate;
  uint8_t signal = eh->rssi;
  uint8_t noise = eh->silence;
  
  WritablePacket *p = p_in->uniqueify();
  if (!p) { return 0; }
  
  uint8_t *c = p->data();
  c[_offset+0] = rate;
  c[_offset+1] = signal;
  c[_offset+2] = noise;
  
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CopyRXStats)

