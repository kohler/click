/*
 * fromdevice.{cc,hh} -- element steals packets from Linux devices using
 * register_net_in
 * Robert Morris
 * Eddie Kohler: register once per configuration
 * Benjie Chen: scheduling, internal queue
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
#include "fromdevice.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "router.hh"
#include "elements/standard/scheduleinfo.hh"

static AnyDeviceMap from_device_map;
static int registered_readers;
static struct notifier_block notifier;

extern "C" int click_FromDevice_in
  (struct notifier_block *nb, unsigned long val, void *v);


FromDevice::FromDevice()
  : _registered(0), _puller_ptr(0), _pusher_ptr(0), _drops(0)
{
  add_output();
}

FromDevice::~FromDevice()
{
  if (_registered) uninitialize();
}

void
FromDevice::static_initialize()
{
  notifier.notifier_call = click_FromDevice_in;
  notifier.priority = 1;
  from_device_map.initialize();
}

void
FromDevice::static_cleanup()
{
#ifdef HAVE_CLICK_KERNEL
  if (registered_readers)
    unregister_net_in(&notifier);
#endif
}


int
FromDevice::configure(const String &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh, 
	          cpString, "interface name", &_devname, 
		  cpEnd) < 0)
    return -1;
  _dev = dev_get(_devname.cc());
  if (!_dev)
    _dev = find_device_by_ether_address(_devname);
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
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
  
  _registered = 1;
  
  if (!registered_readers) {
#ifdef HAVE_CLICK_KERNEL
    notifier.next = 0;
    register_net_in(&notifier);
#else
    errh->warning("can't get packets: not compiled for a Click kernel");
#endif
  }
  registered_readers++;
  
#ifndef RR_SCHED
    // start out with default number of tickets, inflate up to max
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(ScheduleInfo::DEFAULT);
#endif
  join_scheduler();
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
  _registered = 0;
  unschedule();
  for (unsigned i = _puller_ptr; i != _pusher_ptr; i = next_i(i))
    _queue[i]->kill();
  _puller_ptr = _pusher_ptr = 0;
}

void
FromDevice::take_state(Element *e, ErrorHandler *errh)
{
  FromDevice *fd = (FromDevice *)e->cast("FromDevice");
  if (!fd) return;
  
  if (_puller_ptr != _pusher_ptr) {
    errh->error("already have packets enqueued, can't take state");
    return;
  }

  memcpy(_queue, fd->_queue, sizeof(Packet *) * FROMDEV_QSIZE);
  _puller_ptr = fd->_puller_ptr;
  _pusher_ptr = fd->_pusher_ptr;
  
  fd->_puller_ptr = fd->_pusher_ptr = 0;
}

/*
 * Called by net_bh() with each packet.
 */
extern "C" int
click_FromDevice_in(struct notifier_block *nb, unsigned long backlog_len,
		    void *v)
{
#if 0 && !HAVE_POLLING
  static int called_times;
#endif
  struct sk_buff *skb = (struct sk_buff *)v;

  int stolen = 0;
  int ifindex = skb->dev->ifindex;
  if (ifindex >= 0 && ifindex < MAX_DEVICES)
    if (FromDevice *kr = (FromDevice *)from_device_map.lookup(ifindex)) {
      stolen = kr->got_skb(skb);

#if 0 && !HAVE_POLLING
      // Call scheduled things - in a nonpolling environment, this is
      // important because we really don't want packets to sit in a queue
      // under high load w/o having ToDevice pull them out of there. This
      // causes a bit of batching, so that not every packet causes Queue to
      // put ToDevice on the work list.  What about the last packet? If
      // net_bh's backlog queue is empty, we want to run_scheduled().
      called_times++;
      if (called_times == 4 || backlog_len == 0) {
	extern Router *current_router; // module.cc
	current_router->driver_once();
	current_router->driver_once();
	current_router->driver_once();
	current_router->driver_once();
	called_times = 0;
      }
#endif
    }
  
  return (stolen ? NOTIFY_STOP_MASK : 0);
}

/*
 * Per-FromDevice packet input routine.
 */
int
FromDevice::got_skb(struct sk_buff *skb)
{
  unsigned next = next_i(_pusher_ptr);
  
  if (next != _puller_ptr) { /* ours */
    assert(skb->data - skb->head >= 14);
    assert(skb->mac.raw == skb->data - 14);
    assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

    /* Retrieve the ether header. */
    skb_push(skb, 14);

    Packet *p = Packet::make(skb);
    if(skb->pkt_type == PACKET_MULTICAST || skb->pkt_type == PACKET_BROADCAST)
      p->set_mac_broadcast_anno(1);

    _queue[_pusher_ptr] = p; /* hand it to run_scheduled */
    _pusher_ptr = next;

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
  int i=0;
  while (i < INPUT_MAX_PKTS_PER_RUN && _puller_ptr != _pusher_ptr) {
    Packet *p = _queue[_puller_ptr];
    _puller_ptr = next_i(_puller_ptr);
    output(0).push(p);
  }

#ifndef RR_SCHED
#ifdef ADJ_TICKETS
  int adj = tickets() / 8;
  if (adj < 4) adj = 4;
  
  if (i == INPUT_MAX_PKTS_PER_RUN) adj *= 2;
  if (i < INPUT_MAX_PKTS_PER_RUN/4) adj = 0 - adj;

  adj_tickets(adj);
#endif /* ADJ_TICKETS */
  reschedule();
#endif /* RR_SCHED */
}

EXPORT_ELEMENT(FromDevice)
ELEMENT_REQUIRES(AnyDevice)
