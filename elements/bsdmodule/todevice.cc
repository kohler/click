/*
 * todevice.{cc,hh} -- element sends packets to BSD devices.
 * Robert Morris
 * Eddie Kohler: register once per configuration
 * Benjie Chen: polling
 * Nickolai Zeldovich: BSD
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
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
#if 0
#include "polldevice.hh"
#endif
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT

CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

/* for watching when devices go offline */
static AnyDeviceMap to_device_map;
static int to_device_count;
#if 0 /* Not yet in BSD XXX */
static struct notifier_block device_notifier;
extern "C" {
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
}
#endif

static void
todev_static_initialize()
{
    if (++to_device_count == 1) {
	to_device_map.initialize();
#if 0 /* XXX not yet in BSD XXX */
	device_notifier.notifier_call = device_notifier_hook;
	device_notifier.priority = 1;
	device_notifier.next = 0;
	register_netdevice_notifier(&device_notifier);
#endif
    }
}

static void
todev_static_cleanup()
{
    if (--to_device_count <= 0) {
#if 0 /* XXX not yet in BSD */
	unregister_netdevice_notifier(&device_notifier);
#endif
    }
}

ToDevice::ToDevice()
  : _dev_idle(0), _rejected(0), _hard_start(0)
{
    // no MOD_INC_USE_COUNT; rely on AnyDevice
    add_input();
    todev_static_initialize();
}

ToDevice::~ToDevice()
{
    // no MOD_DEC_USE_COUNT; rely on AnyDevice
    todev_static_cleanup();
}


int
ToDevice::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _burst = 16;
  bool allow_nonexistent = false;
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_devname,
		  cpOptional,
		  cpUnsigned, "burst size", &_burst,
		  cpKeywords,
		  "BURST", cpUnsigned, "burst size", &_burst,
		  "ALLOW_NONEXISTENT", cpBool, "allow nonexistent interface?", &allow_nonexistent,
		  cpEnd) < 0)
    return -1;

  if (find_device(allow_nonexistent, errh) < 0)
      return -1;
  return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
#if 0 /* XXX no polling in bsd yet */
  // see if a PollDevice with the same device exists: if so, use polling
  // extensions. Also look for duplicate ToDevices; but beware: ToDevice may
  // not have been initialized
  if (_dev)
      for (int ei = 0; ei < router()->nelements(); ei++) {
	  Element *e = router()->element(ei);
	  if (e == this) continue;
	  if (ToDevice *td = (ToDevice *)(e->cast("ToDevice"))) {
	      if (td->ifindex() == ifindex())
		  return errh->error("duplicate ToDevice for `%s'", _devname.cc());
	  }
      }
#endif

  to_device_map.insert(this);
  
  ScheduleInfo::initialize_task(this, &_task, _dev != 0, errh);
#ifdef HAVE_STRIDE_SCHED
  /* start out with max number of tickets */
  _max_tickets = _task.tickets();
  _task.set_tickets(Task::DEFAULT_TICKETS);
#endif

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
  to_device_map.remove(this);
  _task.unschedule();
}

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
 
#if HAVE_TX_POLLING
  bool is_polling = (_dev->polling > 0);
  if (is_polling) {
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
#endif	/* HAVE_TX_POLLING */
  
  SET_STATS(low00, low10, time_now);

  /* try to send from click */
  while (sent < _burst && (busy = IF_QFULL(&_dev->if_snd)) == 0) {

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

#if HAVE_TX_POLLING
  if (is_polling && sent > 0)
    _dev->tx_eob(_dev);

  // If Linux tried to send a packet, but saw tbusy, it will
  // have left it on the queue. It'll just sit there forever
  // (or until Linux sends another packet) unless we poke
  // net_bh(), which calls qdisc_restart(). We are not allowed
  // to call qdisc_restart() ourselves, outside of net_bh().
  if (is_polling && !busy && _dev->qdisc->q.qlen) {
    _dev->tx_eob(_dev);
    netif_wake_queue(_dev);
  }
#endif	/* HAVE_TX_POLLING */

#if CLICK_DEVICE_STATS
  if (sent > 0) _activations++;
#endif

  if (busy) _busy_returns++;

#if HAVE_TX_POLLING
  if (is_polling) {
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
#endif	/* HAVE_TX_POLLING */

  adjust_tickets(sent);
  _task.fast_reschedule();
}

int
ToDevice::queue_packet(Packet *p)
{
  struct mbuf *m = p->steal_m();
  int ret;

#if HAVE_TX_POLLING
  if (_dev->polling > 0)
    ret = _dev->tx_queue(_dev, skb1);
  else 
#endif	/* HAVE_TX_POLLING */
  {
    ret = ether_output_frame(_dev, m);
    _hard_start++;
  }
  if(ret != 0){
    struct mbuf *n;
    if(_rejected == 0)
      printf("ToDevice %s rejected a packet!\n", _dev->if_name);
    while (m) {
      MFREE(m, n);
      m = n;
    }
    _rejected++;
  }
  return ret;
}

#if 0 /* XXX no device monitoring in BSD yet */
void
ToDevice::change_device(struct if_net *dev)
{
    _task.unschedule();
    
    if (!_dev && dev)
	click_chatter("%s: device `%s' came up", declaration().cc(), _devname.cc());
    else if (_dev && !dev)
	click_chatter("%s: device `%s' went down", declaration().cc(), _devname.cc());
    
    to_device_map.remove(this);
#if LINUX_VERSION_CODE >= 0x020400
    if (_dev)
	dev_put(_dev);
#endif
    _dev = dev;
#if LINUX_VERSION_CODE >= 0x020400
    if (_dev)
	dev_hold(_dev);
#endif
    to_device_map.insert(this);

    if (_dev)
	_task.reschedule();
}

extern "C" {
static int
device_notifier_hook(struct notifier_block *nb, unsigned long flags, void *v)
{
    struct if_net *dev = (struct if_net *)v;
    if (flags == NETDEV_UP) {
	if (ToDevice *td = (ToDevice *)to_device_map.lookup_unknown(dev))
	    td->change_device(dev);
    } else if (flags == NETDEV_DOWN) {
	if (ToDevice *td = (ToDevice *)to_device_map.lookup(dev))
	    td->change_device(0);
    }
    return 0;
}
}
#endif

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

ELEMENT_REQUIRES(AnyDevice bsdmodule)
EXPORT_ELEMENT(ToDevice)
