/*
 * todevice.{cc,hh} -- element sends packets to Linux devices and waits
 * for transmit complete interrupts via register_net_out
 * Robert Morris
 * Eddie Kohler : register once per configuration
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
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
    _pull_calls(0), _idle_calls(0), _drain_returns(0), _busy_returns(0),
    _rejected(0), _idle(0), _pkts_sent(0)
{
}

ToDevice::ToDevice(const String &devname)
  : Element(1, 0), _devname(devname), _dev(0), _registered(0),
    _pull_calls(0), _idle_calls(0), _drain_returns(0), 
    _busy_returns(0), _idle(0), _pkts_sent(0)
{
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
    click_chatter("ToDevice(%s): %d sent", declaration().cc(), _pkts_sent);
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
    if (ToDevice *kw = (*ifindex_map)[ifindex_off]) {

      kw->_idle_calls++;
      retval = kw->tx_intr();
    }
  
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
int total_packets_sent = 0;
bool
ToDevice::tx_intr()
{
#if CLICK_STATS >= 2
  unsigned long long c0 = click_get_cycles();
#endif
  int busy;
 
#if CLICK_POLLDEV
  _dev->clean_tx(_dev);
#endif

  while ((busy = _dev->tbusy) == 0) {
    Packet *p;
    _idle++;
    if (p = input(0).pull()) {
      _pkts_sent++;
      push(0, p);
      total_packets_sent++;
      _idle = 0;
    } 
    else break;
  }
  
#if CLICK_POLLDEV
  int tx_left = _dev->clean_tx(_dev);
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
  
  if (busy) {
    _busy_returns++;
  } else {
    _drain_returns++;
  }

#if CLICK_STATS >= 2
  unsigned long long c1 = click_get_cycles();
  _calls += 1;
  _self_cycles += c1 - c0;
#endif

#if CLICK_POLLDEV
  if (busy || _idle <= 1024 || tx_left != 0)
#else
  if (busy || _idle <= 1024)
#endif
    reschedule();
}

void
ToDevice::push(int port, Packet *p)
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

#if CLICK_STATS > 0  
  extern int rtm_opackets;
  extern unsigned long long rtm_obytes;
  rtm_opackets++;
  rtm_obytes += skb1->len;
#endif
  
  int ret = _dev->hard_start_xmit(skb1, _dev);
  if(ret != 0){
    /* device rejected the packet */
    if(_rejected == 0)
      printk("<1>ToDevice %s tx oops\n", _dev->name);
    _rejected += 1;
    kfree_skb(skb1);
  }
}

bool
ToDevice::wants_packet_upstream() const
{
  return input_is_pull(0);
}

void
ToDevice::run_scheduled()
{
  _pull_calls++;
  tx_intr();
}

static String
ToDevice_read_calls(Element *f, void *)
{
  ToDevice *kw = (ToDevice *)f;
  return
    String(kw->_pull_calls) + " pull calls\n" +
    String(kw->_idle_calls) + " tx ready calls\n" +
    String(kw->_drain_returns) + " queue empty returns\n" +
    String(kw->_busy_returns) + " device busy returns\n" +
    String(kw->_rejected) + " hard_start rejections\n";
}

void
ToDevice::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("calls", ToDevice_read_calls, 0);
}

EXPORT_ELEMENT(ToDevice)
