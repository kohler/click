#include <click/config.h>
#include "wifitxfeedback.hh"
CLICK_DECLS

WifiTXFeedback::WifiTXFeedback()
  : Element(0, 1)
{
  MOD_INC_USE_COUNT;
  _print_bits = false;
}

WifiTXFeedback::~WifiTXFeedback()
{
  MOD_DEC_USE_COUNT;

  // does this go here or in cleanup?  in both, i guess...
  register_airo_tx_callback (NULL, NULL);
  register_airo_tx_completed_callback (NULL, NULL);
}

static int wifi_tx_feedback_ctr;

int
WifiTXFeedback::configure(Vector<String> &, ErrorHandler *)
{

  return 0;
}

int
WifiTXFeedback::initialize(ErrorHandler *errh)
{
  int i;
  
  // need some static initialization stuff to make sure we're instantiated only once
  if (wifi_tx_feedback_ctr > 0) {
    printk ("WifiTXFeedback:  already initialized!  this will almost certainly break.\n");
  }
  wifi_tx_feedback_ctr++;
 
  printk ("WifiTXFeedback:  initializing\n");
  
  for (i = 0 ; i < AIRO_MAX_FIDS ; i++) {
    airo_queued_packets[i] = NULL;
  }
  register_airo_tx_callback (&airo_tx_cb_stub, this);
  register_airo_tx_completed_callback (&airo_tx_completed_cb_stub, this);
}

void
WifiTXFeedback::cleanup()
{
  printk ("WifiTXFeedback:  cleaning up\n");
  
  register_airo_tx_callback (NULL, NULL);
  register_airo_tx_completed_callback (NULL, NULL);

  wifi_tx_feedback_ctr--;
}

void
WifiTXFeedback::airo_tx_cb_stub(struct sk_buff *skb, int fid, void *arg)
{
  ((WifiTXFeedback *)arg)->airo_tx_cb (skb, fid);
}

void
WifiTXFeedback::airo_tx_completed_cb_stub(char *result, int fid, void *arg)
{
  ((WifiTXFeedback *)arg)->airo_tx_completed_cb (result, fid);
}

void
WifiTXFeedback::airo_tx_cb(struct sk_buff *skb, int fid)
{
  Packet *p = airo_queued_packets[fid];
  
  //printk ("WifiTXFeedback::tx_cb:  called\n");

  if (p != NULL) {
    printk ("WifiTXFeedback::tx_cb:  fid %d already has a packet!\n", fid);
    p->kill ();
  }
  airo_queued_packets[fid] = Packet::make(skb);
}

void
WifiTXFeedback::airo_tx_completed_cb(char *result, int fid)
{
  Packet *p;

  if (airo_queued_packets[fid] == NULL) {
    printk ("WifiTXFeedback::airo_tx_completed_cb:  no packet for completion on fid %d!\n", fid);
    return;
  }

  p = airo_queued_packets[fid];
  airo_queued_packets[fid] = NULL;
    

  if (_print_bits) {
    char buf[30];
    memset(buf, 0, 28);
    int pos = 0;
    for (int i =0; i < 16; i++) {
      sprintf(buf + pos, "%02x", result[i] & 0xff);
      pos += 2;
      if ((i % 4) == 3) buf[pos++] = ' ';
    }
    
    click_chatter("WifiTXFeedback: %s", buf);
  }


  bool success = !(((unsigned char)result[0x04]) & 0x02);
  int long_retries = (unsigned char) result[0x0d];
  int short_retries = (unsigned char) result[0x0c];
  int rate =  (((unsigned char) result[0x0f]) & ~(1<<7));
   
  
  tx_completed(p, success, long_retries, short_retries, rate);
}

void
WifiTXFeedback::tx_completed(Packet *p, bool success, int long_retries, int short_retries, int rate)
{

  p->set_user_anno_c (TX_ANNO_SUCCESS, success);
  p->set_user_anno_c (TX_ANNO_LONG_RETRIES, long_retries);
  p->set_user_anno_c (TX_ANNO_SHORT_RETRIES, short_retries);
  p->set_user_anno_c (TX_ANNO_RATE, rate);
  
  output(0).push(p);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(WifiTXFeedback)
ELEMENT_REQUIRES(linuxmodule)

