// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdevice.{cc,hh} -- element steals packets from kernel devices using
 * register_net_in
 * Robert Morris
 * Eddie Kohler: AnyDevice, other changes
 * Benjie Chen: scheduling, internal queue
 * Nickolai Zeldovich: BSD
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
static int from_device_count;

#ifdef HAVE_CLICK_BSD_KERNEL
/*
 * Attach ourselves to the current device's packet-receive hook.
 */
static int
register_rx(struct ifnet *d, int qSize)
{
    assert(d);
    int s = splimp();
    d->click_divert++;
    d->click_intrq.ifq_maxlen = qSize;
    d->click_intrq.ifq_drops = 0;
    splx(s);
    registered_readers++;
}

/*
 * Detach from device's packet-receive hook.
 */
static int
unregister_rx(struct ifnet *d)
{
    assert(d);
    int s = splimp();
    d->click_divert--;

    /*
     * Flush the receive queue.
     */
    if (d->click_divert <= 0)
	for (int i = 0; i <= d->click_intrq.ifq_maxlen; i++) {
	    struct mbuf *m;
	    IF_DEQUEUE(&d->click_intrq, m);
	    if (!m) break;
	    m_freem(m);
	}

    splx(s);
    registered_readers--;
}
#endif

static void
fromdev_static_initialize()
{
    if (++from_device_count == 1) {
	from_device_map.initialize();
    }
}

static void
fromdev_static_cleanup()
{
    if (--from_device_count <= 0) {
#ifdef HAVE_CLICK_BSD_KERNEL
	if (registered_readers)
	    printf("Warning: registered reader count mismatch!\n");
#endif
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
		    cpBool, "enter promiscuous mode?", &_promisc,
		    cpUnsigned, "burst size", &_burst,
		    cpKeywords,
		    "PROMISC", cpBool, "enter promiscuous mode?", &_promisc,
		    "PROMISCUOUS", cpBool, "enter promiscuous mode?", &_promisc,
		    "BURST", cpUnsigned, "burst size", &_burst,
		    "ALLOW_NONEXISTENT", cpBool, "allow nonexistent interface?", &allow_nonexistent,
		    cpEnd) < 0)
	return -1;

    if (find_device(allow_nonexistent, errh) < 0)
	return -1;
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
		    return errh->error("duplicate FromDevice for `%s'",
				       _devname.cc());
	    }
	}

    from_device_map.insert(this);
    if (_promisc && _dev)
	ifpromisc(_dev, 1);
    
#ifdef HAVE_CLICK_BSD_KERNEL
    register_rx(_dev, QSIZE);
#else
    errh->warning("can't get packets: not compiled for a Click kernel");
#endif

    ScheduleInfo::initialize_task(this, &_task, _dev != 0, errh);
#ifdef HAVE_STRIDE_SCHED
    // start out with default number of tickets, inflate up to max
    _max_tickets = _task.tickets();
    _task.set_tickets(Task::DEFAULT_TICKETS);
#endif

#if CLICK_DEVICE_STATS
    // Initialize performance stats
    _time_read = 0;
    _time_push = 0;
    _perfcnt1_read = 0;
    _perfcnt2_read = 0;
    _perfcnt1_push = 0;
    _perfcnt2_push = 0;
#endif
    _npackets = 0;

    _capacity = QSIZE;
    return 0;
}

void
FromDevice::uninitialize()
{
#ifdef HAVE_CLICK_BSD_KERNEL
    unregister_rx(_dev);
#endif
    
    _task.unschedule();
    
    from_device_map.remove(this);
    if (_promisc && _dev)
	ifpromisc(_dev, 0);
}

void
FromDevice::take_state(Element *e, ErrorHandler *errh)
{
  FromDevice *fd = (FromDevice *)e->cast("FromDevice");
  if (!fd) return;
}

void
FromDevice::run_scheduled()
{
    int npq = 0;
    while (npq < _burst) {
#if CLICK_DEVICE_STATS
	unsigned low0, low1;
	unsigned long long time_now;
#endif

	/*
	 * Try to dequeue a packet from the interrupt input queue.
	 */
	SET_STATS(low0, low1, time_now);

	struct mbuf *m;
	int s = splimp();
	IF_DEQUEUE(&_dev->click_intrq, m);
	splx(s);
	if (NULL == m) break;

	/*
	 * Got a packet -- retrieve the MAC header and make a real Packet.
	 */
	int push_len = 14;

	m->m_data -= push_len;
	m->m_len += push_len;
	m->m_pkthdr.len += push_len;

	Packet *p = Packet::make(m);
	GET_STATS_RESET(low0, low1, time_now,
			_perfcnt1_read, _perfcnt2_read, _time_read);
	output(0).push(p);
	GET_STATS_RESET(low0, low1, time_now,
			_perfcnt1_push, _perfcnt2_push, _time_push);
	npq++;
	_npackets++;
    }
#if CLICK_DEVICE_ADJUST_TICKETS
    adjust_tickets(npq);
#endif
    _task.fast_reschedule();
}

int
FromDevice::get_inq_drops()
{
    return _dev->click_intrq.ifq_drops;
}

static String
FromDevice_read_stats(Element *f, void *)
{
    FromDevice *fd = (FromDevice *)f;
    return
	String(fd->_npackets) + " packets received\n" +
	String(fd->get_inq_drops()) + " input queue drops\n" +
#if CLICK_DEVICE_STATS
	String(fd->_time_read) + " cycles read\n" +
	String(fd->_time_push) + " cycles push\n" +
	String(fd->_perfcnt1_read) + " perfcnt1 read\n" +
	String(fd->_perfcnt2_read) + " perfcnt2 read\n" +
	String(fd->_perfcnt1_push) + " perfcnt1 push\n" +
	String(fd->_perfcnt2_push) + " perfcnt2 push\n" +
#endif
	String();
}

void
FromDevice::add_handlers()
{
    add_read_handler("stats", FromDevice_read_stats, 0);
    add_task_handlers(&_task);
}

ELEMENT_REQUIRES(AnyDevice Storage bsdmodule)
EXPORT_ELEMENT(FromDevice)
