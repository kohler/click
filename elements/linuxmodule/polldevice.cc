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
  struct sk_buff *skb;
  int got=0;
  Packet *new_pkts[POLLDEV_MAX_PKTS_PER_RUN];

#if DEV_KEEP_STATS
  unsigned long time_now = get_cycles();
#endif   
  
  /* need to have this somewhere */
  // _dev->tx_clean(_dev);
  
  while(got<POLLDEV_MAX_PKTS_PER_RUN && (skb = _dev->rx_poll(_dev))) {
    assert(skb->data - skb->head >= 14);
    assert(skb->mac.raw == skb->data - 14);
    assert(skb_shared(skb) == 0);

    /* Retrieve the ether header. */
    skb_push(skb, 14);

    Packet *p = Packet::make(skb);
    if(skb->pkt_type == PACKET_MULTICAST || skb->pkt_type == PACKET_BROADCAST)
      p->set_mac_broadcast_anno(1);

    new_pkts[got] = p;
    got++;
  }
  
  /* if POLLDEV_MAX_PKTS_PER_RUN is greater than RX ring size, we need to fill
   * more often... */
  _dev->rx_refill(_dev);

#if DEV_KEEP_STATS
  if (_activations > 0 || got > 0) {
    _activations++;
    if (got == 0) _idle_calls++;
    _pkts_received += got;
    _time_recv += get_cycles()-time_now;
  }
#endif

#if DEV_KEEP_STATS
  unsigned long tmptime = get_cycles();
#endif

  for(int i=0; i<got; i++)
    output(0).push(new_pkts[i]);

#if DEV_KEEP_STATS
  if (_activations > 0 || got > 0)
    _time_pushing += get_cycles()-tmptime;
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
