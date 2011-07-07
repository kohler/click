// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdevice.{cc,hh} -- element steals packets from kernel devices
 *
 * Robert Morris
 * Eddie Kohler: AnyDevice, other changes
 * Benjie Chen: scheduling, internal queue
 * Nickolai Zeldovich, Luigi Rizzo, Marko Zec: BSD
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2004 International Computer Science Institute
 * Copyright (c) 2004 University of Zagreb
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
#include <click/args.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/linker.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/netisr.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

CLICK_DECLS

#define POLL_LIST_LEN 128	// XXX should refernce a proper #include

#if 0
struct pollrec {
        poll_handler_t  *handler;
        struct ifnet    *ifp;
};

int *polling;			// polling mode
static int *poll_handlers;	// # of NICs registered for BSD kernel polling
static int *reg_frac;		// How often we have to check status registers
static struct pollrec *pr;	// BSD kernel polling handlers
#endif

static AnyDeviceMap from_device_map;
static int registered_readers;
static int from_device_count;


/*
 * Process incoming packets using the ng_ether_input_p hook.
 * The if_spare2 (normally unused) field from the struct ifnet is
 * used as a pointer to the fromdevice data structure. Call it "xp".
 *
 * If xp == NULL, no FromDevice element is registered on this
 * interface, so return to pass the packet back to FreeBSD.
 * Otherwise, xp points to the element, which in turn contains
 * a queue. Append the packet there, clear *mp to grab the pkt from FreeBSD.
 * call wakeup() to poentially wake up the element.
 *
 * Special case: m->m_pkthdr.rcvif == &_dev_click means the packet is coming
 * from click and directed to this interface on the host.
 * We are already running at splimp() so we need no further protection.
 */
extern "C"
void
click_ether_input(struct ifnet *ifp, struct mbuf **mp)
{
    // FIXME: better detection method...
    if (CLICK_IFP2FD(ifp) == NULL)	// not for click.
    	return;

    struct mbuf *m = *mp;
    if (m->m_pkthdr.rcvif == &_dev_click) {
	// Special case: from click to FreeBSD
	m->m_pkthdr.rcvif = ifp;	// Reset rcvif to correct value, and
	return;				// let FreeBSD continue processing.
    }

    *mp = NULL;		// tell ether_input no further processing needed.

    FromDevice *me = (FromDevice *)(CLICK_IFP2FD(ifp));

#if 0
     /* XXX: needs rewriting for polling(4) rewrite. -bms */
    /*
     * If sysctl kern.polling.enable == 2 we should take care of polling
     * this NIC from inside a Click thread, so steal the handler from BSD.
     */
    if (!me->_polling && (ifp->if_ipending & IFF_POLLING) &&
	polling && *polling == 2) {
	struct pollrec *prp = pr;
	int i;

	for (i = 0; i < *poll_handlers; i++, prp++)
	    if (prp->ifp == ifp) {
		me->_poll_handler = prp->handler;
		me->_poll_status_tick = ticks;
		prp->handler = NULL;
		prp->ifp = NULL;
		printf("Click FromDevice(%s) taking control over NIC driver polling\n", ifp->if_xname);
		me->_polling = -1; // wakeup task thread only once more
		break;
	    }
	if (!me->_polling) {
	    printf("Strange, couldn't find polling handler for %s\n",
			ifp->if_xname);
	    me->_polling = -2; // Do not bother trying to register again
	}
    }
#endif

    if (_IF_QFULL(me->_inq)) {
	_IF_DROP(me->_inq);
	m_freem(m);
    } else
	_IF_ENQUEUE(me->_inq, m);
    /*
    if (me->_polling != 1)
    */
	me->intr_reschedule();
#if __FreeBSD_version >= 800000 && defined(BSD_NETISRSCHED)
    netisr_dispatch(NETISR_CLICK, m);
#endif

	/*
    if (me->_polling == -1)
	me->_polling = 1; // no need to wakeup task thread any more
	*/
#ifdef FROMDEVICE_TSTAMP
    me->_tstamp = rdtsc();
#endif
}

/*
 * Process outgoing packets using the ng_ether_output_p hook.
 * If if_spare3 == NULL, no FromHost element is registered on this
 * interface, so return 0 to pass the packet back to FreeBSD.
 * Otherwise, if_spare3 points to the element, which in turn contains
 * a queue. Append the packet there, clear *mp to grab the pkt from FreeBSD,
 * and possibly wakeup the element.
 *
 * We _need_ splimp()/splx() to avoid races.
 */
extern "C"
int
click_ether_output(struct ifnet *ifp, struct mbuf **mp)
{
    return 0;
#if 0
    int s = splimp();
/*
    if (ifp->if_spare3 == NULL) { // not for click...
	splx(s);
	return 0;
    }
*/
    struct mbuf *m = *mp;
    *mp = NULL; // tell ether_output no further processing needed

    FromHost *me = (FromHost *)(ifp->if_spare3);
    if (_IF_QFULL(me->_inq)) {
        _IF_DROP(me->_inq);
        m_freem(m);
    } else
        _IF_ENQUEUE(me->_inq, m);
    me->intr_reschedule();
    splx(s);
    return 0;
#endif
}

extern "C"
void
click_ether_input_orphan(struct ifnet *ifp, struct mbuf **mp)
{
}

/*
 * This dummy function is meant to be assigned to ng_ether_link_state_p
 * which is NULL by default but is called when the state of a network
 * interface changes and the ac_netgraph hook is not NULL (which is the
 * case for us).
 */
extern "C"
void
click_ether_link_state(struct ifnet *ifp, int state)
{
}

static void
fromdev_static_initialize()
{
    linker_file_t kernel_lf = linker_kernel_file; /* kernel file */

#if 0
    pr = (struct pollrec *) linker_file_lookup_symbol(kernel_lf, "pr", 0);
    reg_frac = (int *) linker_file_lookup_symbol(kernel_lf, "reg_frac", 0);
    poll_handlers = (int *)
		    linker_file_lookup_symbol(kernel_lf, "poll_handlers", 0);
    if (pr && poll_handlers && reg_frac)
	polling = (int *) linker_file_lookup_symbol(kernel_lf, "polling", 0);
    else
	polling = NULL;

    if (polling)
	printf("Cool, we are running Click on a polling capable kernel!\n");
#endif

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
    _readers = 0; // noone registered so far
    _polling = 0; // we do not poll until NIC driver registers itself
    fromdev_static_initialize();
}

FromDevice::~FromDevice()
{
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
    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _devname)
	.read_p("PROMISC", _promisc)
	.read_p("BURST", _burst)
	.read("ALLOW_NONEXISTENT", allow_nonexistent)
	.complete() < 0)
	return -1;

    if (find_device(allow_nonexistent, &from_device_map, errh) < 0)
	return -1;
    return 0;
}

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
				       _devname.c_str());
	    }
	}

    if (device() == NULL)
        return errh->error("FromDevice for `%s' cannot be initialized",
                           _devname.c_str());

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
	assert(_inq);
	_inq->ifq_maxlen = QSIZE;
	mtx_init(&_inq->ifq_mtx, "fromdevice", NULL, MTX_DEF);
	CLICK_IFP2FD(device()) = (void *)this;
    } else {
	if (_readers == 0)
	    printf("Warning, _readers mismatch (should not be 0)\n");
    }
    _readers++;
    registered_readers++;
    splx(s);

    ScheduleInfo::initialize_task(this, &_task, device() != 0, errh);
#if HAVE_STRIDE_SCHED
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
	CLICK_IFP2FD(device()) = NULL ;
    }

#if 0
    if (_polling == 1) {	// return polling handler to the kernel
	struct pollrec *prp = pr;
	int i;
	for (i = 0; i < *poll_handlers; i++, prp++)
	    if (prp->ifp == NULL)
		break;
	prp->handler = _poll_handler;
	prp->ifp = _dev;
	if (*poll_handlers == 0 && (_dev->if_drv_flags & IFF_DRV_RUNNING))
	    *poll_handlers = 1;
    }
#endif

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

#if 0 /* XXX: method signature has disappeared. -bms */
void
FromDevice::take_state(Element *e, ErrorHandler *errh)
{
  FromDevice *fd = (FromDevice *)e->cast("FromDevice");
  if (!fd) return;
}
#endif

bool
FromDevice::run_task(Task *)
{
    int npq = 0;
    // click_chatter("FromDevice::run_task().");

#if 0 /* XXX: See polling(4). -bms */
    if (_dev && _polling == 1) {
	if (_dev->if_ipending & IFF_POLLING) {
	    enum poll_cmd cmd;

	    if ( _poll_status_tick <= ticks ) {
		_poll_status_tick = ticks + *reg_frac;
		cmd = POLL_AND_CHECK_STATUS;
	    } else
		cmd = POLL_ONLY;

	    if ( *polling != 2 ) { // Need to return the handler to the kernel
		struct pollrec *prp = pr;
		int i;
		_polling = 0;		// No more polling in Click
		for (i = 0; i < *poll_handlers; i++, prp++)
		    if (prp->ifp == NULL)
			break;
		prp->handler = _poll_handler;
		prp->ifp = _dev;
		if (*poll_handlers == 0 &&
		    (_dev->if_drv_flags & IFF_DRV_RUNNING))
		    *poll_handlers = 1;
	    } else
		_poll_handler(_dev, cmd, _burst);
	} else
	    _polling = 0;		// No more polling
    }
#endif

#ifdef FROMDEVICE_TSTAMP
    if (_tstamp) {
	uint64_t now = rdtsc();
	click_chatter("FromDevice::run_task() latency=%ld", now - _tstamp);
	_tstamp = 0;
    }
#endif

    while (npq <= _burst) {
	struct mbuf *m = 0;

	// Try to dequeue a packet from the interrupt input queue.
	IF_DEQUEUE(_inq, m);
	if (m == NULL) {
#if CLICK_DEVICE_ADJUST_TICKETS
	    adjust_tickets(npq);
#endif
	    //if (_polling)
	//	_task.fast_reschedule();
	    return npq > 0;
	}

	// Got a packet, which includes the MAC header. Make it a real Packet.

	Packet *p = Packet::make(m);
	assert(p);
        p->set_timestamp_anno(Timestamp()); /* XXX */
	output(0).push(p);
	npq++;
	_npackets++;
    }
#if CLICK_DEVICE_ADJUST_TICKETS
    adjust_tickets(npq);
#endif
//printf("fromdevice couldn't handle all packets\n");
    _task.fast_reschedule();
    return true;
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

CLICK_ENDDECLS
ELEMENT_REQUIRES(AnyDevice bsdmodule)
EXPORT_ELEMENT(FromDevice)
