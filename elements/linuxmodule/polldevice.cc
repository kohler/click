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
  : _dev(0), _last_dma_length(0),
    _pkts_received(0), _pkts_on_dma(0), _activations(0),
    _tks_allocated(0)
{
  add_output();
  _dma_burst_resched = 0;
  _dma_full_resched = 0;
  _dma_empty_resched = 0;
  _idle_calls = 0;
  _dma_full = 0;
}

PollDevice::PollDevice(const String &devname)
  : _devname(devname), _dev(0), _last_dma_length(0),
    _pkts_received(0), _pkts_on_dma(0), _activations(0),
    _tks_allocated(0)
{
#ifdef CLICK_BENCHMARK
  _bm_done = 0;
#endif
  add_output();
  _dma_burst_resched = 0;
  _dma_full_resched = 0;
  _dma_empty_resched = 0;
  _idle_calls = 0;
  _dma_full = 0;
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

  // start out with default number of tickets, inflate up to max
  int max_tickets = ScheduleInfo::query(this, errh);
  set_max_tickets(max_tickets);
  set_tickets(ScheduleInfo::DEFAULT);
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
    _dev->intr_defer = 0; 
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

  // _dev->clean_tx(_dev);

  /* order of && clauses important */
  while(got<POLLDEV_MAX_PKTS_PER_RUN && (skb = _dev->rx_poll(_dev))) {
    _pkts_received++;

    assert(skb->data - skb->head >= 14);
    assert(skb->mac.raw == skb->data - 14);
    assert(skb_shared(skb) == 0);

    /* Retrieve the ether header. */
    skb_push(skb, 14);

    Packet *p = Packet::make(skb);
    if(skb->pkt_type == PACKET_MULTICAST || skb->pkt_type == PACKET_BROADCAST)
      p->set_mac_broadcast_anno(1);

    output(0).push(p);
    got++;
  }
  
  /* !!! careful: if POLLDEV_MAX_PKTS_PER_RUN is greater than RX ring size, we
   * need to fill_rx more often... by default, RX ring size is 32, and we set
   * POLLDEV_MAX_PKTS_PER_RUN to 8, so we can fill once per run */

  int queued_pkts_now = _dev->clean_rx(_dev);
  _pkts_on_dma += queued_pkts_now;
  int queued_pkts_before = queued_pkts_now + got;
  
  if (_activations > 0 || got > 0) {
    _activations++;
    _tks_allocated += ntickets();
    if (got == 0) _idle_calls++;
    if (queued_pkts_before >= _dev->rx_dma_length-4)
      _dma_full++;
  }

  /* adjusting tickets */
  int dmal = _dev->rx_dma_length;
  int dma_thresh_high = dmal/4;
  int dma_thresh_low  = dmal/8;
  int adj = ntickets()/4;
  if (adj<2) adj=2;

  /* handles burstiness: fast start */
  if (queued_pkts_before > dma_thresh_high && 
      _last_dma_length < dma_thresh_low) {
    adj *= 2;
    if (_activations>0) _dma_burst_resched++;
  }

  /* rx dma ring is fairly full, schedule ourselves more */
  else if (queued_pkts_before > dma_thresh_high) {
    if (_activations>0) _dma_full_resched++;
  }
 
  /* rx dma ring was fairly empty, schedule ourselves less */
  else if (queued_pkts_before < dma_thresh_low && 
           _last_dma_length < dma_thresh_low) {
    adj=0-adj;
    if (_activations>0) _dma_empty_resched++;
  }

  adj_tickets(adj);
  _last_dma_length = queued_pkts_before;

  reschedule();
#endif
}
 
static String
PollDevice_read_calls(Element *f, void *)
{
  PollDevice *kw = (PollDevice *)f;
  return
    String(kw->max_tickets()) + " maximum number of tickets\n" + 
    String(kw->_pkts_received) + " packets received\n" +
    String(kw->_dma_full) + " times DMA ring seems full\n" +
    String(kw->_pkts_on_dma) + " packets seen pending on DMA ring\n" +
    String(kw->_dma_burst_resched) + " dma burst resched\n" +
    String(kw->_dma_full_resched) + " dma full resched\n" +
    String(kw->_dma_empty_resched) + " dma empty resched\n" +
    String(kw->_tks_allocated) + " total tickets seen\n" +
    String(kw->_idle_calls) + " idle calls\n" +
    String(kw->_activations) + " activations\n";
}

void
PollDevice::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("calls", PollDevice_read_calls, 0);
}

EXPORT_ELEMENT(PollDevice)
