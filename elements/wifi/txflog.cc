#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include "txflog.hh"
CLICK_DECLS

TXFLog::TXFLog()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

TXFLog::~TXFLog()
{
  MOD_DEC_USE_COUNT;
}

int
TXFLog::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpKeywords,
		  cpEnd) < 0) {
    return -1;
  }


  return 0;
}

Packet *
TXFLog::simple_action(Packet *p_in)
{
  struct click_wifi_extra e;
  memcpy(&e, p_in->all_user_anno(), sizeof(struct click_wifi_extra));
  _p.push_back(e);      
  _t.push_back(p_in->timestamp_anno());
  return p_in;
}


enum {H_RESET, H_LOG, H_MORE};

static String
TXFLog_read_param(Element *e, void *thunk)
{
  TXFLog *td = (TXFLog *)e;
  switch ((uintptr_t) thunk) {
  case H_LOG: {
    int x = 0;
    StringAccum sa;
    while ( x < 50 && td->_p.size()) {
      struct click_wifi_extra *ceh = &td->_p[0];
      sa << td->_t[0];
      sa << " flags";
      if (ceh->flags & WIFI_EXTRA_TX_FAIL) {
	sa << " WIFI_FAIL";
      }

      if (ceh->flags & WIFI_EXTRA_TX_USED_ALT_RATE) {
	sa << " WIFI_ALT_RATE";
      }

      sa << " retries " << (int) ceh->retries;


      sa << " rate " << (int)ceh->rate;
      sa << " rate1 " << (int)ceh->rate1;
      sa << " rate2 " << (int)ceh->rate2;
      sa << " rate3 " << (int)ceh->rate3;


      sa << " max_retries " << (int)ceh->max_retries;
      sa << " max_retries1 " << (int)ceh->max_retries1;
      sa << " max_retries2 " << (int)ceh->max_retries2;
      sa << " max_retries3 " << (int)ceh->max_retries3;

      sa << " virt_col " << (int) ceh->virt_col;
      sa << " rssi " << (int)ceh->rssi;
      //      sa << " silence " << (int)ceh->silence;
      //sa << " power " << (int)ceh->power;

      sa << "\n";


      x++;
      td->_p.pop_front();
      td->_t.pop_front();
    }
    return sa.take_string();
  }
  case H_MORE: return String(td->_p.size()) + "\n";
    
  default:
    return String();
  }

  

}

static int 
TXFLog_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *)
{
  TXFLog *f = (TXFLog *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_RESET: f->_p.clear(); f->_t.clear(); break;
  }
    return 0;

}

void
TXFLog::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("more", TXFLog_read_param, (void *) H_MORE);
  add_read_handler("log", TXFLog_read_param, (void *) H_LOG);
  add_write_handler("reset", TXFLog_write_param, (void *) H_RESET);
}
#include <click/dequeue.cc>
template class DEQueue<struct click_wifi_extra>;

CLICK_ENDDECLS
EXPORT_ELEMENT(TXFLog)

