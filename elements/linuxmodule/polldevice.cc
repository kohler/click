/*
 * polldevice.{cc,hh} -- element steals packets from Linux devices by polling.
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/package.hh>
#include <click/glue.hh>
#include "polldevice.hh"
#include "fromdevice.hh"
#include "todevice.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/skbmgr.hh>
#include "elements/standard/scheduleinfo.hh"

extern "C" {
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <unistd.h>
}

#include <asm/msr.h>

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
  : _registered(false)
{
  // no MOD_INC_USE_COUNT; rely on AnyDevice
  add_output();
  polldev_static_initialize();
}

PollDevice::~PollDevice()
{
  // no MOD_DEC_USE_COUNT; rely on AnyDevice
  assert(!_registered);
  polldev_static_cleanup();
}


int
PollDevice::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _burst = 8;
  _promisc = false;
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_devname,
		  cpOptional,
		  cpBool, "promiscuous mode", &_promisc,
		  cpUnsigned, "burst", &_burst,
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
    if (_dev->polling != 2)
      return errh->error("PollDevice detected wrong version of polling patch");
  }
 
  if (_promisc) dev_set_promiscuity(_dev, 1);

#ifndef RR_SCHED
  /* start out with default number of tickets, inflate up to max */
  _max_tickets = ScheduleInfo::query(this, errh);
  _task.set_tickets(ScheduleInfo::DEFAULT);
#endif
  _task.initialize(this, true);

  reset_counts();
  return 0;
#else
  errh->warning("can't get packets: not compiled with polling extensions");
  return 0;
#endif
}

void
PollDevice::reset_counts()
{
  _npackets = 0;

#if CLICK_DEVICE_STATS
  _activations = 0;
  _time_poll = 0;
  _time_refill = 0;
  _time_allocskb = 0;
  _perfcnt1_poll = 0;
  _perfcnt1_refill = 0;
  _perfcnt1_allocskb = 0;
  _perfcnt1_pushing = 0;
  _perfcnt2_poll = 0;
  _perfcnt2_refill = 0;
  _perfcnt2_allocskb = 0;
  _perfcnt2_pushing = 0;
#endif
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  _push_cycles = 0;
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
  _task.unschedule();
#endif
}

void
PollDevice::run_scheduled()
{
#if HAVE_POLLING
  struct sk_buff *skb_list, *skb;
  int got=0;
#if CLICK_DEVICE_STATS
  unsigned long long time_now;
  unsigned low00, low10;
#endif

  SET_STATS(low00, low10, time_now);

  got = _burst;
  skb_list = _dev->rx_poll(_dev, &got);

#if CLICK_DEVICE_STATS
  if (got > 0 || _activations > 0) {
    GET_STATS_RESET(low00, low10, time_now, 
		    _perfcnt1_poll, _perfcnt2_poll, _time_poll);
    _activations++;
  }
#endif

  int nskbs = got;
  if (got == 0) 
    nskbs = _dev->rx_refill(_dev, 0);

  if (nskbs > 0) {
    struct sk_buff *new_skbs = skbmgr_allocate_skbs(0, 1536, &nskbs);

#if CLICK_DEVICE_STATS
    if (_activations > 0)
      GET_STATS_RESET(low00, low10, time_now, 
	              _perfcnt1_allocskb, _perfcnt2_allocskb, _time_allocskb);
#endif

    nskbs = _dev->rx_refill(_dev, &new_skbs);

#if CLICK_DEVICE_STATS
    if (_activations > 0) 
      GET_STATS_RESET(low00, low10, time_now, 
	              _perfcnt1_refill, _perfcnt2_refill, _time_refill);
#endif

    if (new_skbs) {
      click_chatter("too much skbs for refill");
      skbmgr_recycle_skbs(new_skbs, 0);
    }
  }

  for (int i = 0; i < got; i++) {
    skb = skb_list;
    skb_list = skb_list->next;
    skb->next = NULL;
 
    if (skb_list) {
      // prefetch annotation area, and first 2 cache
      // lines that contain ethernet and ip headers.
#if __i386__ && HAVE_INTEL_CPU
      asm volatile("prefetcht0 %0" : : "m" (skb_list->cb[0]));
      asm volatile("prefetcht0 %0" : : "m" (*(skb_list->data)));
      asm volatile("prefetcht0 %0" : : "m" (*(skb_list->data+32)));
#endif
    }

    /* Retrieve the ether header. */
    skb_push(skb, 14);
    if (skb->pkt_type == PACKET_HOST)
      skb->pkt_type |= PACKET_CLEAN;

    Packet *p = Packet::make(skb);

    _npackets++;
#if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
    unsigned long long before_push_cycles = click_get_cycles();
#endif
    output(0).push(p);
#if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
    _push_cycles += click_get_cycles() - before_push_cycles - CLICK_CYCLE_COMPENSATION;
#endif
  }

#if CLICK_DEVICE_STATS
  if (_activations > 0) {
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_pushing, _perfcnt2_pushing, _push_cycles);
#if _DEV_OVRN_STATS_
    if ((_activations % 1024) == 0) _dev->get_stats(_dev);
#endif
  }
#endif

  adjust_tickets(got);
  _task.fast_reschedule();

#endif /* HAVE_POLLING */
}
 
static String
PollDevice_read_calls(Element *f, void *)
{
  PollDevice *kw = (PollDevice *)f;
  return
    String(kw->_npackets) + " packets received\n" +
#if CLICK_DEVICE_STATS
    String(kw->_time_poll) + " cycles poll\n" +
    String(kw->_time_refill) + " cycles refill\n" +
    String(kw->_time_allocskb) + " cycles allocskb\n" +
    String(kw->_push_cycles) + " cycles pushing\n" +
    String(kw->_perfcnt1_poll) + " perfctr1 poll\n" +
    String(kw->_perfcnt1_refill) + " perfctr1 refill\n" +
    String(kw->_perfcnt1_allocskb) + " perfctr1 allocskb\n" +
    String(kw->_perfcnt1_pushing) + " perfctr1 pushing\n" +
    String(kw->_perfcnt2_poll) + " perfctr2 poll\n" +
    String(kw->_perfcnt2_refill) + " perfctr2 refill\n" +
    String(kw->_perfcnt2_allocskb) + " perfctr2 allocskb\n" +
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
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
   case 1:
    return String(pd->_push_cycles) + "\n";
#endif
#if CLICK_DEVICE_STATS
   case 2:
    return String(pd->_time_poll) + "\n";
   case 3:
    return String(pd->_time_refill) + "\n";
#endif
   default:
    return String();
  }
}

static int
PollDevice_write_stats(const String &, Element *e, void *, ErrorHandler *)
{
  PollDevice *pd = (PollDevice *)e;
  pd->reset_counts();
  return 0;
}

void
PollDevice::add_handlers()
{
  add_read_handler("calls", PollDevice_read_calls, 0);
  add_read_handler("packets", PollDevice_read_stats, 0);
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  add_read_handler("push_cycles", PollDevice_read_stats, (void *)1);
#endif
#if CLICK_DEVICE_STATS
  add_read_handler("poll_cycles", PollDevice_read_stats, (void *)2);
  add_read_handler("refill_dma_cycles", PollDevice_read_stats, (void *)3);
#endif
  add_write_handler("reset_counts", PollDevice_write_stats, 0);
  add_task_handlers(&_task);
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(PollDevice)
