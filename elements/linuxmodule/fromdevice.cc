/*
 * fromdevice.{cc,hh} -- element steals packets from Linux devices using
 * register_net_in
 * Robert Morris
 * Eddie Kohler: register once per configuration
 * Benjie Chen: scheduling, internal queue
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
#include "fromdevice.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include "elements/standard/scheduleinfo.hh"

static AnyDeviceMap from_device_map;
static int registered_readers;
static struct notifier_block notifier;
static int from_device_count;

extern "C" int click_FromDevice_in
  (struct notifier_block *nb, unsigned long val, void *v);


static void
fromdev_static_initialize()
{
  from_device_count++;
  if (from_device_count > 1) return;
  notifier.notifier_call = click_FromDevice_in;
  notifier.priority = 1;
  from_device_map.initialize();
}

static void
fromdev_static_cleanup()
{
  from_device_count--;
  if (from_device_count > 0) return;
#ifdef HAVE_CLICK_KERNEL
  if (registered_readers)
    unregister_net_in(&notifier);
#endif
}

FromDevice::FromDevice()
  : _registered(false), _drops(0)
{
  // no MOD_INC_USE_COUNT; rely on AnyDevice
  add_output();
  _promisc = 0;
  fromdev_static_initialize();
}

FromDevice::~FromDevice()
{
  // no MOD_DEC_USE_COUNT; rely on AnyDevice
  if (_registered) uninitialize();
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
  _burst = 8;
  if (cp_va_parse(conf, this, errh, 
	          cpString, "interface name", &_devname, 
		  cpOptional,
		  cpBool, "promiscuous", &_promisc,
		  cpUnsigned, "burst", &_burst,
		  cpEnd) < 0)
    return -1;
  _dev = dev_get_by_name(_devname.cc());
  if (!_dev)
    _dev = find_device_by_ether_address(_devname, this);
  if (!_dev)
    return errh->error("unknown device `%s'", _devname.cc());
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
  for (int fi = 0; fi < router()->nelements(); fi++) {
    Element *e = router()->element(fi);
    if (e == this) continue;
    if (FromDevice *fd=(FromDevice *)(e->cast("FromDevice"))) {
      if (fd->ifindex() == ifindex())
	return errh->error("duplicate FromDevice for `%s'", _devname.cc());
    }
  }

  if (from_device_map.insert(this) < 0)
    return errh->error("cannot use FromDevice for device `%s'", _devname.cc());
  
  _registered = true;
  
  if (!registered_readers) {
#ifdef HAVE_CLICK_KERNEL
    notifier.next = 0;
    register_net_in(&notifier);
#else
    errh->warning("can't get packets: not compiled for a Click kernel");
#endif
  }
  registered_readers++;
  
  if (_promisc) dev_set_promiscuity(_dev, 1);
  
#ifndef RR_SCHED
  // start out with default number of tickets, inflate up to max
  _max_tickets = ScheduleInfo::query(this, errh);
#endif
  _task.initialize(this, true);

  _head = _tail = 0;
  _capacity = QSIZE;
  return 0;
}

void
FromDevice::uninitialize()
{
  registered_readers--;
#ifdef HAVE_CLICK_KERNEL
  if (registered_readers == 0)
    unregister_net_in(&notifier);
#endif
  if (_registered)
    from_device_map.remove(this);
  _registered = false;
  _task.unschedule();
  for (unsigned i = _head; i != _tail; i = next_i(i))
    _queue[i]->kill();
  _head = _tail = 0;

  if (_promisc) dev_set_promiscuity(_dev, -1);
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

/*
 * Called by net_bh() with each packet.
 */
extern "C" int
click_FromDevice_in(struct notifier_block *nb, unsigned long backlog_len,
		    void *v)
{
  struct sk_buff *skb = (struct sk_buff *)v;

  int stolen = 0;
  int ifindex = skb->dev->ifindex;
  if (ifindex >= 0 && ifindex < MAX_DEVICES)
    if (FromDevice *kr = (FromDevice *)from_device_map.lookup(ifindex)) {
      stolen = kr->got_skb(skb);
    }
  
  return (stolen ? NOTIFY_STOP_MASK : 0);
}

/*
 * Per-FromDevice packet input routine.
 */
int
FromDevice::got_skb(struct sk_buff *skb)
{
  unsigned next = next_i(_tail);
  
  if (next != _head) { /* ours */
    assert(skb->data - skb->head >= 14);
    assert(skb->mac.raw == skb->data - 14);
    assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

    /* Retrieve the ether header. */
    skb_push(skb, 14);

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
