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
 
  // statistics
  unsigned long long _pkts_received;
  unsigned long long _pkts_on_dma;
  unsigned long long _activations;
  unsigned long long _tks_allocated;
  unsigned long long _dma_burst_resched;
  unsigned long long _dma_full_resched;
  unsigned long long _dma_empty_resched;
  unsigned long long _idle_calls;
  unsigned long long _dma_full;
  
 private:
  String _devname;
  struct device *_dev;
  int _last_dma_length;
};

#endif 

