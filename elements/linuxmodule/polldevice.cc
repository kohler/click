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
  : _dev(0), _last_rx(0)
{
  add_output();
#if DEV_KEEP_STATS
  _idle_calls = 0;
  _pkts_received = 0;
  _activations = 0;
  _time_recv = 0;
  _time_pushing = 0;
  _time_running = 0;
  _time_first_recv = 0;
  _time_clean = 0;
  _l2misses_clean = 0;
  _dcu_cycles_clean = 0;
  _l2misses_touch = 0;
  _dcu_cycles_touch = 0;
  _l2misses_rx = 0;
  _dcu_cycles_rx = 0;
  _l2misses_first = 0;
  _dcu_cycles_first = 0;
#endif
}

PollDevice::PollDevice(const String &devname)
  : _devname(devname), _dev(0), _last_rx(0)
{
#ifdef CLICK_BENCHMARK
  _bm_done = 0;
#endif
  add_output();
#if DEV_KEEP_STATS
  _idle_calls = 0;
  _pkts_received = 0;
  _activations = 0;
  _time_recv = 0;
  _time_pushing = 0;
  _time_running = 0;
  _time_first_recv = 0;
  _time_clean = 0;
  _l2misses_clean = 0;
  _dcu_cycles_clean = 0;
  _l2misses_touch = 0;
  _dcu_cycles_touch = 0;
  _l2misses_rx = 0;
  _dcu_cycles_rx = 0;
  _l2misses_first = 0;
  _dcu_cycles_first = 0;
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
  // start out with default number of tickets, inflate up to max
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(ScheduleInfo::DEFAULT);
#endif
  join_scheduler();
  
  return 0;
#else
  return errh->warning("can't get packets: not compiled with polling extensions");
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
  
  /* need to have this somewhere */
  // _dev->tx_clean(_dev);

#if DEV_KEEP_STATS
  unsigned long time_now = get_cycles();
  unsigned low, high;
  unsigned low00, low01, low10, low11;

  rdpmc(0, low00, high);
  rdpmc(1, low10, high);

  unsigned long tt;
  tt = get_cycles();
#endif   
  
  got = POLLDEV_MAX_PKTS_PER_RUN;
  skb_list = _dev->rx_poll(_dev, &got);
  
  /* POLLDEV_MAX_PKTS_PER_RUN must not be greater than RX ring size */
  _dev->rx_refill(_dev);

#if DEV_KEEP_STATS
  if (_activations > 0 || got > 0) {
    _activations++;
    _time_first_recv += get_cycles()-tt;
      
    rdpmc(0, low01, high);
    rdpmc(1, low11, high);
    
    _l2misses_first += 
      (low01 >= low00)?low01 - low00 : (UINT_MAX - low00 + low01);
    _dcu_cycles_first += 
      (low11 >= low10)?low11 - low10 : (UINT_MAX - low10 + low11);
  
    rdpmc(0, low00, high); 
    rdpmc(1, low10, high);

    _pkts_received += got;
    if (got == 0) _idle_calls++;
  
    tt = get_cycles();
  }

#endif

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
  
#if 0
    if (skb_next) {
      volatile char pp0 = *(skb_next->data);
      volatile char pp1 = *(skb_next->data+33);
    }
#endif

    output(0).push(p);
  }
  assert(skb_list == NULL);

#if DEV_KEEP_STATS
  rdpmc(0, low01, high);
  rdpmc(1, low11, high);
  
  if (_activations > 0 || got > 0) {
    _dcu_cycles_touch += 
      (low11 >= low10)?low11 - low10 : (UINT_MAX - low10 + low11);
    _l2misses_touch += 
      (low01 >= low00)?low01 - low00 : (UINT_MAX - low00 + low01);
    _time_pushing += get_cycles()-tt;
  }
#endif

#ifndef RR_SCHED
  /* adjusting tickets */
  int adj = tickets()/4;
  if (adj<2) adj=2;
  /* handles burstiness: fast start */
  if (got == POLLDEV_MAX_PKTS_PER_RUN && 
      _last_rx <= POLLDEV_MAX_PKTS_PER_RUN/4) adj*=2;
  /* rx dma ring is fairly full, schedule ourselves more */
  else if (got == POLLDEV_MAX_PKTS_PER_RUN);
  /* rx dma ring was fairly empty, schedule ourselves less */
  else if (got < POLLDEV_MAX_PKTS_PER_RUN/4 && 
           _last_rx < POLLDEV_MAX_PKTS_PER_RUN/4) adj=0-adj;
  else adj=0;

  adj_tickets(adj);

  _last_rx = got;
  reschedule();
#endif

#if DEV_KEEP_STATS
  if (_activations>0)
    _time_running += get_cycles()-time_now;
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
    String(kw->_time_recv) + " cycles rx\n" +
    String(kw->_time_pushing) + " cycles pushing\n" +
    String(kw->_time_running) + " cycles running\n" +
    String(kw->_time_first_recv) + " cycles first rx\n" +
    String(kw->_time_clean) + " cycles cleaning\n" +
    String(kw->_l2misses_first) + " l2 misses first\n" +
    String(kw->_dcu_cycles_first) + " dcu cycles first\n" +
    String(kw->_l2misses_rx) + " l2 misses rx\n" +
    String(kw->_dcu_cycles_rx) + " dcu cycles rx\n" +
    String(kw->_l2misses_clean) + " l2 misses cleaning\n" +
    String(kw->_dcu_cycles_clean) + " dcu cycles cleaning\n" +
    String(kw->_l2misses_touch) + " l2 misses touching\n" +
    String(kw->_dcu_cycles_touch) + " dcu cycles touching\n" +
    String(kw->_activations) + " activations\n";
#else
    String();
#endif
}

void
PollDevice::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("calls", PollDevice_read_calls, 0);
}

EXPORT_ELEMENT(PollDevice)
