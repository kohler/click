/*
 * polldevice.{cc,hh} -- element steals packets from Linux devices by polling.
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "fromdevice.hh"
#include "todevice.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "router.hh"
#include "elemfilter.hh"
#include "elements/standard/scheduleinfo.hh"

extern "C" {
#include <linux/netdevice.h>
#include <unistd.h>
}

#include "perfcount.hh"
#include "asm/msr.h"

/* for hot-swapping */
static AnyDeviceMap poll_device_map;
static int poll_device_count;


static void
polldev_static_initialize()
{
  poll_device_count++;
  if (poll_device_count > 1) return;
  poll_device_map.initialize();
}

static void
polldev_static_cleanup()
{
  poll_device_count--;
}

PollDevice::PollDevice()
  : _registered(false), _promisc(false)
{
  add_output();
  polldev_static_initialize();
}

PollDevice::~PollDevice()
{
  assert(!_registered);
  polldev_static_cleanup();
}


int
PollDevice::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_devname,
		  cpOptional,
		  cpBool, "promiscuous mode", &_promisc,
		  cpEnd) < 0)
    return -1;
#if HAVE_POLLING
  _dev = dev_get(_devname.cc());
  if (!_dev)
    _dev = find_device_by_ether_address(_devname, this);
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  // must check both _dev->polling and _dev->poll_on as some drivers
  // memset() their device structures to all zero
  if (_dev->polling < 0 || !_dev->poll_on)
    return errh->error("device `%s' not pollable, use FromDevice instead", _devname.cc());
#endif
  return 0;
}


/*
 * Use Linux interface for polling, added by us, in include/linux/netdevice.h,
 * to poll devices.
 */
int
PollDevice::initialize(ErrorHandler *errh)
{
#if HAVE_POLLING
  /* try to find a ToDevice with the same device: if none exists, then we need
   * to manage tx queue as well as rx queue. need to do it this way because
   * ToDevice may not have been initialized
   */
  for (int fi = 0; fi < router()->nelements(); fi++) {
    Element *e = router()->element(fi);
    if (e == this) continue;
    if (PollDevice *pd=(PollDevice *)(e->cast("PollDevice"))) {
      if (pd->ifindex() == ifindex())
	return errh->error("duplicate PollDevice for `%s'", _devname.cc());
    } else if (FromDevice *fd = (FromDevice *)(e->cast("FromDevice"))) {
      if (fd->ifindex() == ifindex())
	return errh->error("both FromDevice and PollDevice for `%s'", 
	                   _devname.cc());
    }
  }
  
  if (poll_device_map.insert(this) < 0)
    return errh->error("cannot use PollDevice for device `%s'", _devname.cc());
  _registered = true;
  
  AnyDevice *l = poll_device_map.lookup(ifindex());
  if (l->next() == 0) {
    /* turn off interrupt if interrupts weren't already off */
    _dev->poll_on(_dev);
  }
 
  if (_promisc) dev_set_promiscuity(_dev, 1);

#ifndef RR_SCHED
  /* start out with default number of tickets, inflate up to max */
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(ScheduleInfo::DEFAULT);
#endif

#if CLICK_DEVICE_ADJUST_TICKETS
  _last_rx = 0;
#endif

#if CLICK_DEVICE_STATS
  _activations = 0;
  _idle_calls = 0;
  _pkts_received = 0;
  _time_poll = 0;
  _time_refill = 0;
  _time_pushing = 0;
  _perfcnt1_poll = 0;
  _perfcnt1_refill = 0;
  _perfcnt1_pushing = 0;
  _perfcnt2_poll = 0;
  _perfcnt2_refill = 0;
  _perfcnt2_pushing = 0;
#endif

  _npackets = 0;
#if CLICK_DEVICE_THESIS_STATS
  _push_cycles = 0;
#endif
  
  join_scheduler();

  return 0;
#else
  errh->warning("can't get packets: not compiled with polling extensions");
  return 0;
#endif
}


void
PollDevice::uninitialize()
{
#if HAVE_POLLING
  assert(_registered);
  poll_device_map.remove(this);
  _registered = false;
  if (poll_device_map.lookup(ifindex()) == 0) {
    if (_dev && _dev->polling > 0)
      _dev->poll_off(_dev);
  }
  if (_promisc) dev_set_promiscuity(_dev, -1);
  unschedule();
#endif
}

void
PollDevice::run_scheduled()
{
#if HAVE_POLLING
  struct sk_buff *skb_list, *skb;
  int got=0;
#if CLICK_DEVICE_STATS
  unsigned long time_now;
  unsigned low00, low10;
#endif

  SET_STATS(low00, low10, time_now);

  got = INPUT_BATCH;
  skb_list = _dev->rx_poll(_dev, &got);

#if CLICK_DEVICE_STATS
  if (got > 0 || _activations > 0) {
    _activations++;
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_poll, _perfcnt2_poll, _time_poll);
    _pkts_received += got;
    if (got == 0) _idle_calls++;
  }
#endif
  
  _dev->rx_refill(_dev);
  
#if CLICK_DEVICE_STATS
  if (_activations > 0)
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_refill, _perfcnt2_refill, _time_refill);
#endif

  for(int i=0; i<got; i++) {
    skb = skb_list;
    skb_list = skb_list->next;
    skb->next = skb->prev = NULL;
   
#if 0
    assert(skb);
    assert(skb->data - skb->head >= 14);
    assert(skb->mac.raw == skb->data - 14);
    assert(skb_shared(skb) == 0);
#endif

    /* Retrieve the ether header. */
    skb_push(skb, 14);

    Packet *p = Packet::make(skb);

    _npackets++;
#if CLICK_DEVICE_THESIS_STATS
    unsigned long long before_push_cycles = click_get_cycles();
#endif
    output(0).push(p);
#if CLICK_DEVICE_THESIS_STATS
    _push_cycles += click_get_cycles() - before_push_cycles;
#endif
  }

#if CLICK_DEVICE_STATS
  if (_activations > 0 && got > 0) {
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_pushing, _perfcnt2_pushing, _time_pushing);
#if _DEV_OVRN_STATS_
    if ((_activations % 1024) == 0) _dev->get_stats(_dev);
#endif
  }
#endif

#if CLICK_DEVICE_ADJUST_TICKETS
  int base = tickets() / 4;
  if (base < 2) base = 2;
  int adj = 0;
  
  /*
   * bursty traffic: sent substantially more packets this time than last time,
   * so increase our tickets a lot to adapt.
   */
  if (got == INPUT_BATCH && _last_rx <= INPUT_BATCH/8) 
    adj = base * 2;
  /* 
   * was able to get many packets, so increase our ticket some to adapt.
   */
  else if (got == INPUT_BATCH)
    adj = base;
  /*
   * no packets, decrease tickets by some
   */
  else if (got < INPUT_BATCH/4) 
    adj= 0 - base;

  adj_tickets(adj);
  _last_rx = got;
#endif

  reschedule();

#endif /* HAVE_POLLING */
}
 
static String
PollDevice_read_calls(Element *f, void *)
{
  PollDevice *kw = (PollDevice *)f;
  return
#if CLICK_DEVICE_STATS
    String(kw->_idle_calls) + " idle calls\n" +
    String(kw->_pkts_received) + " packets received\n" +
    String(kw->_time_poll) + " cycles poll\n" +
    String(kw->_time_refill) + " cycles refill\n" +
    String(kw->_time_pushing) + " cycles pushing\n" +
    String(kw->_perfcnt1_poll) + " perfctr1 poll\n" +
    String(kw->_perfcnt1_refill) + " perfctr1 refill\n" +
    String(kw->_perfcnt1_pushing) + " perfctr1 pushing\n" +
    String(kw->_perfcnt2_poll) + " perfctr2 poll\n" +
    String(kw->_perfcnt2_refill) + " perfctr2 refill\n" +
    String(kw->_perfcnt2_pushing) + " perfctr2 pushing\n" +
    String(kw->_activations) + " activations\n";
#else
    String();
#endif
}

static String
PollDevice_read_stats(Element *e, void *thunk)
{
  PollDevice *pd = (PollDevice *)e;
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(pd->_npackets) + "\n";
#if CLICK_DEVICE_THESIS_STATS
   case 1:
    return String(pd->_push_cycles) + "\n";
#endif
   default:
    return String();
  }
}

static int
PollDevice_write_stats(const String &, Element *e, void *, ErrorHandler *)
{
  PollDevice *pd = (PollDevice *)e;
  pd->_npackets = 0;
#if CLICK_DEVICE_THESIS_STATS
  pd->_push_cycles = 0;
#endif
  return 0;
}

void
PollDevice::add_handlers()
{
  add_read_handler("calls", PollDevice_read_calls, 0);
  add_read_handler("packets", PollDevice_read_stats, 0);
#if CLICK_DEVICE_THESIS_STATS
  add_read_handler("push_cycles", PollDevice_read_stats, (void *)1);
#endif
  add_write_handler("reset_counts", PollDevice_write_stats, 0);
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(PollDevice)
