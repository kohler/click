// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_WIFITXFEEDBACK_HH
#define CLICK_WIFITXFEEDBACK_HH
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/packet_anno.hh>
#include <click/timer.hh>
CLICK_DECLS


enum TX_ANNO {
    TX_ANNO_SUCCESS = 0,
    TX_ANNO_LONG_RETRIES = 1,
    TX_ANNO_SHORT_RETRIES = 2,
    TX_ANNO_RATE = 3
};


typedef void (*tx_callback)(struct sk_buff *skb, int fid, void *arg);
typedef void (*tx_completed_callback)(char *result, int fid, void *arg);

extern "C" {
  void register_airo_tx_callback (tx_callback cb, void *arg);
  void register_airo_tx_completed_callback (tx_completed_callback cb, void *arg);
}

class WifiTXFeedback : public Element { public:
  
  WifiTXFeedback();
  ~WifiTXFeedback();

  const char *class_name() const { return "WifiTXFeedback"; }
  const char *processing() const { return PUSH; }
  WifiTXFeedback *clone() const { return new WifiTXFeedback; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  void cleanup();

private:

  static void airo_tx_cb_stub (struct sk_buff *skb, int fid, void *arg);
  static void airo_tx_completed_cb_stub (char *result, int fid, void *arg);

  void airo_tx_cb (struct sk_buff *skb, int fid);
  void airo_tx_completed_cb (char *result, int fid);
  void tx_completed(Packet *p, bool fail, int long_retries, int short_retries, int rate);
  bool _active;
  bool _print_bits;
  
#define AIRO_MAX_FIDS 6
  Packet *airo_queued_packets[AIRO_MAX_FIDS];
};

CLICK_ENDDECLS
#endif

