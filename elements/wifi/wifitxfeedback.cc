#include <click_wifi.h>
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
    _txf_count = 0;
    for (int x = 0; x < TXF_MAP_SIZE; x++) {
      _map[x] = 0;
    }
  }

  void cleanup() {
    register_click_wifi_tx_cb(NULL, NULL);
    _txf_count = 0;
    for (int x = 0; x < TXF_MAP_SIZE; x++) {
      _map[x] = 0;
    }
  }
  int insert(WifiTXFeedback *txf) {
    for (int x = 0; x < TXF_MAP_SIZE; x++) {
      if (!_map[x]) {
	_txf_count++;
	_map[x] = txf;
	if (_txf_count == 1) {
	  register_click_wifi_tx_cb(&got_skb, this);
	}
	return x;
      }
    }
    return -1;
  }

  bool remove(int x) {
    if (x >= 0 && x < TXF_MAP_SIZE) {
      _map[x] = 0;
      _txf_count--;
      if (_txf_count == 0) {
	register_click_wifi_tx_cb(NULL, NULL);
      }
      return true;
    }
    return false;
  }

  static int got_skb(struct sk_buff *skb, void *n) {
    TXFMap *m = (TXFMap *) n;
    if (m) {
      for (int x = 0; x < TXF_MAP_SIZE; x++) {
	if (m->_map[x]) {
	  m->_map[x]->got_skb(skb);
	  return 0;
	}
      }
    }
    kfree_skb(skb);
    return 0;
  }

  int _txf_count;
};

static TXFMap txf_map;



void
WifiTXFeedback::static_initialize()
{
  txf_map.initialize();
}


void
WifiTXFeedback::static_cleanup()
{
  txf_map.cleanup();
}



WifiTXFeedback::WifiTXFeedback()
  : Element(0, 1),
    _successes(0),
    _failures(0),
    _runs(0),
    _task(this)
{
  MOD_INC_USE_COUNT;
  _head = _tail = 0;
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
  
  ScheduleInfo::initialize_task(this, &_task, 1, errh);

  _map_index = txf_map.insert(this);

  if (_map_index < 0) {
    click_chatter("%{element}: couldn't intialize, got %d !!!\n",
		  this,
		  _map_index);
    return -1;
  }


  _capacity = QSIZE;
  return 0;

}

void
WifiTXFeedback::cleanup(CleanupStage)
{
  for (unsigned i = _head; i != _tail; i = next_i(i))
    _queue[i]->kill();
  _head = _tail = 0;    

  if (!txf_map.remove(_map_index)) {
    click_chatter("%{element}: couldn't cleanup - %d !!!\n",
		  this,
		  _map_index);
  }
}

int 
WifiTXFeedback::got_skb(struct sk_buff *skb) {
  assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

  /* Retrieve the MAC header. */
  //skb_push(skb, skb->data - skb->mac.raw);

  unsigned next = next_i(_tail);

  if (next != _head) {
    Packet *p = Packet::make(skb);
    _queue[_tail] = p;
    _tail = next;
  } else {
    kfree_skb(skb);
    _drops++;
  }
      
  _task.reschedule();

}


bool
WifiTXFeedback::run_task() 
{
  _runs++;
  int npq = 0;

  while (_head != _tail) {
    Packet *p = _queue[_head];
    _head = next_i(_head);

    int status = WIFI_TX_STATUS_ANNO(p);

    if (status & WIFI_FAILURE) {
      _failures++;
    } else {
      _successes++;
    }

    if ((status & WIFI_FAILURE) && noutputs() == 2) {
      output(1).push(p);
    } else {
      output(0).push(p);
    }
    npq++;
  }

  if (npq > 0) {
    _task.fast_reschedule();
  }

  return npq > 0;
}

String
WifiTXFeedback::static_print_stats(Element *e, void *)
{
  WifiTXFeedback *n = (WifiTXFeedback *) e;
  StringAccum sa;
  sa << "successes " << n->_successes;
  sa << " failures " << n->_failures;
  sa << " head " << n->_head;
  sa << " tail " << n->_tail;
  sa << " txf_count " << txf_map._txf_count;
  sa << " runs " << n->_runs;
  sa << " scheduled " << n->_task.scheduled();
  sa << "\n";
  return sa.take_string();
}

void
WifiTXFeedback::add_handlers()
{
  add_task_handlers(&_task);
  add_default_handlers(true);
  add_read_handler("stats", static_print_stats, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(linuxmodule)

EXPORT_ELEMENT(WifiTXFeedback)



