/*
 * fromdevice.{cc,hh} -- element steals packets from Linux devices using
 * register_net_in
 * Robert Morris
 * Eddie Kohler : register once per configuration
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
#include "polldevice.hh"
#include "fromdevice.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "router.hh"
#include "elements/standard/scheduleinfo.hh"
extern "C" {
#include <linux/netdevice.h>
}
#include "netdev.h"

static int registered_readers;
static struct notifier_block notifier;
static unsigned first_time = 1;
static void* ifindex_map[MAX_DEVICES][DEV_ELEM_TYPES];

extern "C" int click_FromDevice_in
  (struct notifier_block *nb, unsigned long val, void *v);


FromDevice::FromDevice()
  : _dev(0), _registered(0), _puller_ptr(0), _pusher_ptr(0), _drops(0)
{
  add_output();
  for(int i=0;i<FROMDEV_QSIZE;i++) _queue[i] = 0;
}

FromDevice::FromDevice(const String &devname)
  : _devname(devname), _registered(0), _dev(0),
    _puller_ptr(0), _pusher_ptr(0), _drops(0)
{
  add_output();
  for(int i=0;i<FROMDEV_QSIZE;i++) _queue[i] = 0;
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
}

void
FromDevice::static_cleanup()
{
#ifdef HAVE_CLICK_KERNEL
  if (registered_readers)
    unregister_net_in(&notifier);
#endif
}


FromDevice *
FromDevice::clone() const
{
  return new FromDevice();
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
  /* can't have a PollDevice with the same device */
  for(int fi = 0; fi < router()->nelements(); fi++) {
    Element *f = router()->element(fi);
    PollDevice *pd = (PollDevice *)(f->cast("PollDevice"));
    if (pd && pd->ifnum() == _dev->ifindex)
      return errh->error("have PollDevice and FromDevice for device `%s'", 
	                  _devname.cc());
  }

  void *p = update_ifindex_map(_dev->ifindex, errh, FROMDEV_OBJ, this);
  if (p == 0) return -1;
  else if (p != this)
    return errh->error("duplicate FromDevice for device `%s'", _devname.cc());
  
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
  remove_ifindex_map(_dev->ifindex, FROMDEV_OBJ);
  _registered = 0;
  unschedule();
}


/*
 * Called by net_bh() with each packet.
 */
extern "C" int
click_FromDevice_in(struct notifier_block *nb, unsigned long backlog_len,
		    void *v)
{
  static int called_times;
  struct sk_buff *skb = (struct sk_buff *)v;

  int stolen = 0;
  int ifindex = skb->dev->ifindex;
  if (ifindex >= 0 && ifindex < MAX_DEVICES)
    if (FromDevice *kr = 
	(FromDevice*) lookup_ifindex_map(ifindex, FROMDEV_OBJ)) {
      stolen = kr->got_skb(skb);

#ifndef HAVE_POLLING
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
  if (_queue[_pusher_ptr] == 0) { /* ours */
    assert(skb->data - skb->head >= 14);
    assert(skb->mac.raw == skb->data - 14);
    assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

    /* Retrieve the ether header. */
    skb_push(skb, 14);

    Packet *p = Packet::make(skb);
    if(skb->pkt_type == PACKET_MULTICAST || skb->pkt_type == PACKET_BROADCAST)
      p->set_mac_broadcast_anno(1);

    _queue[_pusher_ptr] = p; /* hand it to run_scheduled */
    _pusher_ptr = next_i(_pusher_ptr);

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
  while(i<INPUT_MAX_PKTS_PER_RUN && _queue[_puller_ptr] != 0) {
    Packet *p = _queue[_puller_ptr];
    _queue[_puller_ptr] = 0; /* give it back to got_skb() */
    _puller_ptr = next_i(_puller_ptr);
    output(0).push(p);
  }

#ifndef RR_SCHED
#ifdef ADJ_TICKETS
  int adj = tickets() / 4;
  if (adj < 4) adj = 4;
  
  if (i == INPUT_MAX_PKTS_PER_RUN);
  else if (i < INPUT_MAX_PKTS_PER_RUN/4) adj = 0 - adj;
  else adj = 0;

  adj_tickets(adj);
#endif /* ADJ_TICKETS */
  reschedule();
#endif /* RR_SCHED */
}

EXPORT_ELEMENT(FromDevice)
