
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

extern "C" {
#include <linux/netdevice.h>
}


PollDevice::PollDevice()
  : _dev(0)
{
  add_output();
}

PollDevice::PollDevice(const String &devname)
  : _devname(devname), _dev(0)
{
#ifdef CLICK_BENCHMARK
  _bm_done = 0;
#endif
  add_output();
}

PollDevice::~PollDevice()
{
  uninitialize();
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
  _dev = dev_get(_devname.cc());
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  if (!_dev->pollable) 
    return errh->error("device `%s' not pollable", _devname.cc());
  _dev->intr_off(_dev);
  _dev->intr_defer = 1;
  idle = 0;
  total_intr_wait = 0;
  return 0;
}


void
PollDevice::uninitialize()
{
  if (_dev)
  { 
    _dev->intr_defer = 0; 
    _dev->intr_on(_dev);
    click_chatter
	("PollDevice(%s): waited with intr on %d times\n", 
	 _dev->name, total_intr_wait);
  }
}


Packet *
PollDevice::pull(int)
{
  extern int rtm_ipackets;
  extern unsigned long long rtm_ibytes;
  struct sk_buff *skb;

  skb = _dev->rx_poll(_dev);
  _dev->fill_rx(_dev);
  _dev->clean_tx(_dev);

  if (skb != 0L)
  {
    idle = 0;
    rtm_ipackets++;
    rtm_ibytes += skb->len;

    assert(skb->data - skb->head >= 14);
    assert(skb->mac.raw == skb->data - 14);
    assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

    /* Retrieve the ether header. */
    skb_push(skb, 14);

    Packet *p = Packet::make(skb);
    if(skb->pkt_type == PACKET_MULTICAST || skb->pkt_type == PACKET_BROADCAST)
      p->set_mac_broadcast_anno(1);

    return p;
  } 
  else 
  {
    idle++;
    return 0L;
  }
}
 

bool
PollDevice::still_busy()
{
  if (idle <= 100) return true;
  return false;
}

struct wait_queue** 
PollDevice::get_wait_queue()
{
  if (idle > 100) 
    return &_dev->intr_wq;
  else
    return NULL;
}

void 
PollDevice::do_waiting()
{ 
  total_intr_wait++;
  _dev->intr_on(_dev);
}

void
PollDevice::finish_waiting()
{
  _dev->intr_off(_dev);
}


ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(PollDevice)

