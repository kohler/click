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
 * Copyright (c) 2007-2011 Regents of the University of California
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
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/straccum.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
# include <linux/rtnetlink.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

static AnyDeviceMap from_device_map;
static int registered_readers;
#if HAVE_CLICK_KERNEL
static struct notifier_block packet_notifier;
#endif
#if CLICK_FROMDEVICE_USE_BRIDGE
static struct notifier_block device_notifier_early;
#endif
static struct notifier_block device_notifier;

#if CLICK_FROMDEVICE_USE_BRIDGE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/if_bridge.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif


extern "C" {
#if HAVE_CLICK_KERNEL
static int packet_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
#elif CLICK_FROMDEVICE_USE_BRIDGE
static struct sk_buff *click_br_handle_frame_hook(struct net_bridge_port *p, struct sk_buff *skb);
static struct sk_buff *(*real_br_handle_frame_hook)(struct net_bridge_port *p, struct sk_buff *skb);
static int device_notifier_hook_early(struct notifier_block *nb, unsigned long val, void *v);
#endif
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}

void
FromDevice::static_initialize()
{
    from_device_map.initialize();
#if HAVE_CLICK_KERNEL
    packet_notifier.notifier_call = packet_notifier_hook;
    packet_notifier.priority = 1;
    packet_notifier.next = 0;
#endif
#if CLICK_FROMDEVICE_USE_BRIDGE
    device_notifier_early.notifier_call = device_notifier_hook_early;
    device_notifier_early.priority = INT_MAX;
    device_notifier_early.next = 0;
    register_netdevice_notifier(&device_notifier_early);
#endif
    device_notifier.notifier_call = device_notifier_hook;
    device_notifier.priority = INT_MIN;
    device_notifier.next = 0;
    register_netdevice_notifier(&device_notifier);
}

void
FromDevice::static_cleanup()
{
#if HAVE_CLICK_KERNEL
    if (registered_readers)
	unregister_net_in(&packet_notifier);
#elif CLICK_FROMDEVICE_USE_BRIDGE
    if (br_handle_frame_hook == click_br_handle_frame_hook)
	br_handle_frame_hook = real_br_handle_frame_hook;
    unregister_netdevice_notifier(&device_notifier_early);
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
    String alignment;
    if (AnyDevice::configure_keywords(conf, errh, true) < 0
	|| (Args(conf, this, errh)
	    .read_mp("DEVNAME", _devname)
	    .read_p("BURST", _burst)
	    .read("ACTIVE", _active)
	    .read("ALIGNMENT", AnyArg(), alignment)
	    .complete() < 0))
	return -1;

    // make queue look full so packets sent to us are ignored
    _head = _tail = _capacity = 0;

    net_device *dev = lookup_device(errh);
    set_device(dev, &from_device_map, anydev_from_device);
    return errh->nerrors() ? -1 : 0;
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
	    return errh->error("device %<%s%> has duplicate reader", _devname.c_str());
	used = this;
    }

    if (registered_readers == 0) {
#if HAVE_CLICK_KERNEL
	packet_notifier.next = 0;
	register_net_in(&packet_notifier);
#elif CLICK_FROMDEVICE_USE_BRIDGE
	real_br_handle_frame_hook = br_handle_frame_hook;
	br_handle_frame_hook = click_br_handle_frame_hook;
#elif HAVE_LINUX_NETDEV_RX_HANDLER_REGISTER
	/* OK */
#else
	errh->warning("can't get packets: not compiled for a Click kernel");
#endif
    }
    ++registered_readers;

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
	--registered_readers;
	if (registered_readers == 0) {
#if HAVE_CLICK_KERNEL
	    unregister_net_in(&packet_notifier);
#elif CLICK_FROMDEVICE_USE_BRIDGE
	    br_handle_frame_hook = real_br_handle_frame_hook;
#endif
	}
    }

    clear_device(&from_device_map, anydev_from_device);

    if (stage >= CLEANUP_INITIALIZED)
	for (Storage::index_type i = _head; i != _tail; i = next_i(i))
	    _queue[i]->kill();
    _head = _tail = 0;
}

void
FromDevice::take_state(Element *e, ErrorHandler *errh)
{
    if (FromDevice *fd = (FromDevice *)e->cast("FromDevice")) {
	SpinlockIRQ::flags_t flags;
	local_irq_save(flags);

	Storage::index_type fd_i = fd->_head;
	while (fd_i != fd->_tail) {
	    Storage::index_type next = next_i(_tail);
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
	_highwater_length = size();

	local_irq_restore(flags);
    }
}

/*
 * Called by Linux net_bh[2.2]/net_rx_action[2.4] with each packet.
 */
extern "C" {

#if HAVE_CLICK_KERNEL
static int
packet_notifier_hook(struct notifier_block *nb, unsigned long backlog_len, void *v)
{
    struct sk_buff *skb = (struct sk_buff *)v;
    int stolen = 0;
    FromDevice *fd = 0;
    unsigned long lock_flags;
    from_device_map.lock(false, lock_flags);
    while (stolen == 0 && (fd = (FromDevice *)from_device_map.lookup(skb->dev, fd)))
	stolen = fd->got_skb(skb);
    from_device_map.unlock(false, lock_flags);
    return (stolen ? NOTIFY_STOP_MASK : 0);
}

#elif CLICK_FROMDEVICE_USE_BRIDGE
static struct sk_buff *
click_br_handle_frame_hook(struct net_bridge_port *p, struct sk_buff *skb)
{
# if CLICK_DEVICE_UNRECEIVABLE_SK_BUFF
    if (__get_cpu_var(click_device_unreceivable_sk_buff) == skb)
	// This packet is being passed to Linux by ToHost.
	return skb;
# endif

    int stolen = 0;
    FromDevice *fd = 0;
    unsigned long lock_flags;

    from_device_map.lock(false, lock_flags);
    while (stolen == 0 && (fd = (FromDevice *)from_device_map.lookup(skb->dev, fd)))
	stolen = fd->got_skb(skb);
    from_device_map.unlock(false, lock_flags);
    if (stolen)
	return 0;
    else if (real_br_handle_frame_hook)
	return real_br_handle_frame_hook(p, skb);
    else
	return skb;
}

#elif HAVE_LINUX_NETDEV_RX_HANDLER_REGISTER
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
rx_handler_result_t
click_fromdevice_rx_handler(struct sk_buff **pskb)
# else
struct sk_buff *
click_fromdevice_rx_handler(struct sk_buff *skb)
#define RX_HANDLER_PASS skb
#define RX_HANDLER_CONSUMED 0
# endif
{
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
    struct sk_buff *skb = *pskb;
# endif
# if CLICK_DEVICE_UNRECEIVABLE_SK_BUFF
    if (__get_cpu_var(click_device_unreceivable_sk_buff) == skb)
	// This packet is being passed to Linux by ToHost.
	return RX_HANDLER_PASS;
# endif

    int stolen = 0;
    FromDevice *fd = 0;
    unsigned long lock_flags;

    from_device_map.lock(false, lock_flags);
    while (stolen == 0 && (fd = (FromDevice *)from_device_map.lookup(skb->dev, fd)))
	stolen = fd->got_skb(skb);
    from_device_map.unlock(false, lock_flags);
    if (stolen)
	return RX_HANDLER_CONSUMED;
    else
	return RX_HANDLER_PASS;
}
#endif

#if CLICK_FROMDEVICE_USE_BRIDGE
static int
device_notifier_hook_early(struct notifier_block *nb, unsigned long flags, void *v)
{
    unsigned long lock_flags;
    net_device* dev = (net_device*)v;
    AnyDevice *es[8];
    int i, nes;

#ifdef NETDEV_GOING_DOWN
    if (flags == NETDEV_GOING_DOWN)
	flags = NETDEV_DOWN;
#endif
    if (flags == NETDEV_DOWN || flags == NETDEV_UP || flags == NETDEV_CHANGE) {
	bool exists = (flags != NETDEV_UP);
	from_device_map.lock(true, lock_flags);
	nes = from_device_map.lookup_all(dev, exists, es, 8);
	for (i = 0; i < nes; i++)
	    ((FromDevice*)(es[i]))->alter_from_device(-1);
	from_device_map.unlock(true, lock_flags);
    }

    return 0;
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
	bool exists = (flags != NETDEV_UP);
	net_device* dev = (net_device*)v;
	unsigned long lock_flags;
	from_device_map.lock(true, lock_flags);
	AnyDevice *es[8];
	int nes = from_device_map.lookup_all(dev, exists, es, 8);
	for (int i = 0; i < nes; i++)
	    ((FromDevice*)(es[i]))->set_device(flags == NETDEV_DOWN ? 0 : dev, &from_device_map, AnyDevice::anydev_change | AnyDevice::anydev_from_device);
	from_device_map.unlock(true, lock_flags);
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

    unsigned next = next_i(_tail), head = _head;

    if (next != head) { /* ours */
	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
	    return 1;
	assert(skb_shared(skb) == 0);

	/* Retrieve the MAC header. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	skb_push(skb, skb->data - skb_mac_header(skb));
#else
	skb_push(skb, skb->data - skb->mac.raw);
#endif

	Packet *p = Packet::make(skb);
	_queue[_tail] = p; /* hand it to run_task */
	packet_memory_barrier(_queue[_tail], _tail);

#if CLICK_DEBUG_SCHEDULING
	_schinfo[_tail].enq_time.assign_now();
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

	unsigned s = size(head, next);
	if (s > _highwater_length)
	    _highwater_length = s;

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
    Timestamp now = Timestamp::now();
    RouterThread *rt = _task.thread();
    StringAccum sa;
    sa << "dt " << (now - _schinfo[idx].enq_time);
    if (_schinfo[idx].enq_state != RouterThread::S_RUNNING) {
	Timestamp etime = rt->task_epoch_time(_schinfo[idx].enq_task_epoch + 1);
	if (etime)
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
	packet_memory_barrier(_queue[_head], _head);
#if CLICK_DEBUG_SCHEDULING
	emission_report(_head);
#endif
	_head = next_i(_head);
	output(0).push(p);
	npq++;
	_count++;
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

String FromDevice::read_handler(Element *e, void *thunk)
{
    FromDevice *fd = static_cast<FromDevice *>(e);
    StringAccum sa;
    switch (reinterpret_cast<intptr_t>(thunk)) {
    case h_active:
	sa << (fd->_dev && fd->_active);
	break;
    case h_length:
	sa << fd->size();
	break;
    case h_calls:
	sa << "calls to run_task(): " << fd->_runs << "\n"
	   << "calls to push():     " << fd->_count << "\n"
	   << "empty runs:          " << fd->_empty_runs << "\n"
	   << "drops:               " << fd->_drops << "\n";
	break;
    }
    return sa.take_string();
}

int FromDevice::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    FromDevice *fd = static_cast<FromDevice *>(e);
    switch (reinterpret_cast<intptr_t>(thunk)) {
    case h_active:
	if (!BoolArg().parse(str, fd->_active))
	    return errh->error("active parameter must be boolean");
	return 0;
    case h_reset_counts:
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
    add_read_handler("active", read_handler, h_active, Handler::CHECKBOX);
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_read_handler("length", read_handler, h_length);
    add_data_handlers("highwater_length", Handler::OP_READ, &_highwater_length);
    add_read_handler("calls", read_handler, h_calls);
    add_write_handler("active", write_handler, h_active);
    add_write_handler("reset_counts", write_handler, h_reset_counts, Handler::BUTTON);
}

#undef CLICK_FROMDEVICE_USE_BRIDGE
ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(FromDevice)
