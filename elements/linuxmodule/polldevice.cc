/*
 * polldevice.{cc,hh} -- element steals packets from Linux devices by polling.
 * 
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "polldevice.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "router.hh"
#include "elemfilter.hh"
#include "elements/standard/scheduleinfo.hh"

extern "C" {
#include <linux/netdevice.h>
#include <unistd.h>
}

#include "perfcount.hh"
#include "asm/msr.h"

PollDevice::PollDevice()
  : _dev(0), _activations(0), _last_rx(0)
{
  add_output();
#if DEV_KEEP_STATS
  _idle_calls = 0;
  _pkts_received = 0;
  _time_poll = 0;
  _time_refill = 0;
  _time_pushing = 0;
  _perfcnt1_poll = 0;
  _perfcnt1_refill = 0;
  _perfcnt1_pushing = 0;
  _perfcnt2_poll = 0;
  _perfcnt2_refill = 0;
  _perfcnt2_pushing = 0;
#endif
}

PollDevice::PollDevice(const String &devname)
  : _devname(devname), _dev(0), _activations(0), _last_rx(0)
{
#ifdef CLICK_BENCHMARK
  _bm_done = 0;
#endif
  add_output();
#if DEV_KEEP_STATS
  _idle_calls = 0;
  _pkts_received = 0;
  _time_poll = 0;
  _time_refill = 0;
  _time_pushing = 0;
  _perfcnt1_poll = 0;
  _perfcnt1_refill = 0;
  _perfcnt1_pushing = 0;
  _perfcnt2_poll = 0;
  _perfcnt2_refill = 0;
  _perfcnt2_pushing = 0;
#endif
}

PollDevice::~PollDevice()
{
}

void
PollDevice::static_initialize()
{
}

void
PollDevice::static_cleanup()
{
}


PollDevice *
PollDevice::clone() const
{
  return new PollDevice();
}

int
PollDevice::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpString, "interface name", &_devname,
		     cpEnd);
}


/*
 * Use Linux interface for polling, added by us, in include/linux/netdevice.h,
 * to poll devices.
 */
int
PollDevice::initialize(ErrorHandler *errh)
{
#if HAVE_POLLING
  _dev = dev_get(_devname.cc());
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  if (!_dev->pollable) 
    return errh->error("device `%s' not pollable", _devname.cc());
  
  _dev->intr_off(_dev);

#ifndef RR_SCHED
  /* start out with default number of tickets, inflate up to max */
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(ScheduleInfo::DEFAULT);
#endif
  join_scheduler();
  
  return 0;
#else
  errh->warning("can't get packets: not compiled with polling extensions");
  return 0;
#endif
}


void
PollDevice::uninitialize()
{
#if HAVE_POLLING
  if (_dev) {
    _dev->intr_on(_dev);
    unschedule();
  }
#endif
}

void
PollDevice::run_scheduled()
{
#if HAVE_POLLING
  struct sk_buff *skb_list, *skb;
  int got=0;
#if DEV_KEEP_STATS
  unsigned long time_now;
  unsigned low00, low10;
#endif
  
  /* need to have this somewhere */
  /* _dev->tx_clean(_dev); */

  SET_STATS(low00, low10, time_now);

#ifdef BATCH_PKT_PROC
  
  got = POLLDEV_MAX_PKTS_PER_RUN;
  skb_list = _dev->rx_poll(_dev, &got);

  if (got > 0 || _activations > 0) {
    _activations++;
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_poll, _perfcnt2_poll, _time_poll);
#if DEV_KEEP_STATS
    _pkts_received += got;
    if (got == 0) _idle_calls++;
#endif
  }
  
  _dev->rx_refill(_dev);
  
  if (_activations > 0)
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_refill, _perfcnt2_refill, _time_refill);

  for(int i=0; i<got; i++) {
    struct sk_buff *skb_next;
    skb = skb_list;
    skb_next = skb_list = skb_list->next;
    skb->next = skb->prev = NULL;
    
    assert(skb);
    assert(skb->data - skb->head >= 14);
    assert(skb->mac.raw == skb->data - 14);
    assert(skb_shared(skb) == 0);

    /* Retrieve the ether header. */
    skb_push(skb, 14);

    Packet *p = Packet::make(skb);
    if(skb->pkt_type == PACKET_MULTICAST || skb->pkt_type == PACKET_BROADCAST)
      p->set_mac_broadcast_anno(1);
  
    output(0).push(p);
  }
  assert(skb_list == NULL);

  if (_activations > 0 && got > 0)
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_pushing, _perfcnt2_pushing, _time_pushing);

#else  /* BATCH_PKT_PROC */
 
  int i=1;
  got = 0;
  while(i > 0 && got < POLLDEV_MAX_PKTS_PER_RUN) {
    i = 1;

    skb_list = _dev->rx_poll(_dev, &i);

    if (i > 0) {
      GET_STATS_RESET(low00, low10, time_now, 
	              _perfcnt1_poll, _perfcnt2_poll, _time_poll);

      got += i;
      skb = skb_list;
      assert(skb);
      assert(skb->data - skb->head >= 14);
      assert(skb->mac.raw == skb->data - 14);
      assert(skb_shared(skb) == 0);

      /* Retrieve the ether header. */
      skb_push(skb, 14);

      Packet *p = Packet::make(skb);
      if(skb->pkt_type == PACKET_MULTICAST || skb->pkt_type == PACKET_BROADCAST)
        p->set_mac_broadcast_anno(1);
  
      output(0).push(p);
      
      GET_STATS_RESET(low00, low10, time_now, 
	              _perfcnt1_pushing, _perfcnt2_pushing, _time_pushing);
    }
  }

  if (got > 0 || _activations > 0) _activations++;

  if (_activations > 0)
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_poll, _perfcnt2_poll, _time_poll);

  _dev->rx_refill(_dev);

  if (_activations > 0) {
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_refill, _perfcnt2_refill, _time_refill);
#if DEV_KEEP_STATS
    if (got == 0) _idle_calls++;
    else _pkts_received+=got;
#endif
  }

#endif /* !BATCH_PKT_PROC */

#ifndef RR_SCHED
#ifdef ADJ_TICKETS
  /* adjusting tickets */
  int adj = tickets()/4;
  if (adj<2) adj=2;
  
  /* handles burstiness: fast start */
  if (got == POLLDEV_MAX_PKTS_PER_RUN && 
      _last_rx <= POLLDEV_MAX_PKTS_PER_RUN/8) adj*=2;
  /* rx dma ring is fairly full, schedule ourselves more */
  else if (got == POLLDEV_MAX_PKTS_PER_RUN);
  /* rx dma ring was fairly empty, schedule ourselves less */
  else if (got < POLLDEV_MAX_PKTS_PER_RUN/4) adj=0-adj;
  else adj=0;

  adj_tickets(adj);
#endif

  _last_rx = got;
  reschedule();
#endif /* !RR_SCHED */

#ifdef DEV_RXFIFO_STATS
  if ((_activations % 2048)==0) _dev->get_stats(_dev);
#endif

#endif /* HAVE_POLLING */
}
 
static String
PollDevice_read_calls(Element *f, void *)
{
  PollDevice *kw = (PollDevice *)f;
  return
#if DEV_KEEP_STATS
    String(kw->_idle_calls) + " idle calls\n" +
    String(kw->_pkts_received) + " packets received\n" +
    String(kw->_time_poll) + " cycles poll\n" +
    String(kw->_time_refill) + " cycles refill\n" +
    String(kw->_time_pushing) + " cycles pushing\n" +
    String(kw->_perfcnt1_poll) + " perfctr1 poll\n" +
    String(kw->_perfcnt1_refill) + " perfctr1 refill\n" +
    String(kw->_perfcnt1_pushing) + " perfctr1 pushing\n" +
    String(kw->_perfcnt2_poll) + " perfctr2 poll\n" +
    String(kw->_perfcnt2_refill) + " perfctr2 refill\n" +
    String(kw->_perfcnt2_pushing) + " perfctr2 pushing\n" +
#endif
    String(kw->_activations) + " activations\n";
}

void
PollDevice::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("calls", PollDevice_read_calls, 0);
}

EXPORT_ELEMENT(PollDevice)
