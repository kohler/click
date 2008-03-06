// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdevice.{cc,hh} -- element steals packets from Linux devices using
 * register_net_in
 * Eddie Kohler
 * Robert Morris
 * Benjie Chen: scheduling, internal queue
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2007-2008 Regents of the University of California
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
    _burst = 8;
    _active = true;
    if (AnyDevice::configure_keywords(conf, errh, true) < 0
	|| cp_va_kparse(conf, this, errh,
			"DEVNAME", cpkP+cpkM, cpString, &_devname,
			"BURST", cpkP, cpUnsigned, &_burst,
			"ACTIVE", 0, cpBool, &_active,
			cpEnd) < 0)
	return -1;

    // make queue look full so packets sent to us are ignored
    _head = _tail = _capacity = 0;

    return find_device(&from_device_map, errh);
}

/*
 * Use a Linux interface added by us, in net/core/dev.c,
 * to register to grab incoming packets.
 */
int
FromDevice::initialize(ErrorHandler *errh)
{
    if (AnyDevice::initialize_keywords(errh) < 0)
	return -1;
    
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

    reset_counts();

    ScheduleInfo::initialize_task(this, &_task, _dev != 0, errh);
#if HAVE_STRIDE_SCHED
    // user specifies max number of tickets; we start with default
    _max_tickets = _task.tickets();
    _task.set_tickets(Task::DEFAULT_TICKETS);
#endif

    // set true queue size (now we can start receiving packets)
    _capacity = QSIZE;
    
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

    if (stage >= CLEANUP_INITIALIZED)
	for (unsigned i = _head; i != _tail; i = next_i(i))
	    _queue[i]->kill();
    _head = _tail = 0;
}

void
FromDevice::take_state(Element *e, ErrorHandler *errh)
{
    if (FromDevice *fd = (FromDevice *)e->cast("FromDevice")) {
	SpinlockIRQ::flags_t flags;
	local_irq_save(flags);

	unsigned fd_i = fd->_head;
	while (fd_i != fd->_tail) {
	    unsigned next = next_i(_tail);
	    if (next == _head)
		break;
	    _queue[_tail] = fd->_queue[fd_i];
	    fd_i = fd->next_i(fd_i);
	    _tail = next;
	}
	for (; fd_i != fd->_tail; fd_i = fd->next_i(fd_i))
	    fd->_queue[fd_i]->kill();
	if (_head != _tail)
	    _task.reschedule();

	fd->_head = fd->_tail = fd->_capacity = 0;

	local_irq_restore(flags);
    }
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
    FromDevice *fd = 0;
    from_device_map.lock(false);
    while (stolen == 0 && (fd = (FromDevice *)from_device_map.lookup(skb->dev, fd)))
	stolen = fd->got_skb(skb);
    from_device_map.unlock(false);
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
    if (flags == NETDEV_DOWN || flags == NETDEV_UP || flags == NETDEV_CHANGE) {
	bool down = (flags == NETDEV_DOWN);
	net_device* dev = (net_device*)v;
	Vector<AnyDevice*> es;
	from_device_map.lock(true);
	from_device_map.lookup_all(dev, down, es);
	for (int i = 0; i < es.size(); i++)
	    ((FromDevice*)(es[i]))->set_device(down ? 0 : dev, &from_device_map, true);
	from_device_map.unlock(true);
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
    if (!_active)
	return 0;		// 0 means not handled
    
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

    } else if (_capacity > 0) {
	/* queue full, drop */
	kfree_skb(skb);
	_drops++;
	
    } else // not yet initialized
	return 0;

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
    
    click_chatter("%s packet: %s", name().c_str(), sa.c_str()); 
}
#endif

bool
FromDevice::run_task(Task *)
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
    // 9/18/06: Frederic Van Quickenborne reports (1/24/05) that ticket
    // adjustments in FromDevice+ToDevice cause odd behavior.  The ticket
    // adjustments actually don't feel necessary to me in From/ToDevice any
    // more, since FromDevice's interrupt handler will reschedule FromDevice
    // as necessary; now "ticket adjustment" is subsumed by "scheduled or not
    // scheduled".  So commenting this out.
    // adjust_tickets(npq);
    if (npq > 0)
	_task.fast_reschedule();
    return npq > 0;
}

enum { H_ACTIVE, H_DROPS, H_CALLS, H_RESET_COUNTS };

String FromDevice::read_handler(Element *e, void *thunk)
{
    FromDevice *fd = static_cast<FromDevice *>(e);
    switch (reinterpret_cast<intptr_t>(thunk)) {
      case H_ACTIVE:
	return cp_unparse_bool(fd->_dev && fd->_active);
      case H_DROPS:
	return String(fd->_drops);
      case H_CALLS: {
	  StringAccum sa;
	  sa << "calls to run_task(): " << fd->_runs << "\n"
	     << "calls to push():     " << fd->_pushes << "\n"
	     << "empty runs:          " << fd->_empty_runs << "\n"
	     << "drops:               " << fd->_drops << "\n";
	  return sa.take_string();
      }
      default:
	return String();
    }
}

int FromDevice::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    FromDevice *fd = static_cast<FromDevice *>(e);
    switch (reinterpret_cast<intptr_t>(thunk)) {
      case H_ACTIVE:
	if (!cp_bool(cp_uncomment(str), &fd->_active))
	    return errh->error("active parameter must be boolean");
	return 0;
      case H_RESET_COUNTS:
	fd->reset_counts();
	return 0;
      default:
	return 0;
    }
}

void
FromDevice::add_handlers()
{
    add_task_handlers(&_task);
    add_read_handler("active", read_handler, (void *) H_ACTIVE, Handler::CHECKBOX);
    add_read_handler("drops", read_handler, (void *) H_DROPS);
    add_read_handler("calls", read_handler, (void *) H_CALLS);
    add_write_handler("active", write_handler, (void *) H_ACTIVE);
    add_write_handler("reset_counts", write_handler, (void *) H_RESET_COUNTS, Handler::BUTTON);
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(FromDevice)
