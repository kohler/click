// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromhost.{cc,hh} -- element steals packets from kernel
 * Luigi Rizzo
 *
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
#include "fromhost.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <net/if_var.h>
#include <net/ethernet.h>

FromHost::FromHost()
    : _inq(0)
{
    MOD_INC_USE_COUNT;
    add_output();
}

FromHost::~FromHost()
{
    MOD_DEC_USE_COUNT;
}

int
FromHost::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 8;	// same as in FromDevice
    clear_need_wakeup();

    if (cp_va_parse(conf, this, errh,
                    cpString, "interface name", &_devname,
                    cpEnd) < 0 )
        return -1;
    if (find_device(false, errh) < 0)
        return -1;
    return 0;
}

int
FromHost::initialize(ErrorHandler *errh)
{
    // create queue
    int s = splimp();
    if (device()->if_poll_xmit != NULL) {
	splx(s);
	click_chatter("FromHost: %s%d already in use",
		device()->if_name, device()->if_unit);
	return -1;
    }
    (FromHost *)(device()->if_poll_xmit) = this;
    _inq = (struct ifqueue *)
            malloc(sizeof (struct ifqueue), M_DEVBUF, M_NOWAIT|M_ZERO);
    assert(_inq);
    _inq->ifq_maxlen = QSIZE;
    clear_need_wakeup();
    ScheduleInfo::initialize_task(this, &_task, true, errh);
    splx(s);
    return 0;
}

void
FromHost::cleanup(CleanupStage)
{
    if (!_inq)
	return;
    
    // Flush the receive queue.
    int s = splimp();
    struct ifqueue *q = _inq ;
    _inq = NULL;
    clear_need_wakeup();
    device()->if_poll_xmit = NULL;
    splx(s);

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

bool
FromHost::run_task()
{
    int npq = 0;
    // click_chatter("FromHost::run_task().");
    while (npq < _burst) {
	struct mbuf *m = 0;
        int s = splimp();
        IF_DEQUEUE(_inq, m);
        if (m == NULL) {
            set_need_wakeup();
            splx(s);
	    return npq > 0;
	}
	splx(s);
    
        // Got an mbuf, including the MAC header. Make it a real Packet.
        Packet *p = Packet::make(m);
        output(0).push(p);
        npq++;
    }
#if CLICK_DEVICE_ADJUST_TICKETS 
    adjust_tickets(npq);
#endif
    _task.fast_reschedule();
    return true;
}

ELEMENT_REQUIRES(AnyDevice bsdmodule)
EXPORT_ELEMENT(FromHost)
