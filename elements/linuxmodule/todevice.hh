#ifndef TODEVICE_HH
#define TODEVICE_HH

extern "C" {
#include <linux/netdevice.h>
}

/*
 * =c
 * ToDevice(devname)
 * =d
 * Sends packets out a Linux network interface device.
 *
 * Packets must have a link header. For ethernet, ToDevice
 * makes sure every packet is at least 60 bytes long.
 *
 * =n
 * The Linux networking code may also send packets out the device. Click won't
 * see those packets. Worse, Linux may cause the device to be busy when a
 * ToDevice wants to send a packet. Click is not clever enough to re-queue
 * such packets, and discards them. 
 *
 * ToDevice interacts with Linux in three possible modes: when Click is
 * running with its own scheduler thread, in polling mode; when Click is
 * running with its own scheduler thread, in interrupt mode; or when Click is
 * not running with its own scheduler thread (i.e. not running on top of a
 * polling capable kernel) and ToDevice is invoked by registered in/out
 * notifiers. In the last case, there are no race conditions because ToDevice
 * only send packets when it is notified by Linux to do so. Linux notifies
 * ToDevice in net_bh, which is not reentrant.
 *
 * In the first two cases, we depend on the net driver's send operation for
 * synchronization (e.g. tulip send operation uses a bit lock).
 *
 * This element is only available inside the kernel module.
 *
 * =a FromDevice
 * =a ToLinux
 */

#include "element.hh"
#include "string.hh"
#include "netdev.h"

class ToDevice : public Element {
  
 public:
  
  ToDevice();
  ToDevice(const String &);
  ~ToDevice();
  static void static_initialize();
  static void static_cleanup();
  
  const char *class_name() const	{ return "ToDevice"; }
  const char *processing() const	{ return PULL; }
  
  ToDevice *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  void run_scheduled();
  
  bool tx_intr();

#if _CLICK_STATS_
  // Statistics.
  unsigned long long _idle_calls;
  unsigned long long _idle_pulls;
  unsigned long long _busy_returns;
  unsigned long long _pkts_sent;    
  unsigned long long _time_pull;
  unsigned long long _time_clean;
  unsigned long long _time_queue;
  unsigned long long _perfcnt1_pull;
  unsigned long long _perfcnt1_clean;
  unsigned long long _perfcnt1_queue;
  unsigned long long _perfcnt2_pull;
  unsigned long long _perfcnt2_clean;
  unsigned long long _perfcnt2_queue;
#endif
  unsigned long _rejected;
  unsigned long _hard_start;
  unsigned long _activations; 

  int ifnum() 				{return _dev!=0 ? _dev->ifindex : -1;}
  int polling()				{return _polling;}
  
 private:

  int queue_packet(Packet *p);
  struct device *_dev;
  String _devname;
  unsigned _registered;
  unsigned _polling;
  int _dev_idle;
  int _last_dma_length;
  int _last_tx;
  int _last_busy;
};

#endif
