#ifndef POLLDEVICE_HH
#define POLLDEVICE_HH

extern "C" {
#include <linux/netdevice.h>
}

/*
 * =c
 * PollDevice(devname)
 * =d
 * Poll packets received by the Linux network interface named devname.
 * Packets will be pushed to output 0. The packets include the link-level
 * header.
 *
 * Linux won't see any packets from the device. If you want Linux to process
 * packets, you should hand them to ToLinux.
 *
 * This element can only be used with devices that support the Click polling
 * extension. We have written polling patches for the Tulip Ethernet driver.
 *
 * This element is only available inside the kernel module.
 *
 * =a FromDevice
 * =a ToDevice
 * =a FromLinux
 * =a ToLinux */

#include "element.hh"
#include "string.hh"
#include "glue.hh"
#include "netdev.h"

class PollDevice : public Element {
 public:
  
  PollDevice();
  PollDevice(const String &);
  ~PollDevice();
  
  static void static_initialize();
  static void static_cleanup();
  
  const char *class_name() const		{ return "PollDevice"; }
  const char *processing() const		{ return PUSH; }
  
  PollDevice *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);

  void run_scheduled();

#if _CLICK_STATS_
  // statistics
  unsigned long long _pkts_received;
  unsigned long long _idle_calls;
  unsigned long long _time_poll;
  unsigned long long _time_refill;
  unsigned long long _time_pushing;
  unsigned long long _perfcnt1_poll;
  unsigned long long _perfcnt1_refill;
  unsigned long long _perfcnt1_pushing;
  unsigned long long _perfcnt2_poll;
  unsigned long long _perfcnt2_refill;
  unsigned long long _perfcnt2_pushing;
#endif
  unsigned long _activations;
  int ifnum() 				{return _dev!=0 ? _dev->ifindex : -1;}

 private:
  struct device *_dev;
  String _devname;
  unsigned _registered;
  unsigned int _last_rx;
  unsigned _manage_tx;
};

#endif 

