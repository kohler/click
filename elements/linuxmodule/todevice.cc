/*
 * todevice.{cc,hh} -- element sends packets to Linux devices.
 * Robert Morris
 * Eddie Kohler: register once per configuration
 * Benjie Chen: polling
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "polldevice.hh"
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include "elements/standard/scheduleinfo.hh"

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <net/pkt_sched.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include <asm/msr.h>

ToDevice::ToDevice()
  : _polling(0), _registered(0),
    _dev_idle(0), _rejected(0), _hard_start(0)
{
  // no MOD_INC_USE_COUNT; rely on AnyDevice
  add_input();
}

ToDevice::~ToDevice()
{
  // no MOD_DEC_USE_COUNT; rely on AnyDevice
  if (_registered) uninitialize();
}


int
ToDevice::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _burst = 16;
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_devname,
		  cpOptional,
		  cpUnsigned, "burst", &_burst,
		  cpEnd) < 0)
    return -1;
  _dev = dev_get_by_name(_devname.cc());
  if (!_dev)
    _dev = find_device_by_ether_address(_devname, this);
  if (!_dev)
    return errh->error("unknown device `%s'", _devname.cc());
  return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
#ifndef HAVE_CLICK_KERNEL
  errh->warning("not compiled for a Click kernel");
#endif

  // see if a PollDevice with the same device exists: if so, use polling
  // extensions. Also look for duplicate ToDevices; but beware: ToDevice may
  // not have been initialized
  for (int ei = 0; ei < router()->nelements(); ei++) {
    Element *e = router()->element(ei);
    if (e == this) continue;
    if (ToDevice *td = (ToDevice *)(e->cast("ToDevice"))) {
      if (td->ifindex() == ifindex())
	return errh->error("duplicate ToDevice for `%s'", _devname.cc());
    } else if (PollDevice *pd = (PollDevice *)(e->cast("PollDevice"))) {
      if (pd->ifindex() == ifindex())
	_polling = 1;
    }
  }

  _registered = 1;

#ifdef HAVE_STRIDE_SCHED
  /* start out with max number of tickets */
  _max_tickets = ScheduleInfo::query(this, errh);
#endif
  _task.initialize(this, true);

  reset_counts();

  return 0;
}

void
ToDevice::reset_counts()
{
  _npackets = 0;
  
  _busy_returns = 0; 
#if CLICK_DEVICE_STATS
  _activations = 0;
  _time_clean = 0;
  _time_freeskb = 0;
  _time_queue = 0;
  _perfcnt1_pull = 0;
  _perfcnt1_clean = 0;
  _perfcnt1_freeskb = 0;
  _perfcnt1_queue = 0;
  _perfcnt2_pull = 0;
  _perfcnt2_clean = 0;
  _perfcnt2_freeskb = 0;
  _perfcnt2_queue = 0;
#endif
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  _pull_cycles = 0;
#endif
}


void
ToDevice::uninitialize()
{
  _registered = 0;
  _task.unschedule();
}

/*
 * Problem: Linux drivers aren't required to
 * accept a packet even if they've marked themselves
 * as idle. What do we do with a rejected packet?
 */

void
ToDevice::run_scheduled()
{
  int busy;
  int sent = 0;
  
#if CLICK_DEVICE_STATS
  unsigned low00, low10;
  unsigned long long time_now;
#endif

  SET_STATS(low00, low10, time_now);
 
#if HAVE_POLLING
  if (_polling) {
    struct sk_buff *skbs = _dev->tx_clean(_dev);

#if CLICK_DEVICE_STATS
    if (_activations > 0 && skbs) {
      GET_STATS_RESET(low00, low10, time_now, 
		      _perfcnt1_clean, _perfcnt2_clean, _time_clean);
    }
#endif

    if (skbs)
      skbmgr_recycle_skbs(skbs, 1);
    
#if CLICK_DEVICE_STATS
    if (_activations > 0 && skbs) {
      GET_STATS_RESET(low00, low10, time_now, 
		      _perfcnt1_freeskb, _perfcnt2_freeskb, _time_freeskb);
    }
#endif
  }
#endif
  
  SET_STATS(low00, low10, time_now);

  /* try to send from click */
  while (sent < _burst && (busy = _dev->tbusy) == 0) {

#if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
    unsigned long long before_pull_cycles = click_get_cycles();
#endif
  
    Packet *p = input(0).pull();
    if (!p)
      break;
    
    _npackets++;
#if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
    _pull_cycles += click_get_cycles() - before_pull_cycles - CLICK_CYCLE_COMPENSATION;
#endif
    
    GET_STATS_RESET(low00, low10, time_now, 
		    _perfcnt1_pull, _perfcnt2_pull, _pull_cycles);
    
    int r = queue_packet(p);
    
    GET_STATS_RESET(low00, low10, time_now, 
		    _perfcnt1_queue, _perfcnt2_queue, _time_queue);

    if (r < 0) break;
    sent++;
  }

#if HAVE_POLLING
  if (_polling && sent > 0)
    _dev->tx_eob(_dev);

  // If Linux tried to send a packet, but saw tbusy, it will
  // have left it on the queue. It'll just sit there forever
  // (or until Linux sends another packet) unless we poke
  // net_bh(), which calls qdisc_restart(). We are not allowed
  // to call qdisc_restart() ourselves, outside of net_bh().
  if (_polling && !busy && _dev->qdisc->q.qlen) {
    _dev->tx_eob(_dev);
    mark_bh(NET_BH);
  }
#endif

#if CLICK_DEVICE_STATS
  if (sent > 0) _activations++;
#endif

  if (busy) _busy_returns++;

#if HAVE_POLLING
  if (_polling) {
    if (busy && sent == 0) {
      _dev_idle++;
      if (_dev_idle==1024) {
        /* device didn't send anything, ping it */
        _dev->tx_start(_dev);
        _dev_idle=0;
        _hard_start++;
      }
    } else
      _dev_idle = 0;
  }
#endif

  adjust_tickets(sent);
  _task.fast_reschedule();
}

int
ToDevice::queue_packet(Packet *p)
{
  struct sk_buff *skb1 = p->steal_skb();
  
  /*
   * Ensure minimum ethernet packet size (14 hdr + 46 data).
   * I can't figure out where Linux does this, so I don't
   * know the correct procedure.
   */

  if(skb1->len < 60){
    if(skb_tailroom(skb1) < 60 - skb1->len){
      printk("ToDevice: too small: len %d tailroom %d\n",
             skb1->len, skb_tailroom(skb1));
      kfree_skb(skb1);
      return -1;
    }
    // printk("padding %d:%d:%d\n", skb1->truesize, skb1->len, 60-skb1->len);
    skb_put(skb1, 60 - skb1->len);
  }

  int ret;
#if HAVE_POLLING
  if (_polling)
    ret = _dev->tx_queue(_dev, skb1);
  else 
#endif
  {
    ret = _dev->hard_start_xmit(skb1, _dev);
    _hard_start++;
  }
  if(ret != 0){
    if(_rejected == 0)
      printk("<1>ToDevice %s rejected a packet!\n", _dev->name);
    kfree_skb(skb1);
    _rejected++;
  }
  return ret;
}

static String
ToDevice_read_calls(Element *f, void *)
{
  ToDevice *td = (ToDevice *)f;
  return
    String(td->_rejected) + " packets rejected\n" +
    String(td->_hard_start) + " hard start xmit\n" +
    String(td->_busy_returns) + " device busy returns\n" +
    String(td->_npackets) + " packets sent\n" +
#if CLICK_DEVICE_STATS
    String(td->_pull_cycles) + " cycles pull\n" +
    String(td->_time_clean) + " cycles clean\n" +
    String(td->_time_freeskb) + " cycles freeskb\n" +
    String(td->_time_queue) + " cycles queue\n" +
    String(td->_perfcnt1_pull) + " perfctr1 pull\n" +
    String(td->_perfcnt1_clean) + " perfctr1 clean\n" +
    String(td->_perfcnt1_freeskb) + " perfctr1 freeskb\n" +
    String(td->_perfcnt1_queue) + " perfctr1 queue\n" +
    String(td->_perfcnt2_pull) + " perfctr2 pull\n" +
    String(td->_perfcnt2_clean) + " perfctr2 clean\n" +
    String(td->_perfcnt2_freeskb) + " perfctr2 freeskb\n" +
    String(td->_perfcnt2_queue) + " perfctr2 queue\n" +
    String(td->_activations) + " transmit activations\n";
#else
    String();
#endif
}

static String
ToDevice_read_stats(Element *e, void *thunk)
{
  ToDevice *td = (ToDevice *)e;
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(td->_npackets) + "\n";
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
   case 1:
    return String(td->_pull_cycles) + "\n";
#endif
#if CLICK_DEVICE_STATS
   case 2:
    return String(td->_time_queue) + "\n";
   case 3:
    return String(td->_time_clean) + "\n";
#endif
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
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  add_read_handler("pull_cycles", ToDevice_read_stats, (void *)1);
#endif
#if CLICK_DEVICE_STATS
  add_read_handler("enqueue_cycles", ToDevice_read_stats, (void *)2);
  add_read_handler("clean_dma_cycles", ToDevice_read_stats, (void *)3);
#endif
  add_write_handler("reset_counts", ToDevice_write_stats, 0);
  add_task_handlers(&_task);
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(ToDevice)
