#include <linux/click_wifi.h>
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
#include "wifitxfeedback.hh"

CLICK_DECLS


WifiTXFeedback::WifiTXFeedback()
  : Element(0, 1)
{
  MOD_INC_USE_COUNT;
}

WifiTXFeedback::~WifiTXFeedback()
{
  MOD_DEC_USE_COUNT;
}


int
WifiTXFeedback::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_parse(conf, this, errh, 
		    cpOptional,
		    cpKeywords,
		    cpEnd) < 0) {
      return -1;
    }
  return 0;
}

int
WifiTXFeedback::initialize(ErrorHandler *errh)
{
  register_click_wifi_tx_cb(&static_got_skb, this);
  return 0;

}

void
WifiTXFeedback::cleanup(CleanupStage)
{
  register_click_wifi_tx_cb(NULL, NULL);
}

/*
 * Per-FromDevice packet input routine.
 */
int
WifiTXFeedback::static_got_skb(struct sk_buff *skb, void *arg)
{
  if (arg) {
    ((WifiTXFeedback *) arg)->got_skb(skb);
  } else {
    click_chatter("WifiTxFeedback: arg is null!");
  }
}
int WifiTXFeedback::got_skb(struct sk_buff *skb) {
  assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

  /* Retrieve the MAC header. */
  //skb_push(skb, skb->data - skb->mac.raw);

  Packet *p = Packet::make(skb);
  int success = WIFI_TX_SUCCESS_ANNO(p);
  if (p) {
    if (noutputs() == 2 && !success) {
      output(1).push(p);
    } else {
      output(0).push(p);
    }
  }
}

void
WifiTXFeedback::add_handlers()
{
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(linuxmodule)

EXPORT_ELEMENT(WifiTXFeedback)



