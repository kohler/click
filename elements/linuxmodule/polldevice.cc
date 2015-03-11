// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * polldevice.{cc,hh} -- element steals packets from Linux devices by polling.
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include "polldevice.hh"
#include "fromdevice.hh"
#include "todevice.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/skbmgr.hh>
#include <click/standard/scheduleinfo.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/netdevice.h>
#if __i386__
#include <click/perfctr-i586.hh>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

/* for hot-swapping */
static AnyDeviceMap poll_device_map;
static struct notifier_block device_notifier;
extern "C" {
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}

void
PollDevice::static_initialize()
{
    poll_device_map.initialize();
    device_notifier.notifier_call = device_notifier_hook;
    device_notifier.priority = 1;
    device_notifier.next = 0;
    register_netdevice_notifier(&device_notifier);
}

void
PollDevice::static_cleanup()
{
    unregister_netdevice_notifier(&device_notifier);
}

PollDevice::PollDevice()
{
}

PollDevice::~PollDevice()
{
}


int
PollDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 8;
    _headroom = 64;
    _length = 0;
    _user_length = false;
    if (AnyDevice::configure_keywords(conf, errh, true) < 0
	|| (Args(conf, this, errh)
	    .read_mp("DEVNAME", _devname)
	    .read_p("BURST", _burst)
	    .read("HEADROOM", _headroom)
	    .read("LENGTH", _length).read_status(_user_length)
	    .complete() < 0))
	return -1;

#if HAVE_LINUX_POLLING
    net_device *dev = lookup_device(errh);
    if (dev && (!dev->poll_on || dev->polling < 0)) {
	dev_put(dev);
	return errh->error("device %<%s%> not pollable, use FromDevice instead", _devname.c_str());
    }
    set_device(dev, &poll_device_map, 0);
#endif
    return errh->nerrors() ? -1 : 0;
}


/*
 * Use Linux interface for polling, added by us, in include/linux/netdevice.h,
 * to poll devices.
 */
int
PollDevice::initialize(ErrorHandler *errh)
{
    if (AnyDevice::initialize_keywords(errh) < 0)
	return -1;

#if HAVE_LINUX_POLLING
    // check for duplicate readers
    if (ifindex() >= 0) {
	void *&used = router()->force_attachment("device_reader_" + String(ifindex()));
	if (used)
	    return errh->error("duplicate reader for device '%s'", _devname.c_str());
	used = this;

	if (!router()->attachment("device_writer_" + String(ifindex()) + "_0"))
	    errh->warning("no ToDevice(%s) in configuration\n(\
Generally, you will get bad performance from PollDevice unless\n\
you include a ToDevice for the same device. Try adding\n\
'Idle -> ToDevice(%s)' to your configuration.)", _devname.c_str(), _devname.c_str());
    }

    if (_dev && !_dev->polling) {
	/* turn off interrupt if interrupts weren't already off */
	int rx_buffer_length = _dev->poll_on(_dev);
	if (_dev->polling != 2)
	    return errh->error("PollDevice detected wrong version of polling patch");
	if (!_user_length)
	    _length = (rx_buffer_length < 1536 ? 1536 : rx_buffer_length);
    }
    if (_dev && _headroom < LL_RESERVED_SPACE(_dev))
	errh->warning("device %s requests at least %d bytes of HEADROOM", _devname.c_str(), (int) LL_RESERVED_SPACE(_dev));

    ScheduleInfo::initialize_task(this, &_task, _dev != 0, errh);
#if HAVE_STRIDE_SCHED
    // user specifies max number of tickets; we start with default
    _max_tickets = _task.tickets();
    _task.set_tickets(Task::DEFAULT_TICKETS);
#endif

    reset_counts();

#else
    errh->warning("can't get packets: not compiled with polling extensions");
#endif

    return 0;
}

void
PollDevice::reset_counts()
{
  _npackets = 0;

#if CLICK_DEVICE_STATS
  _activations = 0;
  _empty_polls = 0;
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
  _buffers_reused = 0;
}

void
PollDevice::cleanup(CleanupStage)
{
#if HAVE_LINUX_POLLING
    net_device *had_dev = _dev;

    // call clear_device first so we can check poll_device_map for
    // other users
    clear_device(&poll_device_map, 0);

    unsigned long lock_flags;
    poll_device_map.lock(false, lock_flags);
    if (had_dev && had_dev->polling > 0 && !poll_device_map.lookup(had_dev, 0))
	had_dev->poll_off(had_dev);
    poll_device_map.unlock(false, lock_flags);
#endif
}

bool
PollDevice::run_task(Task *)
{
#if HAVE_LINUX_POLLING
  struct sk_buff *skb_list, *skb;
  int got=0;
# if CLICK_DEVICE_STATS
  uint64_t time_now;
  unsigned low00, low10;
# endif

  SET_STATS(low00, low10, time_now);

  got = _burst;
  skb_list = _dev->rx_poll(_dev, &got);

# if CLICK_DEVICE_STATS
  if (got > 0 || _activations > 0) {
    GET_STATS_RESET(low00, low10, time_now,
		    _perfcnt1_poll, _perfcnt2_poll, _time_poll);
    if (got == 0)
      _empty_polls++;
    else
      _activations++;
  }
# endif

  int nskbs = got;
  if (got == 0)
    nskbs = _dev->rx_refill(_dev, 0);

  if (nskbs > 0) {
    /*
     * Need to allocate 1536+16 == 1552 bytes per packet.
     * "Extra 16 bytes in the SKB for eepro100 RxFD -- perhaps there
     * should be some callback to the device driver to query for the
     * desired packet size."
     * Skbmgr adds 64 bytes of headroom and tailroom, so back request off to
     * 1536.
     */
    struct sk_buff *new_skbs = skbmgr_allocate_skbs(_headroom, _length, &nskbs);

# if CLICK_DEVICE_STATS
    if (_activations > 0)
      GET_STATS_RESET(low00, low10, time_now,
	              _perfcnt1_allocskb, _perfcnt2_allocskb, _time_allocskb);
# endif

    nskbs = _dev->rx_refill(_dev, &new_skbs);

# if CLICK_DEVICE_STATS
    if (_activations > 0)
      GET_STATS_RESET(low00, low10, time_now,
	              _perfcnt1_refill, _perfcnt2_refill, _time_refill);
# endif

    if (new_skbs) {
	for (struct sk_buff *skb = new_skbs; skb; skb = skb->next)
	    _buffers_reused++;
	skbmgr_recycle_skbs(new_skbs);
    }
  }

  for (int i = 0; i < got; i++) {
    skb = skb_list;
    skb_list = skb_list->next;
    skb->next = NULL;

    if (skb_list) {
      // prefetch annotation area, and first 2 cache
      // lines that contain ethernet and ip headers.
# if __i386__ && HAVE_INTEL_CPU
      asm volatile("prefetcht0 %0" : : "m" (skb_list->cb[0]));
      // asm volatile("prefetcht0 %0" : : "m" (*(skb_list->data)));
      asm volatile("prefetcht0 %0" : : "m" (*(skb_list->data+32)));
# endif
    }

    /* Retrieve the ether header. */
    skb_push(skb, 14);
    if (skb->pkt_type == PACKET_HOST)
      skb->pkt_type |= PACKET_CLEAN;

    Packet *p = Packet::make(skb);

# ifndef CLICK_WARP9
    if (timestamp())
	p->timestamp_anno().assign_now();
# endif

    _npackets++;
# if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
    click_cycles_t before_push_cycles = click_get_cycles();
# endif
    output(0).push(p);
# if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
    _push_cycles += click_get_cycles() - before_push_cycles - CLICK_CYCLE_COMPENSATION;
# endif
  }

# if CLICK_DEVICE_STATS
  if (_activations > 0) {
    GET_STATS_RESET(low00, low10, time_now,
	            _perfcnt1_pushing, _perfcnt2_pushing, _push_cycles);
#  if _DEV_OVRN_STATS_
    if ((_activations % 1024) == 0)
	_dev->get_stats(_dev);
#  endif
  }
# endif

  adjust_tickets(got);
  _task.fast_reschedule();
  return got > 0;
#else
  return false;
#endif /* HAVE_LINUX_POLLING */
}

void
PollDevice::change_device(net_device *dev)
{
#if HAVE_LINUX_POLLING
    bool dev_change = _dev != dev;

    if (dev_change) {
	_task.strong_unschedule();

	if (dev && (!dev->poll_on || dev->polling < 0)) {
	    click_chatter("%s: device '%s' does not support polling", declaration().c_str(), _devname.c_str());
	    dev = 0;
	}

	if (_dev)
	    _dev->poll_off(_dev);
    }

    set_device(dev, &poll_device_map, anydev_change);

    if (dev_change) {
	if (_dev && !_dev->polling) {
	    int rx_buffer_length = _dev->poll_on(_dev);
	    if (!_user_length)
		_length = (rx_buffer_length < 1536 ? 1536 : rx_buffer_length);
	}

	if (_dev)
	    _task.strong_reschedule();
    }
#else
    (void) dev;
#endif /* HAVE_LINUX_POLLING */
}

extern "C" {
static int
device_notifier_hook(struct notifier_block *nb, unsigned long flags, void *v)
{
#ifdef NETDEV_GOING_DOWN
    if (flags == NETDEV_GOING_DOWN)
	flags = NETDEV_DOWN;
#endif
    if (flags == NETDEV_DOWN || flags == NETDEV_UP || flags == NETDEV_CHANGE) {
	bool exists = (flags != NETDEV_UP);
	net_device *dev = (net_device *)v;
	unsigned long lock_flags;
	poll_device_map.lock(true, lock_flags);
	AnyDevice *es[8];
	int nes = poll_device_map.lookup_all(dev, exists, es, 8);
	for (int i = 0; i < nes; i++)
	    ((PollDevice *)(es[i]))->change_device(flags == NETDEV_DOWN ? 0 : dev);
	poll_device_map.unlock(true, lock_flags);
    }
    return 0;
}
}

static String
PollDevice_read_calls(Element *f, void *)
{
  PollDevice *kw = (PollDevice *)f;
  return
    String(kw->_npackets) + " packets received\n" +
    String(kw->_buffers_reused) + " buffers reused\n" +
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
    String(kw->_empty_polls) + " empty polls\n" +
    String(kw->_activations) + " activations\n";
#else
    String();
#endif
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
    add_data_handlers("count", Handler::f_read, &_npackets);
    // XXX deprecated
    add_data_handlers("packets", Handler::f_read | Handler::f_deprecated, &_npackets);
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
    add_data_handlers("push_cycles", Handler::f_read, &_push_cycles);
#endif
#if CLICK_DEVICE_STATS
    add_data_handlers("poll_cycles", Handler::f_read, &_time_poll);
    add_data_handlers("refill_dma_cycles", Handler::f_read, &_time_refill);
#endif
    add_write_handler("reset_counts", PollDevice_write_stats, 0, Handler::f_button);
    add_data_handlers("buffers_reused", Handler::f_read, &_buffers_reused);
    add_task_handlers(&_task);
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(PollDevice)
