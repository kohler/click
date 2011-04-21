/*
 * todevice.{cc,hh} -- element sends packets to BSD devices.
 * Robert Morris
 * Eddie Kohler: register once per configuration
 * Nickolai Zeldovich, Luigi Rizzo: BSD
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <net/if_var.h>
#include <net/ethernet.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
CLICK_DECLS

/* for watching when devices go offline */
static AnyDeviceMap to_device_map;
static int to_device_count;
#if 0 /* Not yet in BSD XXX */
static struct notifier_block device_notifier;
extern "C" {
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}
#endif

static void
todev_static_initialize()
{
    if (++to_device_count == 1) {
	to_device_map.initialize();
#if 0 /* XXX not yet in BSD XXX */
	device_notifier.notifier_call = device_notifier_hook;
	device_notifier.priority = 1;
	device_notifier.next = 0;
	register_netdevice_notifier(&device_notifier);
#endif
    }
}

static void
todev_static_cleanup()
{
    if (--to_device_count <= 0) {
#if 0 /* XXX not yet in BSD */
	unregister_netdevice_notifier(&device_notifier);
#endif
    }
}

ToDevice::ToDevice()
{
    todev_static_initialize();
}

ToDevice::~ToDevice()
{
    todev_static_cleanup();
}


int
ToDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 16;
    bool allow_nonexistent = false;
    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _devname)
	.read_p("BURST", _burst)
	.read("ALLOW_NONEXISTENT", allow_nonexistent)
	.complete() < 0)
	return -1;

    if (find_device(allow_nonexistent, &to_device_map, errh) < 0)
	return -1;
    return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
    to_device_map.insert(this);

    ScheduleInfo::initialize_task(this, &_task, device() != 0, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);

#if HAVE_STRIDE_SCHED
    // start out with max number of tickets
    set_max_tickets( _task.tickets() );
    _task.set_tickets(Task::DEFAULT_TICKETS);
#endif

  reset_counts();
  return 0;
}

void
ToDevice::reset_counts()
{
    _npackets = 0;
    _busy_returns = 0;
}

void
ToDevice::cleanup(CleanupStage)
{
    to_device_map.remove(this);
}

bool
ToDevice::run_task(Task *)
{
    int busy;
    int sent = 0;
    // click_chatter("ToDevice::run_task().");

    while (sent < _burst && (busy = _IF_QFULL(&device()->if_snd)) == 0) {

	Packet *p = input(0).pull();
	if (!p)
	    break;
	_npackets++;
	struct mbuf *m = p->steal_m();
	if (ether_output_frame(device(), m) != 0)
	    break;
	sent++;
    }

    if (busy)
	_busy_returns++;

#if 0
    adjust_tickets(sent);
#endif
    if (sent > 0 || busy || _signal)
	_task.fast_reschedule();
    return sent > 0;
}

static String
ToDevice_read_calls(Element *f, void *)
{
  ToDevice *td = (ToDevice *)f;
  return
    String(td->_busy_returns) + " device busy returns\n" +
    String(td->_npackets) + " packets sent\n" +
    String();
}

static String
ToDevice_read_stats(Element *e, void *thunk)
{
  ToDevice *td = (ToDevice *)e;
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(td->_npackets);
   default:
    return String();
  }
}

static int
ToDevice_write_stats(const String &, Element *e, void *, ErrorHandler *)
{
  ToDevice *td = (ToDevice *)e;
  td->reset_counts();
  return 0;
}

void
ToDevice::add_handlers()
{
  add_read_handler("calls", ToDevice_read_calls, 0);
  add_read_handler("packets", ToDevice_read_stats, 0);
  add_write_handler("reset_counts", ToDevice_write_stats, 0, Handler::BUTTON);
  add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(AnyDevice bsdmodule)
EXPORT_ELEMENT(ToDevice)
