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
#include <click/straccum.hh>

static AnyDeviceMap from_device_map;
static int registered_readers;
#ifdef HAVE_CLICK_KERNEL
static struct notifier_block packet_notifier;
#endif
static struct notifier_block device_notifier;

extern "C" {
#ifdef HAVE_CLICK_KERNEL
static int packet_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
#endif
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}

void
FromDevice::static_initialize()
{
    from_device_map.initialize();
#ifdef HAVE_CLICK_KERNEL
    packet_notifier.notifier_call = packet_notifier_hook;
    packet_notifier.priority = 1;
    packet_notifier.next = 0;
#endif
    device_notifier.notifier_call = device_notifier_hook;
    device_notifier.priority = 1;
    device_notifier.next = 0;
    register_netdevice_notifier(&device_notifier);
}

void
FromDevice::static_cleanup()
{
#ifdef HAVE_CLICK_KERNEL
    if (registered_readers)
	unregister_net_in(&packet_notifier);
#endif
    unregister_netdevice_notifier(&device_notifier);
}

FromDevice::FromDevice()
{
    _head = _tail = 0;
}

FromDevice::~FromDevice()
{
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
		    cpString, "device name", &_devname, 
		    cpOptional,
		    cpBool, "enter promiscuous mode?", &promisc,
		    cpUnsigned, "burst size", &_burst,
		    cpKeywords,
		    "PROMISC", cpBool, "enter promiscuous mode?", &promisc,
		    "PROMISCUOUS", cpBool, "enter promiscuous mode?", &promisc,
		    "BURST", cpUnsigned, "burst size", &_burst,
		    "ALLOW_NONEXISTENT", cpBool, "allow nonexistent device?", &allow_nonexistent,
		    cpEnd) < 0)
	return -1;
    if (promisc)
	set_promisc();
    
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
	    return errh->error("duplicate reader for device '%s'", _devname.c_str());
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

    reset_counts();

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
    if (FromDevice *fd = (FromDevice *)e->cast("FromDevice")) {
	if (_head != _tail) {
	    errh->error("already have packets enqueued, can't take state");
	    return;
	}

	memcpy(_queue, fd->_queue, sizeof(Packet *) * (QSIZE + 1));
	_head = fd->_head;
	_tail = fd->_tail;
  
	fd->_head = fd->_tail = 0;
    }
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
#ifdef NETDEV_GOING_DOWN
    if (flags == NETDEV_GOING_DOWN)
	flags = NETDEV_DOWN;
#endif
    if (flags == NETDEV_DOWN || flags == NETDEV_UP) {
	bool down = (flags == NETDEV_DOWN);
	net_device* dev = (net_device*)v;
	Vector<AnyDevice*> es;
	from_device_map.lookup_all(dev, down, es);
	for (int i = 0; i < es.size(); i++)
	    ((FromDevice*)(es[i]))->change_device(down ? 0 : dev);
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
	_queue[_tail] = p; /* hand it to run_task */

#if CLICK_DEBUG_SCHEDULING
	click_gettimeofday(&_schinfo[_tail].enq_time);
	RouterThread *rt = _task.thread();
	_schinfo[_tail].enq_state = rt->thread_state();
	int enq_process_asleep = rt->sleeper() && rt->sleeper()->state != TASK_RUNNING;
	_schinfo[_tail].enq_task_scheduled = _task.scheduled();
	_schinfo[_tail].enq_epoch = rt->driver_epoch();
	_schinfo[_tail].enq_task_epoch = rt->driver_task_epoch();
#endif
	
	_tail = next;
	_task.reschedule();

#if CLICK_DEBUG_SCHEDULING
	_schinfo[_tail].enq_woke_process = enq_process_asleep && rt->sleeper()->state == TASK_RUNNING;
#endif

    } else {
	/* queue full, drop */
	kfree_skb(skb);
	_drops++;
    }

    return 1;
}

#if CLICK_DEBUG_SCHEDULING
void
FromDevice::emission_report(int idx)
{
    struct timeval now;
    click_gettimeofday(&now);
    RouterThread *rt = _task.thread();
    StringAccum sa;
    sa << "dt " << (now - _schinfo[idx].enq_time);
    if (_schinfo[idx].enq_state != RouterThread::S_RUNNING) {
	struct timeval etime = rt->task_epoch_time(_schinfo[idx].enq_task_epoch + 1);
	if (timerisset(&etime))
	    sa << " dt_thread " << (etime - _schinfo[idx].enq_time);
    }
    sa << " arrst " << RouterThread::thread_state_name(_schinfo[idx].enq_state)
       << " depoch " << (rt->driver_epoch() - _schinfo[idx].enq_epoch)
       << " dtepoch " << (rt->driver_task_epoch() - _schinfo[idx].enq_task_epoch);
    if (_schinfo[idx].enq_woke_process)
	sa << " woke";
    if (_schinfo[idx].enq_task_scheduled)
	sa << " tasksched";
    
    click_chatter("%s packet: %s", id().c_str(), sa.c_str()); 
}
#endif

bool
FromDevice::run_task()
{
    _runs++;
    int npq = 0;
    while (npq < _burst && _head != _tail) {
	Packet *p = _queue[_head];
#if CLICK_DEBUG_SCHEDULING
	emission_report(_head);
#endif
	_head = next_i(_head);
	output(0).push(p);
	npq++;
	_pushes++;
    }
    if (npq == 0)
	_empty_runs++;
#if CLICK_DEVICE_ADJUST_TICKETS
    adjust_tickets(npq);
#endif
    if (npq > 0)
	_task.fast_reschedule();
    return npq > 0;
}

void
FromDevice::reset_counts()
{
    _runs = 0;
    _empty_runs = 0;
    _pushes = 0;
}

static int
FromDevice_write_stats(const String &, Element *e, void *, ErrorHandler *)
{
    FromDevice *fd = (FromDevice *) e;
    fd->reset_counts();
    return 0;
}

static String
FromDevice_read_stats(Element *e, void *thunk)
{
    FromDevice *fd = (FromDevice *) e;
    switch (reinterpret_cast<intptr_t>(thunk)) {
    case 0: return String(fd->drops()) + "\n"; break;
    case 1: {
	StringAccum sa;
	sa << "calls to run_task(): " << fd->runs() << "\n"
	   << "calls to push():     " << fd->pushes() << "\n"
	   << "empty runs:          " << fd->empty_runs() << "\n"
	   << "drops:               " << fd->drops() << "\n";
	return sa.take_string();
	break;
    }
    default: 
	return String();
    }	
}

void
FromDevice::add_handlers()
{
    add_task_handlers(&_task);
    add_read_handler("drops", FromDevice_read_stats, (void *) 0);
    add_read_handler("calls", FromDevice_read_stats, (void *) 1);
    add_write_handler("reset_counts", FromDevice_write_stats, 0);
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(FromDevice)
