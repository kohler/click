#include <linux/click_wifi.h>
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
#include "wifitxfeedback.hh"

CLICK_DECLS


WifiTXFeedback::WifiTXFeedback()
  : Element(0, 1), _task(this)
{
  MOD_INC_USE_COUNT;
  _head = _tail = 0;
}

WifiTXFeedback::~WifiTXFeedback()
{
  MOD_DEC_USE_COUNT;
}


int
WifiTXFeedback::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 8;
    if (cp_va_parse(conf, this, errh, 
		    cpOptional,
		    cpUnsigned, "burst size", &_burst,
		    cpKeywords,
		    "BURST", cpUnsigned, "burst size", &_burst,
		    cpEnd) < 0) {
      return -1;
    }
  return 0;
}

int
WifiTXFeedback::initialize(ErrorHandler *errh)
{
  ScheduleInfo::initialize_task(this, &_task, true, errh);
  register_click_wifi_tx_cb(&static_got_skb, this);
  _capacity = QSIZE;
  _drops = 0;
  return 0;

}

void
WifiTXFeedback::cleanup()
{
  register_click_wifi_tx_cb(NULL, NULL);
  for (unsigned i = _head; i != _tail; i = next_i(i))
    _queue[i]->kill();
  _head = _tail = 0;    

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
    unsigned next = next_i(_tail);

    if (next != _head) { /* ours */
	assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

	/* Retrieve the MAC header. */
	//skb_push(skb, skb->data - skb->mac.raw);

	Packet *p = Packet::make(skb);
	_queue[_tail] = p; /* hand it to run_task */
	_tail = next;

    } else {
	/* queue full, drop */
	kfree_skb(skb);
	_drops++;
    }

    return 1;
}

bool
WifiTXFeedback::run_task()
{
    int npq = 0;
    while (npq < _burst && _head != _tail) {
	Packet *p = _queue[_head];
	_head = next_i(_head);
	output(0).push(p);
	npq++;
    }
#if CLICK_DEVICE_ADJUST_TICKETS
    adjust_tickets(npq);
#endif
    _task.fast_reschedule();
    return npq > 0;
}

void
WifiTXFeedback::add_handlers()
{
  add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(linuxmodule)
ELEMENT_REQUIRES(HAVE_WIFI)
EXPORT_ELEMENT(WifiTXFeedback)



