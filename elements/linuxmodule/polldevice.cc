
/*
 * polldevice.{cc,hh} -- element steals packets from Linux devices by polling.
 * 
 * Benjie Chen
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
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

static int PollDevice::num_polldevices = 0;
static int PollDevice::num_idle_polldevices = 0;

PollDevice::PollDevice()
  : _dev(0)
{
  add_output();
  _pkts_received=0;
}

PollDevice::PollDevice(const String &devname)
  : _devname(devname), _dev(0)
{
#ifdef CLICK_BENCHMARK
  _bm_done = 0;
#endif
  add_output();
  _pkts_received=0;
}

PollDevice::~PollDevice()
{
}

void
PollDevice::static_initialize()
{
  num_polldevices = num_idle_polldevices = 0;
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
  _dev = dev_get(_devname.cc());
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  if (!_dev->pollable) 
    return errh->error("device `%s' not pollable", _devname.cc());

  _dev->intr_off(_dev);
  _dev->intr_defer = 1;
  _idle = 0;
  _total_intr_wait = 0;
  
  ScheduleInfo::join_scheduler(this, errh);
  
  num_polldevices++;
  return 0;
}


void
PollDevice::uninitialize()
{
  if (_dev) { 
    num_polldevices--;
    if (_idle >= POLLDEV_IDLE_LIMIT) 
      num_idle_polldevices--;
    _idle = 0;

    _dev->intr_defer = 0; 
    _dev->intr_on(_dev);
    
    unschedule();
  }
}

void
PollDevice::run_scheduled()
{
  extern int rtm_ipackets;
  extern unsigned long long rtm_ibytes;
  struct sk_buff *skb;
  int got=0;

  /* order of && clauses important */
  while(got<POLLDEV_MAX_PKTS_PER_RUN && (skb = _dev->rx_poll(_dev))) {
    _pkts_received++;
#if 0
    if (_idle >= POLLDEV_IDLE_LIMIT)
      num_idle_polldevices--; 
#endif
    _idle = 0;
    rtm_ipackets++;
    rtm_ibytes += skb->len;

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
  _dev->fill_rx(_dev);
  _dev->clean_tx(_dev);
  _idle++;

  if (got == POLLDEV_MAX_PKTS_PER_RUN)
    adj_tickets(max_ntickets());
  else if (_idle > 2) {
    int n = ntickets()/4;
    if (n==0) n=1;
    adj_tickets(0-n);
  }
  reschedule();
}
 
static String
PollDevice_read_calls(Element *f, void *)
{
  PollDevice *kw = (PollDevice *)f;
  return
    String(kw->_pkts_received) + " packets received\n" +
    String(kw->_total_intr_wait) + " waits with interrupts on\n";
}

void
PollDevice::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("calls", PollDevice_read_calls, 0);
}

EXPORT_ELEMENT(PollDevice)
