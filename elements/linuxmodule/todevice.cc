/*
 * todevice.{cc,hh} -- element sends packets to Linux devices and waits
 * for transmit complete interrupts via register_net_out
 * Robert Morris
 * Eddie Kohler : register once per configuration
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
#include <linux/netdevice.h>
#include <net/pkt_sched.h>
#undef new
#undef class
#undef delete
}

static int min_ifindex;
static Vector<ToDevice *> *ifindex_map;
static struct notifier_block notifier;
static int registered_writers;

extern "C" int click_ToDevice_out(struct notifier_block *nb, unsigned long val, void *v);

ToDevice::ToDevice()
  : Element(1, 0), _dev(0), _registered(0),
    _idle_calls(0), _busy_returns(0), _hard_start(0), 
    _dev_idle(0), _last_dma_length(0), _last_tx(0),
    _activations(0), _pkts_on_dma(0), _pkts_sent(0),
    _tks_allocated(0)
{
  _dma_full_resched = 0;
  _q_burst_resched = 0;
  _q_full_resched = 0;
  _q_empty_resched = 0;
}

ToDevice::ToDevice(const String &devname)
  : Element(1, 0), _devname(devname), _dev(0), _registered(0),
    _idle_calls(0), _busy_returns(0), _hard_start(0), 
    _dev_idle(0), _last_dma_length(0), _last_tx(0),
    _activations(0), _pkts_on_dma(0), _pkts_sent(0),
    _tks_allocated(0)
{
  _dma_full_resched = 0;
  _q_burst_resched = 0;
  _q_full_resched = 0;
  _q_empty_resched = 0;
}

ToDevice::~ToDevice()
{
  if (_registered) click_chatter("ToDevice still registered");
  uninitialize();
}

void
ToDevice::static_initialize()
{
  if (!ifindex_map)
    ifindex_map = new Vector<ToDevice *>;
  notifier.notifier_call = click_ToDevice_out;
  notifier.priority = 1;
}

void
ToDevice::static_cleanup()
{
  delete ifindex_map;
#ifdef HAVE_CLICK_KERNEL
  if (registered_writers)
    unregister_net_out(&notifier);
#endif
}


ToDevice *
ToDevice::clone() const
{
  return new ToDevice(_devname);
}

int
ToDevice::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpString, "interface name", &_devname,
		     cpEnd);
}

static int
update_ifindex_map(int ifindex, ErrorHandler *errh)
{
  if (ifindex_map->size() == 0) {
    min_ifindex = ifindex;
    ifindex_map->push_back(0);
    return 0;
  }
  
  int left = min_ifindex;
  int right = min_ifindex + ifindex_map->size();
  if (ifindex < left) left = ifindex;
  if (ifindex >= right) right = ifindex + 1;
  if (right - left >= 1000 || right - left < 0)
    return errh->error("too many devices");

  if (left < min_ifindex) {
    int delta = min_ifindex - left;
    for (int i = 0; i < delta; i++)
      ifindex_map->push_back(0);
    for (int i = ifindex_map->size() - delta - 1; i >= 0; i--)
      (*ifindex_map)[i + delta] = (*ifindex_map)[i];
    for (int i = 0; i < delta; i++)
      (*ifindex_map)[i] = 0;
    min_ifindex = left;
  } else if (right > min_ifindex + ifindex_map->size()) {
    int delta = right - min_ifindex - ifindex_map->size();
    for (int i = 0; i < delta; i++)
      ifindex_map->push_back(0);
  }
  return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
  _dev = dev_get(_devname.cc());
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  
  if (input_is_pull(0)) {
    if (!_registered) {
      // add to ifindex_map
      if (update_ifindex_map(_dev->ifindex, errh) < 0)
	return -1;
      int ifindex_off = _dev->ifindex - min_ifindex;
      if ((*ifindex_map)[ifindex_off])
	return errh->error("duplicate ToDevice for device `%s'", _devname.cc());
      (*ifindex_map)[ifindex_off] = this;
      
      if (!registered_writers) {
#ifdef HAVE_CLICK_KERNEL
	notifier.next = 0;
	register_net_out(&notifier);
#else
	errh->warning("not compiled for a Click kernel");
#endif
      }
      
      _registered = 1;
      registered_writers++;
    }
    
    ScheduleInfo::join_scheduler(this, errh);
  }

  return 0;
}

void
ToDevice::uninitialize()
{
  if (_registered) {
    registered_writers--;
#ifdef HAVE_CLICK_KERNEL
    if (registered_writers == 0)
      unregister_net_out(&notifier);
#endif

    // remove from ifindex_map
    int ifindex_off = _dev->ifindex - min_ifindex;
    (*ifindex_map)[ifindex_off] = 0;
    while (ifindex_map->size() > 0 && ifindex_map->back() == 0)
      ifindex_map->pop_back();
    
    _registered = 0;
  }
  unschedule();
}

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
  
#if CLICK_STATS > 0 || XCYC > 0
  entering_ipb();
#endif

  int retval = 0;
  int ifindex_off = dev->ifindex - min_ifindex;
  if (ifindex_off >= 0 && ifindex_off < ifindex_map->size())
    if (ToDevice *kw = (*ifindex_map)[ifindex_off])
      retval = kw->tx_intr();
  
#if CLICK_STATS > 0 || XCYC > 0
  leaving_ipb();
#endif

  return retval;
}

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
 
#if HAVE_POLLING
  queued_pkts = _dev->clean_tx(_dev);
  _pkts_on_dma += queued_pkts;
#endif

  while (sent<TODEV_MAX_PKTS_PER_RUN && (busy=_dev->tbusy)==0) {
    Packet *p;
    if (p = input(0).pull()) {
      queue_packet(p);
      sent++;
    } 
    else break;
  }
  
  if (_activations > 0 || sent > 0) {
    _activations++;
    _tks_allocated += ntickets();
    if (sent == 0) _idle_calls++;
  }
  
  if (sent > 0) _pkts_sent+=sent;

#if HAVE_POLLING
  if (queued_pkts == _last_dma_length + _last_tx && queued_pkts != 0) {
    _dev_idle++;
    if (_dev_idle==1024) {
      /* device didn't send anything, ping it */
      _dev->start_tx(_dev);
      _hard_start++;
      _dev_idle=0;
    }
  } else
    _dev_idle = 0;
#endif
  
  /*
   * If we have packets left in the queue, arrange for
   * net_bh()/qdisc_run_queues() to call us when the
   * device decides it's idle.
   * This is a lot like qdisc_wakeup(), but we don't want to
   * bother trying to send a packet from Linux's queues.
   */
  if (busy) {
    struct Qdisc *q = _dev->qdisc;
    if(q->h.forw == NULL) {
      q->h.forw = qdisc_head.forw;
      qdisc_head.forw = &q->h;
    }
  }
  
  if (busy)
    _busy_returns++;

  /* adjusting tickets. goal: each time we are called, we should send a few
   * packets, but also keep dma ring floating between 1/3 to 3/4 full.
   */

#if HAVE_POLLING
  int dmal = _dev->tx_dma_length;
#else
  int dmal = 16;
#endif
  int dma_thresh_high = dmal-dmal/4;
  int dma_thresh_low  = dmal/4;
  int adj = ntickets()/4;
  if (adj<2) adj=2;

  /* tx dma ring was fairly full, and it was full last time as well, so we
   * should slow down */

  if (busy && queued_pkts > dma_thresh_high 
      && _last_dma_length > dma_thresh_high) { 
    adj = 0-adj;
    if (_activations>0) _dma_full_resched++;
  }

  /* not much there upstream, so slow down */
  else if (!busy && sent < dma_thresh_low) {
    adj = 0-adj;
    if (_activations>0) _q_empty_resched++;
  }
  
  /* handle burstiness */
  else if (sent > dma_thresh_high && !busy && 
           _last_tx < dma_thresh_low && !_last_busy) {
    adj *= 2;
    if (_activations>0) _q_burst_resched++;
  }
  
  /* prevent backlog and keep device running */
  else if (sent > dmal/2) {
    if (_activations>0) _q_full_resched++;
  }
  
  else adj = 0;
  
  adj_tickets(adj);
  _last_dma_length = queued_pkts;
  _last_tx = sent;
  _last_busy = busy;

  reschedule();
}

void
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
      return;
    }
    skb_put(skb1, 60 - skb1->len);
  }

#if HAVE_POLLING
  int ret = _dev->queue_tx(skb1, _dev);
#else
  int ret = _dev->hard_start_xmit(skb1, _dev);
#endif
  if(ret != 0){
    printk("<1>ToDevice %s tx oops\n", _dev->name);
    kfree_skb(skb1);
  }
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
    String(td->max_ntickets()) + " maximum number of tickets\n" +
    String(td->_hard_start) + " hard transmit start\n" +
    String(td->_idle_calls) + " idle tx calls\n" +
    String(td->_busy_returns) + " device busy returns\n" +
    String(td->_pkts_on_dma) + " packets seen pending on DMA ring\n" +
    String(td->_tks_allocated) + " total tickets seen\n" +
    String(td->_dma_full_resched) + " dma full resched\n" +
    String(td->_q_burst_resched) + " q burst resched\n" +
    String(td->_q_full_resched) + " q full resched\n" +
    String(td->_q_empty_resched) + " q empty resched\n" +
    String(td->_pkts_sent) + " packets sent\n" +
    String(td->_activations) + " transmit activations\n";
}

void
ToDevice::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("calls", ToDevice_read_calls, 0);
}

EXPORT_ELEMENT(ToDevice)
