
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

extern "C" {
#include <linux/netdevice.h>
#include <unistd.h>
}

static int PollDevice::_num_polldevices = 0;
static int PollDevice::_num_idle_polldevices = 0;

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

  router()->can_wait(this);

  _dev->intr_off(_dev);
  _dev->intr_defer = 1;
  _idle = 0;
  _total_intr_wait = 0;

  _num_polldevices++;
  return 0;
}


void
PollDevice::uninitialize()
{
  if (_dev)
  { 
    _num_polldevices--;
    if (_idle >= POLLDEV_IDLE_LIMIT) 
      _num_idle_polldevices--;
    _idle = 0;

    _dev->intr_defer = 0; 
    _dev->intr_on(_dev);
    click_chatter("PollDevice(%s): %d intrs, %d rcvd", 
	_dev->name, _total_intr_wait, _pkts_received);
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
    _dev->fill_rx(_dev);

    if (_idle >= POLLDEV_IDLE_LIMIT)
      _num_idle_polldevices--; 
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
  _dev->clean_tx(_dev);
  _idle++;

  if (_idle >= POLLDEV_IDLE_LIMIT) {
    if (_idle == POLLDEV_IDLE_LIMIT)
      _num_idle_polldevices++;
    if (_num_idle_polldevices == _num_polldevices)
    {
      Vector<PollDevice *> polldevices;
      ElementLink *n = scheduled_list()->scheduled_next();
      while(n != scheduled_list()) {
        if (!((Element*)n)->is_a(class_name())) {
          reschedule();
          return;
        }
	else 
	  polldevices.push_back((PollDevice*)n);
        n = n->scheduled_next();
      }
      PollDevice *p;
      for(int i=0; i<polldevices.size(); i++)
	polldevices[i]->unschedule();
      return;
    }
  }

  if (got == POLLDEV_MAX_PKTS_PER_RUN)
    adj_tickets(max_ntickets());
  else if (_idle > 2) {
    int n = ntickets()/4;
    if (n==0) n=1;
    adj_tickets(0-n);
  }
  reschedule();
}
 
void
PollDevice::set_wakeup_when_busy()
{
  /* put self on intr wait queue */
  _self_wq.task = current;
  _self_wq.next = NULL;
  add_wait_queue(&(_dev->intr_wq), &_self_wq);

  /* turn interrupts back on */
  _total_intr_wait++;
  _dev->intr_on(_dev);
}

void
PollDevice::woke_up()
{
  _dev->intr_off(_dev);
  remove_wait_queue(&(_dev->intr_wq), &_self_wq);
    
  if (_idle >= POLLDEV_IDLE_LIMIT) 
    _num_idle_polldevices--; 
  _idle = 0;

  join_scheduler();
}

EXPORT_ELEMENT(PollDevice)
