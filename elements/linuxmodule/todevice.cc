// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * todevice.{cc,hh} -- element sends packets to Linux devices.
 * Robert Morris
 * Eddie Kohler: register once per configuration
 * Benjie Chen: polling
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
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
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <net/pkt_sched.h>
#if __i386__
#include <asm/msr.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

/* for watching when devices go offline */
static AnyDeviceMap to_device_map;
static int to_device_count;
static struct notifier_block device_notifier;
extern "C" {
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}

static void
todev_static_initialize()
{
    if (++to_device_count == 1) {
	to_device_map.initialize();
	device_notifier.notifier_call = device_notifier_hook;
	device_notifier.priority = 1;
	device_notifier.next = 0;
	register_netdevice_notifier(&device_notifier);
    }
}

static void
todev_static_cleanup()
{
    if (--to_device_count <= 0)
	unregister_netdevice_notifier(&device_notifier);
}

ToDevice::ToDevice()
    : _dev_idle(0), _rejected(0), _hard_start(0)
{
    MOD_INC_USE_COUNT;
    todev_static_initialize();
    add_input();
}

ToDevice::~ToDevice()
{
    todev_static_cleanup();
    MOD_DEC_USE_COUNT;
}


int
ToDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 16;
    bool allow_nonexistent = false;
    if (cp_va_parse(conf, this, errh,
		    cpString, "device name", &_devname,
		    cpOptional,
		    cpUnsigned, "burst size", &_burst,
		    cpKeywords,
		    "BURST", cpUnsigned, "burst size", &_burst,
		    "ALLOW_NONEXISTENT", cpBool, "allow nonexistent device?", &allow_nonexistent,
		    cpEnd) < 0)
	return -1;
    return find_device(allow_nonexistent, &to_device_map, errh);
}

int
ToDevice::initialize(ErrorHandler *errh)
{
#ifndef HAVE_CLICK_KERNEL
    errh->warning("not compiled for a Click kernel");
#endif

    // check for duplicate writers
    if (ifindex() >= 0) {
	void *&used = router()->force_attachment("device_writer_" + String(ifindex()));
	if (used)
	    return errh->error("duplicate writer for device `%s'", _devname.cc());
	used = this;
    }

    ScheduleInfo::initialize_task(this, &_task, _dev != 0, errh);
    _signal = Notifier::upstream_pull_signal(this, 0, &_task);

#ifdef HAVE_STRIDE_SCHED
    // user specifies max number of tickets; we start with default
    _max_tickets = _task.tickets();
    _task.set_tickets(Task::DEFAULT_TICKETS);
#endif

    reset_counts();
    return 0;
}

void
ToDevice::reset_counts()
{
  _npackets = 0;
  
  _busy_returns = 0; 
#if CLICK_DEVICE_STATS
  _activations = 0;
  _time_clean = 0;
  _time_freeskb = 0;
  _time_queue = 0;
  _perfcnt1_pull = 0;
  _perfcnt1_clean = 0;
  _perfcnt1_freeskb = 0;
  _perfcnt1_queue = 0;
  _perfcnt2_pull = 0;
  _perfcnt2_clean = 0;
  _perfcnt2_freeskb = 0;
  _perfcnt2_queue = 0;
#endif
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  _pull_cycles = 0;
#endif
}

void
ToDevice::cleanup(CleanupStage)
{
    clear_device(&to_device_map);
}

/*
 * Problem: Linux drivers aren't required to
 * accept a packet even if they've marked themselves
 * as idle. What do we do with a rejected packet?
 */

#if LINUX_VERSION_CODE < 0x020400
# define netif_queue_stopped(dev)	((dev)->tbusy)
# define netif_wake_queue(dev)		mark_bh(NET_BH)
#endif

bool
ToDevice::run_task()
{
    int busy;
    int sent = 0;

#if LINUX_VERSION_CODE >= 0x020400
    local_bh_disable();
    if (!spin_trylock(&_dev->xmit_lock)) {
	local_bh_enable();
	_task.fast_reschedule();
	return false;
    }

    _dev->xmit_lock_owner = smp_processor_id();
#endif

#if CLICK_DEVICE_STATS
    unsigned low00, low10;
    uint64_t time_now;
    SET_STATS(low00, low10, time_now);
#endif
 
#if HAVE_LINUX_POLLING
    bool is_polling = (_dev->polling > 0);
    if (is_polling) {
	struct sk_buff *skbs = _dev->tx_clean(_dev);
# if CLICK_DEVICE_STATS
	if (_activations > 0 && skbs)
	    GET_STATS_RESET(low00, low10, time_now, 
			    _perfcnt1_clean, _perfcnt2_clean, _time_clean);
# endif
	if (skbs)
	    skbmgr_recycle_skbs(skbs);
# if CLICK_DEVICE_STATS
	if (_activations > 0 && skbs)
	    GET_STATS_RESET(low00, low10, time_now, 
			    _perfcnt1_freeskb, _perfcnt2_freeskb, _time_freeskb);
# endif
    }
#endif
  
    /* try to send from click */
    while (sent < _burst && (busy = netif_queue_stopped(_dev)) == 0) {
#if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
	uint64_t before_pull_cycles = click_get_cycles();
#endif

	Packet *p = input(0).pull();
	if (!p)
	    break;

	_npackets++;
#if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
	_pull_cycles += click_get_cycles() - before_pull_cycles - CLICK_CYCLE_COMPENSATION;
#endif

	GET_STATS_RESET(low00, low10, time_now, 
			_perfcnt1_pull, _perfcnt2_pull, _pull_cycles);

	busy = queue_packet(p);

	GET_STATS_RESET(low00, low10, time_now, 
			_perfcnt1_queue, _perfcnt2_queue, _time_queue);

	if (busy)
	    break;
	sent++;
    }

#if HAVE_LINUX_POLLING
    if (is_polling && sent > 0)
	_dev->tx_eob(_dev);

    // If Linux tried to send a packet, but saw tbusy, it will
    // have left it on the queue. It'll just sit there forever
    // (or until Linux sends another packet) unless we poke
    // net_bh(), which calls qdisc_restart(). We are not allowed
    // to call qdisc_restart() ourselves, outside of net_bh().
    if (is_polling && !busy && _dev->qdisc->q.qlen) {
	_dev->tx_eob(_dev);
	netif_wake_queue(_dev);
    }
#endif

#if CLICK_DEVICE_STATS
    if (sent > 0)
	_activations++;
#endif

    if (busy)
	_busy_returns++;

#if HAVE_LINUX_POLLING
    if (is_polling) {
	if (busy && sent == 0) {
	    _dev_idle++;
	    if (_dev_idle == 1024) {
		/* device didn't send anything, ping it */
		_dev->tx_start(_dev);
		_dev_idle = 0;
		_hard_start++;
	    }
	} else
	    _dev_idle = 0;
    }
#endif

#if LINUX_VERSION_CODE >= 0x020400
    spin_unlock(&_dev->xmit_lock);
    local_bh_enable();
#endif
  
    adjust_tickets(sent);
    // If we're polling, never go to sleep! We're relying on ToDevice to clean
    // the transmit ring.
    // Also don't go to sleep if the transmitting device was busy (as opposed
    // to the queue being empty).
    if (is_polling || sent > 0 || busy || _signal)
	_task.fast_reschedule();
    return sent > 0;
}

int
ToDevice::queue_packet(Packet *p)
{
    struct sk_buff *skb1 = p->skb();
  
    /*
     * Ensure minimum ethernet packet size (14 hdr + 46 data).
     * I can't figure out where Linux does this, so I don't
     * know the correct procedure.
     */
    if (skb1->len < 60) {
	if (skb_tailroom(skb1) < 60 - skb1->len) {
	    printk("ToDevice: too small: len %d tailroom %d\n",
		   skb1->len, skb_tailroom(skb1));
	    kfree_skb(skb1);
	    return -1;
	}
	skb_put(skb1, 60 - skb1->len);
    }

    int ret;
#if HAVE_LINUX_POLLING
    if (_dev->polling > 0)
	ret = _dev->tx_queue(_dev, skb1);
    else
#endif
	{
	    ret = _dev->hard_start_xmit(skb1, _dev);
	    _hard_start++;
	}
    if (ret != 0) {
	if (_rejected == 0)
	    printk("<1>ToDevice %s rejected a packet!\n", _dev->name);
	kfree_skb(skb1);
	_rejected++;
    }
    return ret;
}

void
ToDevice::change_device(net_device *dev)
{
    _task.strong_unschedule();
    
    set_device(dev, &to_device_map);

    if (_dev)
	_task.strong_reschedule();
}

extern "C" {
static int
device_notifier_hook(struct notifier_block *nb, unsigned long flags, void *v)
{
#ifdef NETDEV_GOING_DOWN
    if (flags == NETDEV_GOING_DOWN)
	flags = NETDEV_DOWN;
#endif
    if (flags == NETDEV_DOWN || flags == NETDEV_UP) {
	bool down = (flags == NETDEV_DOWN);
	net_device *dev = (net_device *)v;
	Vector<AnyDevice *> es;
	to_device_map.lookup_all(dev, down, es);
	for (int i = 0; i < es.size(); i++)
	    ((ToDevice *)(es[i]))->change_device(down ? 0 : dev);
    }
    return 0;
}
}

static String
ToDevice_read_calls(Element *f, void *)
{
    ToDevice *td = (ToDevice *)f;
    return
	String(td->_rejected) + " packets rejected\n" +
	String(td->_hard_start) + " hard start xmit\n" +
	String(td->_busy_returns) + " device busy returns\n" +
	String(td->_npackets) + " packets sent\n" +
#if CLICK_DEVICE_STATS
	String(td->_pull_cycles) + " cycles pull\n" +
	String(td->_time_clean) + " cycles clean\n" +
	String(td->_time_freeskb) + " cycles freeskb\n" +
	String(td->_time_queue) + " cycles queue\n" +
	String(td->_perfcnt1_pull) + " perfctr1 pull\n" +
	String(td->_perfcnt1_clean) + " perfctr1 clean\n" +
	String(td->_perfcnt1_freeskb) + " perfctr1 freeskb\n" +
	String(td->_perfcnt1_queue) + " perfctr1 queue\n" +
	String(td->_perfcnt2_pull) + " perfctr2 pull\n" +
	String(td->_perfcnt2_clean) + " perfctr2 clean\n" +
	String(td->_perfcnt2_freeskb) + " perfctr2 freeskb\n" +
	String(td->_perfcnt2_queue) + " perfctr2 queue\n" +
	String(td->_activations) + " transmit activations\n"
#else
	String()
#endif
	;
}

static String
ToDevice_read_stats(Element *e, void *thunk)
{
  ToDevice *td = (ToDevice *)e;
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(td->_npackets) + "\n";
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
   case 1:
    return String(td->_pull_cycles) + "\n";
#endif
#if CLICK_DEVICE_STATS
   case 2:
    return String(td->_time_queue) + "\n";
   case 3:
    return String(td->_time_clean) + "\n";
#endif
   default:
    return String();
  }
}

static int
ToDevice_write_stats(const String &, Element *e, void *, ErrorHandler *)
{
  ToDevice *td = (ToDevice *)e;
  td->reset_counts();
  return 0;
}

void
ToDevice::add_handlers()
{
    add_read_handler("calls", ToDevice_read_calls, 0);
    add_read_handler("packets", ToDevice_read_stats, 0);
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
    add_read_handler("pull_cycles", ToDevice_read_stats, (void *)1);
#endif
#if CLICK_DEVICE_STATS
    add_read_handler("enqueue_cycles", ToDevice_read_stats, (void *)2);
    add_read_handler("clean_dma_cycles", ToDevice_read_stats, (void *)3);
#endif
    add_write_handler("reset_counts", ToDevice_write_stats, 0);
    add_task_handlers(&_task);
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(ToDevice)
