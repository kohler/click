/*
 * todevice.{cc,hh} -- element sends packets to Linux devices.
 * Robert Morris
 * Eddie Kohler: register once per configuration
 * Benjie Chen: polling
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "polldevice.hh"
#include "todevice.hh"
#include "error.hh"
#include "etheraddress.hh"
#include "confparse.hh"
#include "router.hh"
#include "elements/standard/scheduleinfo.hh"
extern "C" {
#define new xxx_new
#define class xxx_class
#define delete xxx_delete
#include <net/pkt_sched.h>
#undef new
#undef class
#undef delete
}

#include "perfcount.hh"
#include "asm/msr.h"

#if 0
static AnyDeviceMap to_device_map;
static struct notifier_block notifier;
extern "C" int click_ToDevice_out(struct notifier_block *nb, unsigned long val, void *v);
#endif
static int registered_writers;

ToDevice::ToDevice()
  : _polling(0), _registered(0),
    _dev_idle(0), _last_dma_length(0), _last_tx(0), _last_busy(0), 
    _rejected(0), _hard_start(0), _activations(0)
{
  add_input();
#if _CLICK_STATS_
  _idle_pulls = 0; 
  _idle_calls = 0; 
  _busy_returns = 0; 
  _pkts_sent = 0; 
  _time_pull = 0;
  _time_clean = 0;
  _time_queue = 0;
  _perfcnt1_pull = 0;
  _perfcnt1_clean = 0;
  _perfcnt1_queue = 0;
  _perfcnt2_pull = 0;
  _perfcnt2_clean = 0;
  _perfcnt2_queue = 0;
#endif
}

ToDevice::~ToDevice()
{
  if (_registered) uninitialize();
}

void
ToDevice::static_initialize()
{
#if 0
  notifier.notifier_call = click_ToDevice_out;
  notifier.priority = 1;
  to_device_map.initialize();
#endif
}

void
ToDevice::static_cleanup()
{
#ifdef HAVE_CLICK_KERNEL 
#if 0
  if (registered_writers)
    unregister_net_out(&notifier);
#endif
#endif
}


int
ToDevice::configure(const String &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_devname,
		  cpEnd) < 0)
    return -1;
  _dev = dev_get(_devname.cc());
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
#ifndef HAVE_CLICK_KERNEL
  errh->warning("not compiled for a Click kernel");
#endif

  /* see if a PollDevice with the same device exists: if so, use polling
   * extensions */
  // need to do it this way because ToDevice may not have been initialized
  for (int fi = 0; fi < router()->nelements(); fi++) {
    Element *e = router()->element(fi);
    if (e == this) continue;
    if (ToDevice *td=(ToDevice *) (e->cast("ToDevice"))) {
      if (td->ifindex() == ifindex())
	return errh->error("duplicate ToDevice for `%s'", _devname.cc());
    } else if (PollDevice *pd = (PollDevice *)(e->cast("PollDevice"))) {
      if (pd->ifindex() == ifindex())
	_polling = 1;
    }
  }

#if 0
  if (to_device_map.insert(this) < 0)
    return errh->error("cannot use ToDevice for device `%s'", _devname.cc());
  
  if (!registered_writers) {
#ifdef HAVE_CLICK_KERNEL
    notifier.next = 0;
    register_net_out(&notifier);
#endif
  }
  registered_writers++;
#endif
  _registered = 1;

#ifndef RR_SCHED
  /* start out with default number of tickets, inflate up to max */
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(ScheduleInfo::DEFAULT);
#endif
  join_scheduler();

  return 0;
}

void
ToDevice::uninitialize()
{
#if 0
  registered_writers--;
#ifdef HAVE_CLICK_KERNEL
  if (registered_writers == 0) 
    unregister_net_out(&notifier);
#endif
  /* remove from ifindex_map */
  if (_registered)
    to_device_map.remove(this);
#endif
  _registered = 0;
  unschedule();
}

#if 0
/*
 * Called by net_bh() when an interface is ready to send.
 * Actually called by qdisc_run_queues() in sch_generic.c.
 *
 * Returning 0 means no more packets to send.
 * Returning 1 means call me again soon.
 */
extern "C" int
click_ToDevice_out(struct notifier_block *nb, unsigned long val, void *v)
{
  struct device *dev = (struct device *) v;
  
  int retval = 0;
  int ifindex = dev->ifindex;
  if (ifindex >= 0 && ifindex < MAX_DEVICES)
    if (ToDevice *kw = (ToDevice*)to_device_map.lookup(ifindex)) {
      if (!kw->polling())
        retval = kw->tx_intr();
    }
  return retval;
}
#endif

/*
 * The kernel thinks our device is idle.
 * Pull a packet and try to stuff it into the device.
 * 
 * Serious problem: Linux drivers aren't required to
 * accept a packet even if they've marked themselves
 * as idle. What do we do with a rejected packet?
 */
bool
ToDevice::tx_intr()
{
  int busy;
  int sent = 0;
  int queued_pkts;
  
#if _CLICK_STATS_
  unsigned low00, low10;
  unsigned long time_now;
#endif

#if HAVE_POLLING
  if (_polling) {
    SET_STATS(low00, low10, time_now);
 
    queued_pkts = _dev->tx_clean(_dev);

    if (_activations > 0)
      GET_STATS_RESET(low00, low10, time_now, 
	              _perfcnt1_clean, _perfcnt2_clean, _time_clean);
  }
#endif

  while (sent<OUTPUT_MAX_PKTS_PER_RUN && (busy=_dev->tbusy)==0) {
    int r;
    Packet *p;
    if (p = input(0).pull()) {
      
      GET_STATS_RESET(low00, low10, time_now, 
	              _perfcnt1_pull, _perfcnt2_pull, _time_pull);

      r = queue_packet(p);
      
      GET_STATS_RESET(low00, low10, time_now, 
	              _perfcnt1_queue, _perfcnt2_queue, _time_queue);

      if (r < 0) break;
      sent++;
    }
    else break;
  }

  if (sent > 0 || _activations > 0) _activations++;

#if _CLICK_STATS_
  if (_activations > 0) {
    if (sent == 0) _idle_calls++;
    if (sent == 0 && !busy) _idle_pulls++;
    if (sent > 0) _pkts_sent+=sent;
    if (busy) _busy_returns++;
  }
#endif

#if HAVE_POLLING
  if (_polling) {
    if (queued_pkts == _last_dma_length + _last_tx && queued_pkts != 0) {
      _dev_idle++;
      if (_dev_idle==1024) {
        /* device didn't send anything, ping it */
        _dev->tx_start(_dev);
        _dev_idle=0;
        _hard_start++;
      }
    } else
      _dev_idle = 0;
  }
#endif
 
#if 0 && !HAVE_POLLING
  if (!_polling) {
    /* If we have packets left in the queue, arrange for
     * net_bh()/qdisc_run_queues() to call us when the device decides it's idle.
     * This is a lot like qdisc_wakeup(), but we don't want to bother trying to
     * send a packet from Linux's queues.
     */
    if (_dev->tbusy) {
      struct Qdisc *q = _dev->qdisc;
      if(q->h.forw == NULL) {
        q->h.forw = qdisc_head.forw;
        qdisc_head.forw = &q->h;
      }
    }
  }
#endif

#ifndef RR_SCHED
#ifdef ADJ_TICKETS
  /* WARNING: fined tuned black magic below, don't change! */
#if HAVE_POLLING
  int dma_thresh_high = _dev->tx_dma_length-_dev->tx_dma_length/8;
  int dma_thresh_low  = _dev->tx_dma_length/4;
#else
  int dma_thresh_high = 16-16/8;
  int dma_thresh_low  = 16/4;
#endif
  int adj = tickets()/4;
  if (adj < 2) adj = 2;

  /* tx dma ring was fairly full, slow down */
  if (busy && queued_pkts > dma_thresh_high 
      && _last_dma_length > dma_thresh_high) adj=0-adj;
  /* not much there upstream, so slow down */
  else if (!busy && sent < dma_thresh_low) adj=0-adj;
  /* handle burstiness: start a bit faster */
  else if (sent > dma_thresh_high && !busy && 
           _last_tx < dma_thresh_low && !_last_busy) adj*=2;
  /* prevent backlog and keep device running */ 
  else if (sent > dma_thresh_low/2) { 
    /* semi-bursty: start a bit faster if we sent a lot */ 
    if (sent > dma_thresh_high) if (adj<8) adj=8; 
  }
  else adj = 0;

  adj_tickets(adj);
#endif /* ADJ_TICKETS */
  
  _last_dma_length = queued_pkts;
  _last_tx = sent;
  _last_busy = busy;
  reschedule();
#endif /* !RR_SCHED */
}

int
ToDevice::queue_packet(Packet *p)
{
  struct sk_buff *skb1 = p->steal_skb();
  
  /*
   * Ensure minimum ethernet packet size (14 hdr + 46 data).
   * I can't figure out where Linux does this, so I don't
   * know the correct procedure.
   */

  if(skb1->len < 60){
    if(skb_tailroom(skb1) < 60 - skb1->len){
      printk("ToDevice: too small: len %d tailroom %d\n",
             skb1->len, skb_tailroom(skb1));
      kfree_skb(skb1);
      return -1;
    }
    skb_put(skb1, 60 - skb1->len);
  }

  int ret;
#if HAVE_POLLING
  if (_polling)
    ret = _dev->tx_queue(skb1, _dev); 
  else 
#endif
  {
    ret = _dev->hard_start_xmit(skb1, _dev);
    _hard_start++;
  }
  if(ret != 0){
    printk("<1>ToDevice %s tx oops\n", _dev->name);
    kfree_skb(skb1);
    _rejected++;
  }
  return ret;
}

void
ToDevice::run_scheduled()
{
  tx_intr();
}

static String
ToDevice_read_calls(Element *f, void *)
{
  ToDevice *td = (ToDevice *)f;
  return
    String(td->_rejected) + " packets rejected\n" +
    String(td->_hard_start) + " hard start xmit\n" +
#if _CLICK_STATS_
    String(td->_idle_calls) + " idle tx calls\n" +
    String(td->_idle_pulls) + " idle pulls\n" +
    String(td->_busy_returns) + " device busy returns\n" +
    String(td->_pkts_sent) + " packets sent\n" +
    String(td->_time_pull) + " cycles pull\n" +
    String(td->_time_clean) + " cycles clean\n" +
    String(td->_time_queue) + " cycles queue\n" +
    String(td->_perfcnt1_pull) + " perfctr1 pull\n" +
    String(td->_perfcnt1_clean) + " perfctr1 clean\n" +
    String(td->_perfcnt1_queue) + " perfctr1 queue\n" +
    String(td->_perfcnt2_pull) + " perfctr2 pull\n" +
    String(td->_perfcnt2_clean) + " perfctr2 clean\n" +
    String(td->_perfcnt2_queue) + " perfctr2 queue\n" +
#endif
    String(td->_activations) + " transmit activations\n";
}

void
ToDevice::add_handlers()
{
  add_read_handler("calls", ToDevice_read_calls, 0);
}

EXPORT_ELEMENT(ToDevice)
ELEMENT_REQUIRES(AnyDevice)
