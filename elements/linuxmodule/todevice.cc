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
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/glue.hh>
#include "polldevice.hh"
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include "elements/standard/scheduleinfo.hh"
extern "C" {
#define new xxx_new
#define class xxx_class
#define delete xxx_delete
#include <net/pkt_sched.h>
#undef new
#undef class
#undef delete
}

#include <asm/msr.h>

ToDevice::ToDevice()
  : _polling(0), _registered(0),
    _dev_idle(0), _last_tx(0), _last_busy(0), 
    _rejected(0), _hard_start(0)
{
  add_input();
}

ToDevice::~ToDevice()
{
  if (_registered) uninitialize();
}


int
ToDevice::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_devname,
		  cpEnd) < 0)
    return -1;
  _dev = dev_get(_devname.cc());
  if (!_dev)
    _dev = find_device_by_ether_address(_devname, this);
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
#ifndef HAVE_CLICK_KERNEL
  errh->warning("not compiled for a Click kernel");
#endif

  /* see if a PollDevice with the same device exists: if so, use polling
   * extensions */
  // need to do it this way because ToDevice may not have been initialized
  for (int fi = 0; fi < router()->nelements(); fi++) {
    Element *e = router()->element(fi);
    if (e == this) continue;
    if (ToDevice *td=(ToDevice *) (e->cast("ToDevice"))) {
      if (td->ifindex() == ifindex())
	return errh->error("duplicate ToDevice for `%s'", _devname.cc());
    } else if (PollDevice *pd = (PollDevice *)(e->cast("PollDevice"))) {
      if (pd->ifindex() == ifindex())
	_polling = 1;
    }
  }

  _registered = 1;

#ifndef RR_SCHED
  /* start out with max number of tickets */
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(max_tickets);
#endif
  join_scheduler();

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
  _idle_pulls = 0; 
  _idle_calls = 0; 
  _linux_pkts_sent = 0; 
  _time_clean = 0;
  _time_queue = 0;
  _perfcnt1_pull = 0;
  _perfcnt1_clean = 0;
  _perfcnt1_queue = 0;
  _perfcnt2_pull = 0;
  _perfcnt2_clean = 0;
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
  unschedule();
}

/*
 * The kernel thinks our device is idle.
 * Pull a packet and try to stuff it into the device.
 * 
 * Serious problem: Linux drivers aren't required to
 * accept a packet even if they've marked themselves
 * as idle. What do we do with a rejected packet?
 */
bool
ToDevice::tx_intr()
{
  int busy;
  int sent = 0;
  int queued_pkts;
  
#if CLICK_DEVICE_STATS
  unsigned low00, low10;
  unsigned long long time_now;
#endif

  SET_STATS(low00, low10, time_now);
 
#if HAVE_POLLING
  if (_polling) {
    queued_pkts = _dev->tx_clean(_dev);

#if CLICK_DEVICE_STATS
    GET_STATS_RESET(low00, low10, time_now, 
		    _perfcnt1_clean, _perfcnt2_clean, _time_clean);
#endif
  }
#endif

  /* try to send from click */
  while (sent < OUTPUT_BATCH && (busy=_dev->tbusy) == 0) {
#if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
    unsigned long long before_pull_cycles = click_get_cycles();
#endif
    
    if (Packet *p = input(0).pull()) {
      
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
    else break;
  }

#if HAVE_POLLING
  if (_polling && !_dev->tbusy && sent < OUTPUT_BATCH) {
    /* try to send from linux if click queue is empty */
    start_bh_atomic();
    while (sent < OUTPUT_BATCH && (busy=_dev->tbusy) == 0) {
      int r;
      
      if ((r = qdisc_restart(_dev)) < 0) {
	/* if qdisc_restart returns -1, that means a packet is sent as long as
	 * dev->tbusy is not set... see net/sched/sch_generic.c in linux src
	 * code */
	sent++;
#if CLICK_DEVICE_STATS
        _linux_pkts_sent++;
#endif
      }
      else break;
    }
    end_bh_atomic();
  }
#endif

#if CLICK_DEVICE_STATS
  if (sent > 0 || _activations > 0) _activations++;
#endif

#if CLICK_DEVICE_STATS
  if (_activations > 0) {
    if (sent == 0) _idle_calls++;
    if (sent == 0 && !busy) _idle_pulls++;
  }
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
 
#if CLICK_DEVICE_ADJUST_TICKETS
  int base = tickets()/4;
  if (base < 2) base = 2;
  int adj = 0;

  /* 
   * didn't get much traffic and did not fill up the device, slow down.
   */
  if (!busy && sent < (OUTPUT_BATCH/4)) 
    adj = -base;
  /* 
   * sent many packets, increase ticket.
   */
  else if (sent > (OUTPUT_BATCH/2)) 
    adj = base * 2;
  /*
   * was able to send more packets than last time, and last time device wasn't
   * busy, this means we are getting more packets from queue.
   */
  else if (sent > (OUTPUT_BATCH/4) && sent > _last_tx && !_last_busy)
    adj = base;

  adj_tickets(adj);
#endif
  
  _last_tx = sent;
  _last_busy = busy;
  reschedule();
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
    skb_put(skb1, 60 - skb1->len);
  }

  int ret;
#if HAVE_POLLING
  if (_polling)
    ret = _dev->tx_queue(skb1, _dev); 
  else 
#endif
  {
    ret = _dev->hard_start_xmit(skb1, _dev);
    _hard_start++;
  }
  if(ret != 0){
    printk("<1>ToDevice %s tx oops\n", _dev->name);
    kfree_skb(skb1);
    _rejected++;
  }
  return ret;
}

void
ToDevice::run_scheduled()
{
  tx_intr();
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
    String(td->_idle_calls) + " idle tx calls\n" +
    String(td->_idle_pulls) + " idle pulls\n" +
    String(td->_linux_pkts_sent) + " linux packets sent\n" +
    String(td->_pull_cycles) + " cycles pull\n" +
    String(td->_time_clean) + " cycles clean\n" +
    String(td->_time_queue) + " cycles queue\n" +
    String(td->_perfcnt1_pull) + " perfctr1 pull\n" +
    String(td->_perfcnt1_clean) + " perfctr1 clean\n" +
    String(td->_perfcnt1_queue) + " perfctr1 queue\n" +
    String(td->_perfcnt2_pull) + " perfctr2 pull\n" +
    String(td->_perfcnt2_clean) + " perfctr2 clean\n" +
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
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(ToDevice)
