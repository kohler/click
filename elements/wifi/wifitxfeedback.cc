#include <linux/click_wifi.h>
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/straccum.hh>
#include "wifitxfeedback.hh"

CLICK_DECLS

/*
 * TXFMap is similar to AnyDeviceMap in the linuxmodule dir.
 * if there is a better way to do this, feel free to change it.
 * I had troubles when hotswapping configs and using WifiTXFeedback, because 
 * the old element would unregister the callback after the new one registered. 
 * when this happended, there would be no callback registered, and no 
 * txfeedback. At least this new code fixes that.
 * --jbicket
 */

class TXFMap { public:
  enum { TXF_MAP_SIZE = 10 };
  WifiTXFeedback *_map[TXF_MAP_SIZE];

  void initialize() {
    for (int x = 0; x < TXF_MAP_SIZE; x++) {
      _map[x] = 0;
    }
  }
  int insert(WifiTXFeedback *txf) {
    for (int x = 0; x < TXF_MAP_SIZE; x++) {
      if (!_map[x]) {
	_map[x] = txf;
	return x;
      }
    }
    return -1;
  }

  bool remove(int x) {
    if (x >= 0 && x < TXF_MAP_SIZE) {
      _map[x] = 0;
      return true;
    }
    return false;
  }

  void got_skb(struct sk_buff *skb) {
    for (int x = 0; x < TXF_MAP_SIZE; x++) {
      if (_map[x]) {
	_map[x]->got_skb(skb);
	return;
      }
    }
    click_chatter("TXFMap couldn't find element for skb - there's a leak!!\n");
  }
};
static int txf_count;
static TXFMap txf_map;

static int static_got_skb(struct sk_buff *skb, void *)
{
  txf_map.got_skb(skb);
}


static int
static_initialize(WifiTXFeedback *txf)
{
  txf_count++;
  int ndx = -1;
  if (txf_count == 1) {
    txf_map.initialize();
    ndx = txf_map.insert(txf);
    register_click_wifi_tx_cb(&static_got_skb, NULL);
  } else {
    ndx = txf_map.insert(txf);
  }
  return ndx;
}

static bool 
static_cleanup(int ndx) 
{
  txf_count--;
  if (txf_count == 0) {
    register_click_wifi_tx_cb(NULL, NULL);
  }
  return txf_map.remove(ndx);

}




WifiTXFeedback::WifiTXFeedback()
  : Element(0, 1),
    _successes(0),
    _failures(0)
{
  MOD_INC_USE_COUNT;
}

WifiTXFeedback::~WifiTXFeedback()
{
  MOD_DEC_USE_COUNT;
}

void
WifiTXFeedback::notify_noutputs(int n) 
{
  set_noutputs(n < 2 ? 1 : 2);
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
  _map_index = static_initialize(this);
  if (_map_index < 0) {
    click_chatter("%{element}: couldn't intialize, got %d !!!\n",
		  this,
		  _map_index);
    return -1;
  }
  return 0;

}

void
WifiTXFeedback::cleanup(CleanupStage)
{
  if (!static_cleanup(_map_index)) {
    click_chatter("%{element}: couldn't cleanup - %d !!!\n",
		  this,
		  _map_index);
  }
}

int WifiTXFeedback::got_skb(struct sk_buff *skb) {
  assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

  /* Retrieve the MAC header. */
  //skb_push(skb, skb->data - skb->mac.raw);

  Packet *p = Packet::make(skb);
  int success = WIFI_TX_SUCCESS_ANNO(p);
  SET_WIFI_NUM_FAILURES(p, WIFI_NUM_FAILURES(p) + !success);
  if (success) {
    _successes++;
  } else {
    _failures++;
  }
  if (p) {
    if (noutputs() == 2 && !success) {
      output(1).push(p);
    } else {
      output(0).push(p);
    }
  }
}

String
WifiTXFeedback::static_print_stats(Element *e, void *)
{
  WifiTXFeedback *n = (WifiTXFeedback *) e;
  StringAccum sa;
  sa << "successes " << n->_successes;
  sa << " failures " << n->_failures;
  sa << "\n";
  return sa.take_string();
}

void
WifiTXFeedback::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("stats", static_print_stats, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(linuxmodule)

EXPORT_ELEMENT(WifiTXFeedback)



