#include <linux/click_wifi.h>
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
#include "rxfcserr.hh"

CLICK_DECLS

RXFCSErr::RXFCSErr()
  : Element(0, 1)
{
  MOD_INC_USE_COUNT;
}

RXFCSErr::~RXFCSErr()
{
  MOD_DEC_USE_COUNT;
}

int
RXFCSErr::configure(Vector<String> &conf, ErrorHandler *errh)
{
  click_chatter("configuring rxfcserr\n");
  return 0;
}

int
RXFCSErr::initialize(ErrorHandler *)
{
  click_chatter("registering fcserr callback\n");
  register_click_wifi_rx_fcserr_cb(&static_cb, this);
  return 0;
}

void
RXFCSErr::cleanup(CleanupStage)
{
  register_click_wifi_rx_fcserr_cb(NULL, NULL);
}

int
RXFCSErr::static_cb(struct sk_buff *skb, void *arg)
{
  if (arg) {
    ((RXFCSErr *)arg)->cb(skb);
  } else {
    click_chatter("RXFCSErr: arg is null!");
  }
  return 0;
}

void 
RXFCSErr::cb(struct sk_buff *skb) 
{
  assert(skb_shared(skb) == 0);
  Packet *p = Packet::make(skb);
  output(0).push(p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(linuxmodule)

EXPORT_ELEMENT(RXFCSErr)



