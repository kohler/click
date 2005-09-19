#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include "srlog.hh"
#include "srpacket.hh"
#include "printsr.hh"
CLICK_DECLS

SRLog::SRLog()
{
}

SRLog::~SRLog()
{
}

int
SRLog::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _active = false;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords,
		  "ACTIVE", cpBool, "xxx", &_active,
		  cpEnd) < 0) {
    return -1;
  }


  return 0;
}

Packet *
SRLog::simple_action(Packet *p_in)
{
  if (!_active) {
    return p_in;
  }
  Fo h;
  
  struct timeval now;
  click_gettimeofday(&now);
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p_in->all_user_anno();
  click_ether *eh = (click_ether *) p_in->data();
  
  
  struct srpacket *pk = (struct srpacket *) (eh+1);
  int len = srpacket::len_wo_data(pk->num_links());
  if (len > 500) {
    click_chatter("%{element} header too big %d\n",
		  this, len);
    return p_in;
  }
  h.rate = ceh->rate;
  h.flags = ceh->flags;
  h.retries = ceh->retries;
  memcpy(h.header, pk, len);
  _p.push_back(h);
  _t.push_back(now); 
  return p_in;
}


enum {H_RESET, H_LOG, H_MORE, H_ACTIVE};

static String
SRLog_read_param(Element *e, void *thunk)
{
  SRLog *td = (SRLog *)e;
  switch ((uintptr_t) thunk) {
  case H_LOG: {
    int x = 0;
    StringAccum sa;
    while ( x < 50 && td->_p.size()) {
      struct srpacket *sr = (struct srpacket *) td->_p[0].header;
      String s = PrintSR::sr_to_string(sr);
      sa << td->_t[0];
      if (td->_p[0].flags & WIFI_EXTRA_TX_FAIL) {
	sa << " FAIL";
      } else {
	sa << " TX_OK";
      }

      sa << " rate " << td->_p[0].rate;
      sa << " retries " << td->_p[0].retries;

      sa << " " << s << "\n";

      x++;
      td->_p.pop_front();
      td->_t.pop_front();
    }
    return sa.take_string();
  }
  case H_MORE: return String(td->_p.size()) + "\n";
  case H_ACTIVE: return String(td->_active) + "\n";
    
  default:
    return String();
  }

  

}

static int 
SRLog_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SRLog *f = (SRLog *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_RESET: f->_p.clear(); f->_t.clear(); break;
 case H_ACTIVE: {
    bool active;
    if (!cp_bool(s, &active)) 
      return errh->error("active must be boolean");
    f->_active = active;
    break;
  }
  }

    return 0;

}

void
SRLog::add_handlers()
{
  add_read_handler("more", SRLog_read_param, (void *) H_MORE);
  add_read_handler("log", SRLog_read_param, (void *) H_LOG);
  add_read_handler("active", SRLog_read_param, (void *) H_ACTIVE);

  add_write_handler("reset", SRLog_write_param, (void *) H_RESET);
  add_write_handler("active", SRLog_write_param, (void *) H_ACTIVE);

}
#include <click/dequeue.cc>
template class DEQueue<struct click_wifi_extra>;

CLICK_ENDDECLS
EXPORT_ELEMENT(SRLog)

