// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdevice.{cc,hh} -- element steals packets from Linux devices using
 * register_net_in
 * Robert Morris
 * Eddie Kohler: AnyDevice, other changes
 * Benjie Chen: scheduling, internal queue
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
#include "fromdevice.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

static AnyDeviceMap from_device_map;
static int registered_readers;
#ifdef HAVE_CLICK_KERNEL
static struct notifier_block packet_notifier;
#endif
static struct notifier_block device_notifier;
static int from_device_count;

extern "C" {
#ifdef HAVE_CLICK_KERNEL
static int packet_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
#endif
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}

static void
fromdev_static_initialize()
{
    if (++from_device_count == 1) {
	from_device_map.initialize();
#ifdef HAVE_CLICK_KERNEL
	packet_notifier.notifier_call = packet_notifier_hook;
	packet_notifier.priority = 1;
#endif
	device_notifier.notifier_call = device_notifier_hook;
	device_notifier.priority = 1;
	device_notifier.next = 0;
	register_netdevice_notifier(&device_notifier);
    }
}

static void
fromdev_static_cleanup()
{
    if (--from_device_count <= 0) {
#ifdef HAVE_CLICK_KERNEL
	if (registered_readers)
	    unregister_net_in(&packet_notifier);
#endif
	unregister_netdevice_notifier(&device_notifier);
    }
}

FromDevice::FromDevice()
{
    MOD_INC_USE_COUNT;
    fromdev_static_initialize();
    add_output();
    _head = _tail = 0;
}

FromDevice::~FromDevice()
{
    fromdev_static_cleanup();
    MOD_DEC_USE_COUNT;
}

void *
FromDevice::cast(const char *n)
{
    if (strcmp(n, "Storage") == 0)
	return (Storage *)this;
    else if (strcmp(n, "FromDevice") == 0)
	return (Element *)this;
    else
	return 0;
}

int
FromDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool promisc = false;
    bool allow_nonexistent = false;
    _burst = 8;
    if (cp_va_parse(conf, this, errh, 
		    cpString, "interface name", &_devname, 
		    cpOptional,
		    cpBool, "enter promiscuous mode?", &promisc,
		    cpUnsigned, "burst size", &_burst,
		    cpKeywords,
		    "PROMISC", cpBool, "enter promiscuous mode?", &promisc,
		    "PROMISCUOUS", cpBool, "enter promiscuous mode?", &promisc,
		    "BURST", cpUnsigned, "burst size", &_burst,
		    "ALLOW_NONEXISTENT", cpBool, "allow nonexistent interface?", &allow_nonexistent,
		    cpEnd) < 0)
	return -1;
    if (promisc)
	set_flag(F_PROMISC);
    
    return find_device(allow_nonexistent, &from_device_map, errh);
}

/*
 * Use a Linux interface added by us, in net/core/dev.c,
 * to register to grab incoming packets.
 */
int
FromDevice::initialize(ErrorHandler *errh)
{
    // check for duplicate readers
    if (ifindex() >= 0) {
	void *&used = router()->force_attachment("device_reader_" + String(ifindex()));
	if (used)
	    return errh->error("duplicate reader for device `%s'", _devname.cc());
	used = this;
    }

    if (!registered_readers) {
#ifdef HAVE_CLICK_KERNEL
	packet_notifier.next = 0;
	register_net_in(&packet_notifier);
#else
	errh->warning("can't get packets: not compiled for a Click kernel");
#endif
    }
    registered_readers++;

    ScheduleInfo::initialize_task(this, &_task, _dev != 0, errh);
#ifdef HAVE_STRIDE_SCHED
    // user specifies max number of tickets; we start with default
    _max_tickets = _task.tickets();
    _task.set_tickets(Task::DEFAULT_TICKETS);
#endif

    from_device_map.move_to_front(this);
    _capacity = QSIZE;
    _drops = 0;
    return 0;
}

void
FromDevice::cleanup(CleanupStage stage)
{
    if (stage >= CLEANUP_INITIALIZED) {
	registered_readers--;
#ifdef HAVE_CLICK_KERNEL
	if (registered_readers == 0)
	    unregister_net_in(&packet_notifier);
#endif
    }
    
    clear_device(&from_device_map);
    
    for (unsigned i = _head; i != _tail; i = next_i(i))
	_queue[i]->kill();
    _head = _tail = 0;    
}

void
FromDevice::take_state(Element *e, ErrorHandler *errh)
{
  FromDevice *fd = (FromDevice *)e->cast("FromDevice");
  if (!fd) return;
  
  if (_head != _tail) {
    errh->error("already have packets enqueued, can't take state");
    return;
  }

  memcpy(_queue, fd->_queue, sizeof(Packet *) * (QSIZE + 1));
  _head = fd->_head;
  _tail = fd->_tail;
  
  fd->_head = fd->_tail = 0;
}

void
FromDevice::change_device(net_device *dev)
{
    set_device(dev, &from_device_map);
}

/*
 * Called by Linux net_bh[2.2]/net_rx_action[2.4] with each packet.
 */
extern "C" {

#ifdef HAVE_CLICK_KERNEL
static int
packet_notifier_hook(struct notifier_block *nb, unsigned long backlog_len, void *v)
{
  struct sk_buff *skb = (struct sk_buff *)v;

  int stolen = 0;
  if (FromDevice *fd = (FromDevice *)from_device_map.lookup(skb->dev, 0))
      stolen = fd->got_skb(skb);
  
  return (stolen ? NOTIFY_STOP_MASK : 0);
}
#endif

static int
device_notifier_hook(struct notifier_block *nb, unsigned long flags, void *v)
{
    if (flags == NETDEV_DOWN || flags == NETDEV_UP) {
	bool down = (flags == NETDEV_DOWN);
	net_device *dev = (net_device *)v;
	Vector<AnyDevice *> es;
	from_device_map.lookup_all(dev, down, es);
	for (int i = 0; i < es.size(); i++)
	    ((FromDevice *)(es[i]))->change_device(down ? 0 : dev);
    }
    return 0;
}

}

/*
 * Per-FromDevice packet input routine.
 */
int
FromDevice::got_skb(struct sk_buff *skb)
{
    unsigned next = next_i(_tail);

    if (next != _head) { /* ours */
	assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

	/* Retrieve the MAC header. */
	skb_push(skb, skb->data - skb->mac.raw);

	Packet *p = Packet::make(skb);
	_queue[_tail] = p; /* hand it to run_scheduled */
	_tail = next;

    } else {
	/* queue full, drop */
	kfree_skb(skb);
	_drops++;
    }

    return 1;
}

void
FromDevice::run_scheduled()
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
}

void
FromDevice::add_handlers()
{
    add_task_handlers(&_task);
}

ELEMENT_REQUIRES(AnyDevice Storage linuxmodule)
EXPORT_ELEMENT(FromDevice)
