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
#include "elements/standard/scheduleinfo.hh"

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
  // no MOD_INC_USE_COUNT; rely on AnyDevice
  add_output();
  fromdev_static_initialize();
}

FromDevice::~FromDevice()
{
    // no MOD_DEC_USE_COUNT; rely on AnyDevice
    fromdev_static_cleanup();
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
FromDevice::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    _promisc = false;
    bool allow_nonexistent = false;
    _burst = 8;
    if (cp_va_parse(conf, this, errh, 
		    cpString, "interface name", &_devname, 
		    cpOptional,
		    cpBool, "set to promiscuous?", &_promisc,
		    cpUnsigned, "burst size", &_burst,
		    cpKeywords,
		    "PROMISC", cpBool, "set to promiscuous?", &_promisc,
		    "PROMISCUOUS", cpBool, "set to promiscuous?", &_promisc,
		    "BURST", cpUnsigned, "burst size", &_burst,
		    "ALLOW_NONEXISTENT", cpBool, "allow nonexistent interface?", &allow_nonexistent,
		    cpEnd) < 0)
	return -1;
    
    _dev = dev_get_by_name(_devname.cc());
    if (!_dev)
	_dev = find_device_by_ether_address(_devname, this);
    if (!_dev) {
	if (!allow_nonexistent)
	    return errh->error("unknown device `%s'", _devname.cc());
	else
	    errh->warning("unknown device `%s'", _devname.cc());
    }
    if (_dev && !(_dev->flags & IFF_UP)) {
	errh->warning("device `%s' is down", _devname.cc());
	_dev = 0;
    }
    
    return 0;
}

/*
 * Use a Linux interface added by us, in net/core/dev.c,
 * to register to grab incoming packets.
 */
int
FromDevice::initialize(ErrorHandler *errh)
{
    // check for duplicates; FromDevice <-> PollDevice conflicts checked by
    // PollDevice
    if (ifindex() >= 0)
	for (int fi = 0; fi < router()->nelements(); fi++) {
	    Element *e = router()->element(fi);
	    if (e == this) continue;
	    if (FromDevice *fd = (FromDevice *)(e->cast("FromDevice"))) {
		if (fd->ifindex() == ifindex())
		    return errh->error("duplicate FromDevice for `%s'", _devname.cc());
	    }
	}

    if (from_device_map.insert(this) < 0)
	return errh->error("cannot use FromDevice for device `%s'", _devname.cc());
    if (_promisc && _dev)
	dev_set_promiscuity(_dev, 1);
    
    if (!registered_readers) {
#ifdef HAVE_CLICK_KERNEL
	packet_notifier.next = 0;
	register_net_in(&packet_notifier);
#else
	errh->warning("can't get packets: not compiled for a Click kernel");
#endif
    }
    registered_readers++;

#ifdef HAVE_STRIDE_SCHED
    // start out with default number of tickets, inflate up to max
    _max_tickets = ScheduleInfo::query(this, errh);
#endif
    _task.initialize(this, true);

    _head = _tail = 0;
    _capacity = QSIZE;
    _drops = 0;
    return 0;
}

void
FromDevice::uninitialize()
{
    registered_readers--;
#ifdef HAVE_CLICK_KERNEL
    if (registered_readers == 0)
	unregister_net_in(&packet_notifier);
#endif
    
    _task.unschedule();
    
    from_device_map.remove(this);
    if (_promisc && _dev)
	dev_set_promiscuity(_dev, -1);
    
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
    if (!_dev && dev)
	click_chatter("%s: device `%s' came up", declaration().cc(), _devname.cc());
    else if (_dev && !dev)
	click_chatter("%s: device `%s' went down", declaration().cc(), _devname.cc());
    
    from_device_map.remove(this);
    if (_promisc && _dev)
	dev_set_promiscuity(_dev, -1);
    _dev = dev;
    if (_promisc && _dev)
	dev_set_promiscuity(_dev, 1);
    from_device_map.insert(this);
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
  int ifindex = skb->dev->ifindex;
  if (FromDevice *fd = (FromDevice *)from_device_map.lookup(ifindex))
      stolen = fd->got_skb(skb);
  
  return (stolen ? NOTIFY_STOP_MASK : 0);
}
#endif

static int
device_notifier_hook(struct notifier_block *nb, unsigned long flags, void *v)
{
    net_device *dev = (net_device *)v;

    if (flags == NETDEV_UP) {
	if (FromDevice *fd = (FromDevice *)from_device_map.lookup_unknown(dev))
	    fd->change_device(dev);
    } else if (flags == NETDEV_DOWN) {
	if (FromDevice *fd = (FromDevice *)from_device_map.lookup(dev->ifindex))
	    fd->change_device(0);
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
