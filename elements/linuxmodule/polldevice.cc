/*
 * polldevice.{cc,hh} -- element steals packets from Linux devices by polling.
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
#include "fromdevice.hh"
#include "todevice.hh"
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
  : _activations(0), _registered(0), _last_rx(0), _manage_tx(1)
{
  add_output();
#if _CLICK_STATS_
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
  if (_registered) uninitialize();
}

void
PollDevice::static_initialize()
{
}

void
PollDevice::static_cleanup()
{
}


int
PollDevice::configure(const String &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_devname,
		  cpEnd) < 0)
    return -1;
#if HAVE_POLLING
  _dev = dev_get(_devname.cc());
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  if (!_dev->pollable) 
    return errh->error("device `%s' not pollable", _devname.cc());
#endif  
  return 0;
}


/*
 * Use Linux interface for polling, added by us, in include/linux/netdevice.h,
 * to poll devices.
 */
int
PollDevice::initialize(ErrorHandler *errh)
{
#if HAVE_POLLING
  /* try to find a ToDevice with the same device: if none exists, then we need
   * to manage tx queue as well as rx queue. need to do it this way because
   * ToDevice may not have been initialized
   */
  for (int fi = 0; fi < router()->nelements(); fi++) {
    Element *e = router()->element(fi);
    if (e == this) continue;
    if (ToDevice *td = (ToDevice *) (e->cast("ToDevice"))) {
      if (td->ifindex() == ifindex())
	_manage_tx = 0;
    } else if (PollDevice *pd=(PollDevice *)(e->cast("PollDevice"))) {
      if (pd->ifindex() == ifindex())
	return errh->error("duplicate PollDevice for `%s'", _devname.cc());
    } else if (FromDevice *fd = (FromDevice *)(e->cast("FromDevice"))) {
      if (fd->ifindex() == ifindex())
	return errh->error("both FromDevice and PollDevice for `%s'", 
	                   _devname.cc());
    }
  }

  // install PollDevice
  _registered = 1;
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
  if (_dev && _dev->pollable)
    _dev->intr_on(_dev);
  _registered = 0;
  unschedule();
#endif
}

void
PollDevice::run_scheduled()
{
#if HAVE_POLLING
  struct sk_buff *skb_list, *skb;
  int got=0;
#if _CLICK_STATS_
  unsigned long time_now;
  unsigned low00, low10;
#endif

  if (_manage_tx) 
    _dev->tx_clean(_dev);

  SET_STATS(low00, low10, time_now);

  got = INPUT_MAX_PKTS_PER_RUN;
  skb_list = _dev->rx_poll(_dev, &got);

  if (got > 0 || _activations > 0) {
    _activations++;
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_poll, _perfcnt2_poll, _time_poll);
#if _CLICK_STATS_
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

  if (_activations > 0 && got > 0) {
    GET_STATS_RESET(low00, low10, time_now, 
	            _perfcnt1_pushing, _perfcnt2_pushing, _time_pushing);
#if _DEV_OVRN_STATS_
    if ((_activations % 1024) == 0) _dev->get_stats(_dev);
#endif
  }

#ifndef RR_SCHED
#ifdef ADJ_TICKETS
  /* adjusting tickets */
  int adj = tickets()/4;
  if (adj<2) adj=2;
  
  /* handles burstiness: fast start */
  if (got == INPUT_MAX_PKTS_PER_RUN && 
      _last_rx <= INPUT_MAX_PKTS_PER_RUN/8) adj*=2;
  /* rx dma ring is fairly full, schedule ourselves more */
  else if (got == INPUT_MAX_PKTS_PER_RUN);
  /* rx dma ring was fairly empty, schedule ourselves less */
  else if (got < INPUT_MAX_PKTS_PER_RUN/4) adj=0-adj;
  else adj=0;

  adj_tickets(adj);
#endif

  _last_rx = got;
  reschedule();
#endif /* !RR_SCHED */

#endif /* HAVE_POLLING */
}
 
static String
PollDevice_read_calls(Element *f, void *)
{
  PollDevice *kw = (PollDevice *)f;
  return
#if _CLICK_STATS_
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
PollDevice::add_handlers()
{
  add_read_handler("calls", PollDevice_read_calls, 0);
}

EXPORT_ELEMENT(PollDevice)
ELEMENT_REQUIRES(AnyDevice)
