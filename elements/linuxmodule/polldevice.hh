#ifndef POLLDEVICE_HH
#define POLLDEVICE_HH

/*
 * =c
 * PollDevice(devname)
 * =d
 * 
 * Poll packets received by the Linux network interface named devname.
 * Packets can be pulled from output 0. The packets include the link-level
 * header.
 *
 * Linux won't see any packets from the device. If you want Linux to process
 * packets, you should hand them to ToLinux.
 *
 * This element is only available inside the kernel module.
 *
 * =a FromDevice
 * =a ToDevice
 * =a ToLinux
 */

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
  Processing default_processing() const		{ return PUSH; }
  
  PollDevice *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers(HandlerRegistry *);
  
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);

  void run_scheduled();

#if DEV_KEEP_STATS
  // statistics
  unsigned long long _l2misses_touch;
  unsigned long long _dcu_cycles_touch;
  unsigned long long _l2misses_rx;
  unsigned long long _dcu_cycles_rx;
  unsigned long long _l2misses_clean;
  unsigned long long _dcu_cycles_clean;
  unsigned long long _pkts_received;
  unsigned long long _activations;
  unsigned long long _idle_calls;
  unsigned long long _time_clean;
  unsigned long long _time_first_recv;
  unsigned long long _time_recv;
  unsigned long long _time_pushing;
  unsigned long long _time_running;
#endif
  
  struct device *_dev;
 private:
  String _devname;
  unsigned int _last_rx;
};

#endif 

