// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdevice.{cc,hh} -- element steals packets from kernel devices using
 * register_net_in
 * Robert Morris
 * Eddie Kohler: AnyDevice, other changes
 * Benjie Chen: scheduling, internal queue
 * Nickolai Zeldovich, Luigi Rizzo: BSD
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
#include "fromhost.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

static AnyDeviceMap from_device_map;
static int registered_readers;
static int from_device_count;

#include <net/if_var.h>
#include <net/ethernet.h>

/*
 * Process incoming packets using the ng_ether_input_p hook.
 * The if_poll_intren (normally unused) field from the struct ifnet is
 * used as a pointer to the fromdevice data structure. Call it "xp".
 *
 * If xp == NULL, no FromDevice element is registered on this
 * interface, so return to pass the packet back to FreeBSD.
 * Otherwise, xp points to the element, which in turn contains
 * a queue. Append the packet there, clear *mp to grab the pkt from FreeBSD.
 * call wakeup() to poentially wake up the element.
 *
 * Special case: m->m_pkthdr.rcvif == NULL means the packet is coming
 * from click and directed to this interface on the host.
 * We are already running at splimp() so we need no further protection.
 */
extern "C"
void
click_ether_input(struct ifnet *ifp, struct mbuf **mp, struct ether_header *eh)
{
    if (ifp->if_poll_intren == NULL)	// not for click.
	return ;
    struct mbuf *m = *mp;
    if (m->m_pkthdr.rcvif == NULL) {	// Special case: from click to FreeBSD
	m->m_pkthdr.rcvif = ifp;	// Reset rcvif to correct value, and
	return;				// let FreeBSD continue processing.
    }

    *mp = NULL;		// tell ether_input no further processing needed.

    FromDevice *me = (FromDevice *)(ifp->if_poll_intren);

    // put the ethernet header back into the mbuf.
    M_PREPEND(m, sizeof(*eh), M_WAIT);
    bcopy(eh, mtod(m, struct ether_header *), sizeof(*eh));

    if (IF_QFULL(me->_inq)) {
	IF_DROP(me->_inq);
	m_freem(m);
    } else
	IF_ENQUEUE(me->_inq, m);
    me->wakeup();
}

/*
 * Process outgoing packets using the ng_ether_output_p hook.
 * If if_poll_xmit == NULL, no FromHost element is registered on this
 * interface, so return 0 to pass the packet back to FreeBSD.
 * Otherwise, if_poll_xmit points to the element, which in turn contains
 * a queue. Append the packet there, clear *mp to grab the pkt from FreeBSD,
 * and possibly wakeup the element.
 *
 * We _need_ splimp()/splx() to avoid races.
 */
extern "C"
int
click_ether_output(struct ifnet *ifp, struct mbuf **mp)
{
    int s = splimp();
    if (ifp->if_poll_xmit == NULL) { // not for click...
	splx(s);
	return 0;
    }
    struct mbuf *m = *mp;
    *mp = NULL; // tell ether_output no further processing needed

    FromHost *me = (FromHost *)(ifp->if_poll_xmit);
    if (IF_QFULL(me->_inq)) {
        IF_DROP(me->_inq);
        m_freem(m);
    } else
        IF_ENQUEUE(me->_inq, m);
    me->wakeup();
    splx(s);
    return 0;
}

static void
fromdev_static_initialize()
{
    if (++from_device_count == 1)
	from_device_map.initialize();
}

static void
fromdev_static_cleanup()
{
    if (--from_device_count <= 0) {
	if (registered_readers)
	    printf("Warning: registered reader count mismatch!\n");
    }
}

FromDevice::FromDevice()
{
    // no MOD_INC_USE_COUNT; rely on AnyDevice
    _readers = 0; // noone registered so far
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
FromDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _promisc = false;
    _inq = NULL;
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
    // check for duplicates
    if (ifindex() >= 0)
	for (int fi = 0; fi < router()->nelements(); fi++) {
	    Element *e = router()->element(fi);
	    if (e == this)
		continue;
	    if (FromDevice *fd = (FromDevice *)(e->cast("FromDevice"))) {
		if (fd->ifindex() == ifindex())
		    return errh->error("duplicate FromDevice for `%s'",
				       _devname.cc());
	    }
	}

    from_device_map.insert(this);
    if (_promisc && device())
	ifpromisc(device(), 1);
    
    assert(device());
    int s = splimp();
    if (_inq == NULL) {
	if (_readers != 0)
	    printf("Warning, _readers mismatch (%d should be 0)\n",
		   _readers);
	_inq = (struct ifqueue *)
	    malloc(sizeof (struct ifqueue), M_DEVBUF, M_NOWAIT|M_ZERO);
	assert(inq);
	_inq->ifq_maxlen = QSIZE;
	(FromDevice *)(device()->if_poll_intren) = this;
    } else {
	if (_readers == 0)
	    printf("Warning, _readers mismatch (should not be 0)\n");
    }
    _readers++;
    registered_readers++;
    splx(s);

    ScheduleInfo::initialize_task(this, &_task, device() != 0, errh);
#ifdef HAVE_STRIDE_SCHED
    // start out with default number of tickets, inflate up to max
    set_max_tickets( _task.tickets() );
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
FromDevice::cleanup(CleanupStage)
{
    if (!device())
	return;

    struct ifqueue *q = NULL;
    int s = splimp();
    registered_readers--;
    _readers--;
    if (_readers == 0) {	// flush queue
	q = _inq ;
	_inq = NULL ;
	device()->if_poll_intren = NULL ;
    }
    splx(s);
    if (q) {		// we do not mutex for this.
	int i, max = q->ifq_maxlen ;
	for (i = 0; i < max; i++) {
	    struct mbuf *m;
	    IF_DEQUEUE(q, m);
	    if (!m)
		break;
	    m_freem(m);
	}
	free(q, M_DEVBUF);
    }
    
    from_device_map.remove(this);
    if (_promisc && device())
	ifpromisc(device(), 0);
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
    // click_chatter("FromDevice::run_scheduled().");
    while (npq < _burst) {
	struct mbuf *m = 0;

	// Try to dequeue a packet from the interrupt input queue.
	int s = splimp();
	IF_DEQUEUE(_inq, m);
	if (m == NULL) {
	    set_need_wakeup();
	    splx(s);
	    adjust_tickets(npq);
	    return;
	}
	splx(s);

	// Got a packet, which includes the MAC header. Make it a real Packet.

	Packet *p = Packet::make(m);
	output(0).push(p);
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
    return (_inq ? _inq->ifq_drops : 0);
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

ELEMENT_REQUIRES(AnyDevice bsdmodule)
EXPORT_ELEMENT(FromDevice)
